"""Pure-Python reference model (python/oes32_triage_ref.py): no extension needed.

Replays the stimuli recorded by the v0.2.0 g++ testbench against the real C++
kernel (docs/evidence/triage_tau_trace.csv, SYNTHETIC) through FixedTriage and
FloatTriage.
"""

import pytest

import oes32_triage_ref as ref
from triage_common import FT, load_evidence_rows, ref_step, row_call


def test_quantiser_edges():
    assert ref.q_from_float(-2.0, *ref.DATA) == -(1 << 17)      # representable
    assert ref.q_from_float(2.0, *ref.DATA) == (1 << 17) - 1    # saturates
    assert ref.q_from_float(0.5 / (1 << 16), *ref.DATA) == 1    # tie -> +inf
    assert ref.q_from_float(-0.5 / (1 << 16), *ref.DATA) == 0   # tie -> +inf
    assert ref.q_from_float(1e9, *ref.THR) == (1 << 23) - 1
    assert ref.q_from_float(-1e9, *ref.TAU) == -(1 << 17)


def test_requant_round_half_up_and_saturate():
    assert ref.requant(0b1000, 4, 18, 18) == 1      # 0.5 -> 1
    assert ref.requant(-0b1000, 4, 18, 18) == 0     # -0.5 -> 0
    assert ref.requant(-0b1001, 4, 18, 18) == -1
    assert ref.requant(1 << 40, 0, 18, 4) == (1 << 17) - 1


def test_fixed_model_replays_cpp_testbench_bit_exact():
    rows = load_evidence_rows()
    assert len(rows) == 1411
    fx = ref.FixedTriage()
    route_mm = tau_mm = 0
    for row in rows:
        route, _ = ref_step(fx, row_call(row))
        route_mm += (route or "none") != row["route_fixed"]
        tau_mm += fx.tau != round(float(row["tau_fixed"]) * (1 << FT))
    assert route_mm == 0
    assert tau_mm == 0


def test_float_model_replays_cpp_golden():
    rows = load_evidence_rows()
    fl = ref.FloatTriage()
    route_mm = 0
    max_err = 0.0
    for row in rows:
        route, tau = ref_step(fl, row_call(row))
        route_mm += (route or "none") != row["route_golden"]
        max_err = max(max_err, abs(tau - float(row["tau_golden"])))
    assert route_mm == 0
    assert max_err <= 1e-8  # CSV is printed with 8 decimals


def test_first_call_and_empty_call():
    fx = ref.FixedTriage()
    assert fx.step(1.0, 6.0, 0.25, 3.0, False) == (None, 3.0)   # init loads clamped sens
    assert fx.step(1.0, 1.0, 0.25, 3.0, False) == (None, 3.0)   # no packet: unchanged


@pytest.mark.parametrize("model", [ref.FixedTriage, ref.FloatTriage])
def test_shock_only_leaves_tau_unchanged(model):
    m = model()
    m.step(1.0, 1.0, 0.25, 4.0, False, value=1.9)
    tau0 = m.step(1.0, 1.0, 0.25, 4.0, False)[1]
    for i in range(16):
        r, t = m.step(1.0, 1.0, 0.25, 4.0, False, value=0.05 * (-1) ** i, shock=True)
        assert r == "residual" and t == tau0
