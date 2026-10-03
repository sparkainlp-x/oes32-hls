"""Helpers shared by the tests/test_triage_*.py modules (SYNTHETIC stimuli only)."""

import csv
import os
from pathlib import Path

import numpy as np
import pytest

REPO = Path(__file__).resolve().parents[1]
EVIDENCE_CSV = REPO / "docs" / "evidence" / "triage_tau_trace.csv"

FD, FT, FH = 16, 14, 18
DATA_MIN, DATA_MAX = -(1 << 17), (1 << 17) - 1
TAU_MIN_RAW, TAU_MAX_RAW = -(1 << 17), (1 << 17) - 1
THR_MIN_RAW, THR_MAX_RAW = -(1 << 23), (1 << 23) - 1
TAU_LSB = 2.0 ** -FT
GOLDEN_TOL = 16 * TAU_LSB  # documented tolerance (README / design note §4)

DEF = dict(thr=1.0, sens=1.0, tmin=0.25, tmax=4.0)


def import_ext():
    """Import the compiled extension.

    If it is missing, skip locally, but fail when OES32_REQUIRE_EXT=1 (CI).
    """
    try:
        import oes32_triage  # noqa: F401
        return oes32_triage
    except ImportError as exc:  # pragma: no cover - environment dependent
        if os.environ.get("OES32_REQUIRE_EXT") == "1":
            raise
        pytest.skip(f"oes32_triage extension not built: {exc}")


def load_evidence_rows():
    with open(EVIDENCE_CSV, newline="") as fh:
        return list(csv.DictReader(line for line in fh if not line.startswith("#")))


def row_call(row):
    """(value or None, shock, thr, sens, tmin, tmax, reset) from an evidence CSV row."""
    value = float(row["value"]) if row["has_packet"] == "1" else None
    return (value, row["is_shock"] == "1", float(row["rate_limit_threshold"]),
            float(row["tau_sensitivity"]), float(row["tau_min"]), float(row["tau_max"]),
            row["reset_tau"] == "1")


def clamp(v, lo, hi):
    if v < lo:
        v = lo
    if v > hi:
        v = hi
    return v


def seeded_scenarios(seed=20261002):
    """Deterministic SYNTHETIC call lists: ramps, +/- spikes (incl. -2.0 and a saturating
    +2.0), a shock-only burst, reset_tau, and tau_min / tau_max clamping.
    Each call is a tuple (value or None, shock, thr, sens, tmin, tmax, reset)."""
    rng = np.random.default_rng(seed)
    d = DEF
    calls = [(None, False, d["thr"], d["sens"], d["tmin"], d["tmax"], False)]
    for x in np.linspace(0, 1.9, 120):
        calls.append((float(x), False, d["thr"], d["sens"], d["tmin"], d["tmax"], False))
    for x in np.linspace(0, -1.9, 120):
        calls.append((float(x), False, d["thr"], d["sens"], d["tmin"], d["tmax"], False))
    for i in range(300):
        x = float(rng.uniform(-0.2, 0.2))
        if i % 25 == 12:
            x = [1.8, -1.8, 2.0, -2.0][(i // 25) % 4]
        calls.append((x, False, d["thr"], d["sens"], d["tmin"], d["tmax"], False))
    calls.append((None, False, d["thr"], d["sens"], d["tmin"], d["tmax"], True))
    for _ in range(4):
        calls.append((1.9, False, d["thr"], d["sens"], d["tmin"], d["tmax"], False))
    for i in range(32):
        calls.append((0.05 * (-1) ** i, True, d["thr"], d["sens"], d["tmin"], d["tmax"], False))
    calls.append((None, False, 10.0, 6.0, 0.25, 3.0, True))
    for _ in range(40):
        calls.append((0.3, False, 10.0, 6.0, 0.25, 3.0, False))
    calls.append((None, False, 0.5, 1.0, 0.5, 4.0, True))
    for i in range(80):
        calls.append(((1.9, -1.9)[i % 2], False, 0.5, 1.0, 0.5, 4.0, False))
    calls.append((None, False, d["thr"], d["sens"], d["tmin"], d["tmax"], True))
    for _ in range(8):
        calls.append((1.9, False, d["thr"], d["sens"], d["tmin"], d["tmax"], False))
    for _ in range(300):
        calls.append((0.05, False, d["thr"], d["sens"], d["tmin"], d["tmax"], False))
    return calls


def ref_step(model, call):
    value, shock, thr, sens, tmin, tmax, reset = call
    return model.step(thr, sens, tmin, tmax, reset, value=value, shock=shock)


def ext_step(kernel, call):
    value, shock, thr, sens, tmin, tmax, reset = call
    return kernel.step(value, is_shock=shock, tau_sensitivity=sens, rate_limit_threshold=thr,
                       tau_min=tmin, tau_max=tmax, reset_tau=reset)
