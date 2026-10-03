"""Behavioural invariants of the real C++ kernel (via pybind11), checked on
Hypothesis-generated SYNTHETIC stimuli and registers, plus targeted edge cases.
These tests use only the extension (no reference model), so they check the
kernel's specification directly."""

from hypothesis import assume, event, given, strategies as st

from triage_common import (DATA_MAX, DATA_MIN, FD, FH, FT, TAU_MAX_RAW, TAU_MIN_RAW, clamp,
                           import_ext)

ot = import_ext()

reg_tau = st.integers(TAU_MIN_RAW, TAU_MAX_RAW)
pos_tau = st.integers(0, 6 << FT)
reg_thr = st.one_of(st.integers(-(1 << 23), (1 << 23) - 1), st.integers(0, 8 << FH))
sample = st.one_of(st.integers(DATA_MIN, DATA_MAX), st.integers(-(1 << FD), 1 << FD),
                   st.sampled_from([DATA_MIN, DATA_MAX, 0]))
calls = st.lists(st.tuples(st.one_of(st.none(), sample), st.booleans(),
                           st.integers(0, 19).map(lambda r: r == 0)), min_size=1, max_size=80)


@st.composite
def regs_ordered(draw):
    """(thr, sens, lo, hi) with lo <= hi, constant for the whole stream.
    Half of the draws use an operating-point range (thr 0.25..4, sens 0.25..4,
    lo 0..1, hi 1..6), so the primary and residual paths are both exercised often;
    the other half use the full register ranges."""
    if draw(st.booleans()):
        return (draw(st.integers(1 << (FH - 2), 4 << FH)), draw(st.integers(1 << (FT - 2), 4 << FT)),
                draw(st.integers(0, 1 << FT)), draw(st.integers(1 << FT, 6 << FT)))
    lo, hi = sorted([draw(st.one_of(reg_tau, pos_tau)), draw(st.one_of(reg_tau, pos_tau))])
    return draw(reg_thr), draw(st.one_of(reg_tau, pos_tau)), lo, hi


def drive(stream, regs):
    thr, sens, lo, hi = regs
    k = ot.TriageKernel()
    out = []
    for value, shock, reset in stream:
        prev = k.tau_raw
        d = k.step_raw(value, shock, sens, thr, lo, hi, reset)
        out.append((value, shock, reset, prev, d))
    return out


@given(calls, regs_ordered())
def test_tau_within_bounds_one_output_payload_unchanged(stream, regs):
    _, _, lo, hi = regs
    for value, shock, _reset, _prev, d in drive(stream, regs):
        assert lo <= d["tau_raw"] <= hi
        assert d["n_input_left"] == 0
        if value is None:
            assert d["route"] is None and d["n_primary"] + d["n_residual"] == 0
        else:
            assert d["n_primary"] + d["n_residual"] == 1
            assert d["route"] == ("residual" if d["n_residual"] else "primary")
            assert d["out_value_raw"] == value and d["out_is_shock"] == shock


@given(calls, reg_thr, reg_tau, st.integers(TAU_MIN_RAW + 1, TAU_MAX_RAW), st.data())
def test_tau_min_greater_than_tau_max_then_tau_max_wins(stream, thr, sens, lo, data):
    hi = data.draw(st.integers(TAU_MIN_RAW, lo - 1))
    for *_, d in drive(stream, (thr, sens, lo, hi)):
        assert d["tau_raw"] == hi


@given(calls, regs_ordered())
def test_residual_never_increases_tau_and_shock_only_keeps_it(stream, regs):
    thr, sens, lo, hi = regs
    for value, shock, reset, prev, d in drive(stream, regs):
        if prev is None or reset or value is None:
            continue
        if d["route"] == "residual":
            event("residual step checked")
            assert d["tau_raw"] <= prev
        if shock and thr >= 0 and abs(value) * hi < thr * (1 << (FD + FT - FH)) - (1 << 12):
            # |x| * tau_max is safely below thr, so the packet is shock-only: excess = 0
            event("shock-only step checked")
            assert d["tau_raw"] == prev


