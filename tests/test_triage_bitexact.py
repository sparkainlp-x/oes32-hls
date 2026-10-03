"""Bit-exact agreement: real C++ kernel (pybind11 ``oes32_triage``) vs the
Python reference model ``oes32_triage_ref.FixedTriage``.

Routing and the raw tau_t bits must match on every call: on seeded SYNTHETIC
scenarios, on the recorded g++ testbench stimuli, and on Hypothesis-generated
random stimuli and register sequences.
"""

import numpy as np
from hypothesis import given, strategies as st

import oes32_triage_ref as ref
from triage_common import (DATA_MAX, DATA_MIN, FD, FH, FT, TAU_MAX_RAW, TAU_MIN_RAW,
                           THR_MAX_RAW, THR_MIN_RAW, ext_step, import_ext,
                           load_evidence_rows, ref_step, row_call, seeded_scenarios)

ot = import_ext()


def compare_calls(calls):
    k, fx = ot.TriageKernel(), ref.FixedTriage()
    for i, call in enumerate(calls):
        r_cpp, t_cpp = ext_step(k, call)
        r_py, t_py = ref_step(fx, call)
        assert r_cpp == r_py, (i, call)
        assert k.tau_raw == fx.tau, (i, call, k.tau_raw, fx.tau)
        assert t_cpp == t_py
    return len(calls)


# ---------------------------------------------------------------- quantisers
# The reference q_from_float overflows (x * 2^F -> inf) above ~1e303; it is kept
# unchanged, so the generic float domain is bounded to |x| <= 1e300. The C++
# saturation for huge magnitudes is checked separately below.
finite = st.floats(min_value=-1e300, max_value=1e300, allow_nan=False, allow_infinity=False)


def test_quantize_huge_magnitudes_saturate():
    for x in (1e300, 1.7e308):
        assert ot.quantize_data(x) == DATA_MAX and ot.quantize_data(-x) == DATA_MIN
        assert ot.quantize_tau(x) == TAU_MAX_RAW and ot.quantize_thr(-x) == THR_MIN_RAW


@given(st.one_of(st.floats(-3, 3), finite))
def test_quantize_data_matches_reference(x):
    assert ot.quantize_data(x) == ref.q_from_float(x, *ref.DATA)


@given(st.one_of(st.floats(-10, 10), finite))
def test_quantize_tau_matches_reference(x):
    assert ot.quantize_tau(x) == ref.q_from_float(x, *ref.TAU)


@given(st.one_of(st.floats(-40, 40), finite))
def test_quantize_thr_matches_reference(x):
    assert ot.quantize_thr(x) == ref.q_from_float(x, *ref.THR)


@given(st.integers(-(1 << 17), (1 << 17) - 1))
def test_quantize_ties_round_half_up(raw):
    # exact half-LSB ties: AP_RND rounds toward +inf
    x = (raw + 0.5) / (1 << FD)
    expected = min(raw + 1, DATA_MAX)
    assert ot.quantize_data(x) == expected == ref.q_from_float(x, *ref.DATA)


def test_non_finite_rejected():
    import pytest
    k = ot.TriageKernel()
    for bad in (float("nan"), float("inf"), -float("inf")):
        with pytest.raises(ValueError):
            k.step(bad)
        with pytest.raises(ValueError):
            k.step(0.1, tau_sensitivity=bad)
    with pytest.raises(ValueError):
        k.step_raw(DATA_MAX + 1, False, 1 << FT, 1 << FH, 0, 1 << FT)


# ---------------------------------------------------------- seeded scenarios
def test_seeded_scenarios_bit_exact():
    n = compare_calls(seeded_scenarios())
    assert n > 1000


def test_evidence_csv_replay_cpp_zero_mismatches():
    """Replays docs/evidence/triage_tau_trace.csv (v0.2.0 g++ testbench) through the
    pybind kernel: routing and tau must match the recorded C++ values exactly."""
    rows = load_evidence_rows()
    k = ot.TriageKernel()
    route_mm = tau_mm = 0
    for row in rows:
        route, tau = ext_step(k, row_call(row))
        route_mm += (route or "none") != row["route_fixed"]
        tau_mm += k.tau_raw != round(float(row["tau_fixed"]) * (1 << FT))
    assert len(rows) == 1411
    assert (route_mm, tau_mm) == (0, 0)


def test_run_vectorised_equals_step():
    calls = seeded_scenarios(seed=7)
    vals = np.array([c[0] if c[0] is not None else 0.0 for c in calls])
    cols = list(zip(*calls))
    out = ot.TriageKernel().run(
        vals, is_shock=np.array(cols[1]), rate_limit_threshold=np.array(cols[2]),
        tau_sensitivity=np.array(cols[3]), tau_min=np.array(cols[4]), tau_max=np.array(cols[5]),
        reset_tau=np.array(cols[6]), has_packet=np.array([c[0] is not None for c in calls]))
    k = ot.TriageKernel()
    for i, call in enumerate(calls):
        d = k.step_detail(call[0], call[1], call[3], call[2], call[4], call[5], call[6])
        code = {None: ot.ROUTE_NONE, "primary": ot.ROUTE_PRIMARY, "residual": ot.ROUTE_RESIDUAL}
        assert out["route"][i] == code[d["route"]]
        assert out["tau_raw"][i] == d["tau_raw"]
        assert out["tau"][i] == d["tau"]
    assert out["route"].dtype == np.int8 and out["tau_raw"].dtype == np.int32


