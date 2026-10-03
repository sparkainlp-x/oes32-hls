"""Fixed-point C++ kernel (via pybind11) vs the double-precision golden model
``oes32_triage_ref.FloatTriage`` on seeded SYNTHETIC scenarios.

Pass criteria (README / docs/TRIAGE_STREAM_DESIGN.md §4): routing matches
exactly, and |tau_fixed - tau_golden| <= 16 LSB of tau_t (2^-10).
Random Hypothesis stimuli are deliberately NOT compared with the golden model:
near an exact tie |x|*tau == thr the two models may legitimately route
differently, and after that tau diverges. The bit-exact comparison with
FixedTriage covers random stimuli instead.
"""

import numpy as np

import oes32_triage_ref as ref
from triage_common import (GOLDEN_TOL, TAU_LSB, ext_step, import_ext, load_evidence_rows,
                           ref_step, row_call, seeded_scenarios)

ot = import_ext()


def golden_compare(calls):
    k, fl = ot.TriageKernel(), ref.FloatTriage()
    route_mm, max_err = 0, 0.0
    for call in calls:
        r1, t1 = ext_step(k, call)
        r2, t2 = ref_step(fl, call)
        route_mm += r1 != r2
        max_err = max(max_err, abs(t1 - t2))
    return route_mm, max_err


def test_golden_seeded_scenarios():
    route_mm, max_err = golden_compare(seeded_scenarios())
    assert route_mm == 0
    assert max_err <= GOLDEN_TOL, max_err / TAU_LSB


def test_golden_on_testbench_stimuli():
    route_mm, max_err = golden_compare([row_call(r) for r in load_evidence_rows()])
    assert route_mm == 0
    assert max_err <= GOLDEN_TOL
    assert abs(max_err - 4.272e-4) < 1e-6      # same 7-LSB maximum as the g++ testbench log


def test_golden_regime_change_signal():
    """Baseline, sustained overload, then baseline with moderate anomalies (notebook §4 signal)."""
    rng = np.random.default_rng(7)
    base = lambda k: rng.choice([-1, 1], k) * np.abs(0.5 + 0.08 * rng.standard_normal(k))
    x = np.concatenate([base(150), rng.choice([-1, 1], 120) * (1.6 + 0.05 * rng.standard_normal(120)),
                        base(300)])
    x[np.arange(290, len(x), 20)] = 1.25
    calls = [(float(v), False, 1.0, 1.0, 0.25, 4.0, False) for v in x]
    route_mm, max_err = golden_compare(calls)
    assert route_mm == 0
    assert max_err <= GOLDEN_TOL