@given(calls, regs_ordered())
def test_primary_relaxes_monotonically_without_overshoot(stream, regs):
    thr, sens, lo, hi = regs
    target = clamp(sens, lo, hi)
    for value, _shock, reset, prev, d in drive(stream, regs):
        if prev is None or reset or d["route"] != "primary":
            continue
        t = d["tau_raw"]
        event("primary step checked" + (" (tau moved)" if t != prev else " (tau unchanged)"))
        assert min(prev, target) <= t <= max(prev, target)      # moves toward target, no overshoot
        assert abs(t - target) <= abs(prev - target)


def test_primary_converges_to_within_8_lsb_of_target():
    k = ot.TriageKernel()
    for _ in range(10):
        k.step(1.9)
    assert k.tau < 0.9
    for _ in range(400):
        k.step(0.05)
    assert abs(k.tau_raw - (1 << FT)) <= 8


@given(regs_ordered(), st.booleans(), st.lists(sample, max_size=20))
def test_reset_reloads_clamped_sensitivity(regs, with_packet_history, history):
    thr, sens, lo, hi = regs
    k = ot.TriageKernel()
    for v in history:
        k.step_raw(v, False, sens, thr, lo, hi, False)
    d = k.step_raw(None, False, sens, thr, lo, hi, True)
    assert d["tau_raw"] == clamp(sens, lo, hi)


def test_first_call_equals_reset():
    a, b = ot.TriageKernel(), ot.TriageKernel()
    for v in (0.3, 1.9, -1.2):
        a.step(v)
    assert a.step(None, tau_sensitivity=6.0, tau_max=3.0, reset_tau=True) == (None, 3.0)
    assert b.step(None, tau_sensitivity=6.0, tau_max=3.0) == (None, 3.0)


def test_extreme_inputs_minus_two_and_saturating_plus_two():
    k = ot.TriageKernel()
    d = k.step_detail(-2.0)
    assert d["out_value_raw"] == DATA_MIN and d["route"] == "residual"   # |-2.0| = 2.0 exactly
    d = k.step_detail(2.0)
    assert d["out_value_raw"] == DATA_MAX and d["route"] == "residual"   # saturates to 2 - 2^-16
    d = k.step_detail(5.0)
    assert d["out_value_raw"] == DATA_MAX
    # w = |x| * tau with |x| = 2.0 and tau near 8 stays < 16: no wrap, still residual
    k2 = ot.TriageKernel()
    d = k2.step_detail(-2.0, tau_sensitivity=7.99, tau_max=7.99, rate_limit_threshold=15.9)
    assert d["route"] == "residual"
    d = k2.step_detail(-2.0, tau_sensitivity=7.99, tau_max=7.99, rate_limit_threshold=16.0,
                       reset_tau=True)
    assert d["route"] == "primary"


@given(st.lists(st.tuples(st.one_of(st.none(), st.integers(DATA_MIN + 1, DATA_MAX)), st.booleans()),
                min_size=1, max_size=80), regs_ordered())
def test_sign_symmetry(stim, regs):
    thr, sens, lo, hi = regs
    kp, kn = ot.TriageKernel(), ot.TriageKernel()
    for v, shock in stim:
        dp = kp.step_raw(v, shock, sens, thr, lo, hi, False)
        dn = kn.step_raw(None if v is None else -v, shock, sens, thr, lo, hi, False)
        assert dp["route"] == dn["route"] and dp["tau_raw"] == dn["tau_raw"]


def test_minus_two_magnitude_exceeds_saturated_plus_two():
    # |-2.0| = 2.0 but +2.0 saturates to 2 - 2^-16: a threshold between them separates them
    thr_raw = (2 << FH) - 1                   # 2 - 2^-18
    k1, k2 = ot.TriageKernel(), ot.TriageKernel()
    r_neg = k1.step_raw(DATA_MIN, False, 1 << FT, thr_raw, 0, 4 << FT)["route"]
    r_pos = k2.step_raw(DATA_MAX, False, 1 << FT, thr_raw, 0, 4 << FT)["route"]
    assert (r_neg, r_pos) == ("residual", "primary")


@given(calls, regs_ordered())
def test_empty_calls_do_not_change_tau(stream, regs):
    thr, sens, lo, hi = regs
    assume(any(v is None for v, _, _ in stream))
    for value, _shock, reset, prev, d in drive(stream, regs):
        if value is None and not reset and prev is not None:
            assert d["tau_raw"] == prev