def test_run_scalar_registers_and_validation():
    import pytest
    x = np.linspace(-1.9, 1.9, 501)
    a = ot.TriageKernel().run(x)
    fx = ref.FixedTriage()
    for i, v in enumerate(x):
        r, _ = fx.step(1.0, 1.0, 0.25, 4.0, False, value=float(v))
        assert ot.ROUTE_NAMES[int(a["route"][i])] == r and a["tau_raw"][i] == fx.tau
    with pytest.raises(ValueError):
        ot.TriageKernel().run(x, tau_min=np.zeros(3))
    with pytest.raises(ValueError):
        ot.TriageKernel().run(np.zeros((2, 2)))


# ------------------------------------------------- function-static state model
def test_interleaved_instances_are_independent():
    calls_a = seeded_scenarios(seed=1)[:400]
    calls_b = seeded_scenarios(seed=2)[200:600]
    ka, kb = ot.TriageKernel(), ot.TriageKernel()
    fa, fb = ref.FixedTriage(), ref.FixedTriage()
    before = ot.kernel_invocations()
    for ca, cb in zip(calls_a, calls_b):
        assert ext_step(ka, ca)[0] == ref_step(fa, ca)[0] and ka.tau_raw == fa.tau
        assert ext_step(kb, cb)[0] == ref_step(fb, cb)[0] and kb.tau_raw == fb.tau
    # every switch after both are initialised costs one restore call (empty stream)
    n = len(calls_a)
    assert ot.kernel_invocations() - before == 2 * n + (2 * n - 2)


def test_reset_method_gives_fresh_state():
    k = ot.TriageKernel()
    assert k.tau is None and not k.initialized
    for _ in range(10):
        k.step(1.9)
    assert k.tau < 1.0
    k.reset()
    assert k.tau_raw is None
    # behaves like a first-ever call even though reset_tau=0
    assert k.step(None, tau_sensitivity=2.5) == (None, 2.5)


def test_step_raw_matches_float_api():
    k1, k2 = ot.TriageKernel(), ot.TriageKernel()
    for v in np.linspace(-2, 2, 200):
        raw = ot.quantize_data(float(v))
        d1 = k1.step_raw(raw, False, 1 << FT, 1 << FH, 1 << (FT - 2), 4 << FT)
        r2, t2 = k2.step(float(v))
        assert d1["route"] == r2 and d1["tau"] == t2 and d1["out_value_raw"] == raw


# ------------------------------------------------------------- Hypothesis
reg_tau = st.one_of(st.integers(TAU_MIN_RAW, TAU_MAX_RAW), st.integers(0, 6 << FT))
reg_thr = st.one_of(st.integers(THR_MIN_RAW, THR_MAX_RAW), st.integers(0, 8 << FH))
sample = st.one_of(st.integers(DATA_MIN, DATA_MAX), st.sampled_from([DATA_MIN, DATA_MAX, 0, 1, -1]))


@st.composite
def raw_sequence(draw):
    regs = [draw(reg_thr), draw(reg_tau), draw(reg_tau), draw(reg_tau)]   # thr, sens, lo, hi
    seq = []
    for _ in range(draw(st.integers(1, 60))):
        if draw(st.integers(0, 9)) == 0:      # occasionally rewrite one register mid-stream
            j = draw(st.integers(0, 3))
            regs[j] = draw(reg_thr if j == 0 else reg_tau)
        value = draw(st.one_of(st.none(), sample, sample, sample))
        seq.append((value, draw(st.booleans()), *regs, draw(st.integers(0, 15)) == 0))
    return seq


@given(raw_sequence())
def test_hypothesis_random_streams_bit_exact(seq):
    k, fx = ot.TriageKernel(), ref.FixedTriage()
    for value_raw, shock, thr, sens, lo, hi, reset in seq:
        d = k.step_raw(value_raw, shock, sens, thr, lo, hi, reset)
        r_py, _ = fx.step(thr / (1 << FH), sens / (1 << FT), lo / (1 << FT), hi / (1 << FT),
                          reset, value=None if value_raw is None else value_raw / (1 << FD),
                          shock=shock)
        assert d["route"] == r_py
        assert d["tau_raw"] == fx.tau


@given(st.lists(st.tuples(st.one_of(st.none(), st.floats(-3, 3)), st.booleans()),
                min_size=1, max_size=80),
       st.floats(0, 6), st.floats(0, 5), st.floats(0, 2), st.floats(0, 6))
def test_hypothesis_float_api_bit_exact(stim, thr, sens, lo, hi):
    k, fx = ot.TriageKernel(), ref.FixedTriage()
    for value, shock in stim:
        r_cpp, t_cpp = k.step(value, shock, sens, thr, lo, hi, False)
        r_py, t_py = fx.step(thr, sens, lo, hi, False, value=value, shock=shock)
        assert (r_cpp, k.tau_raw) == (r_py, fx.tau)
