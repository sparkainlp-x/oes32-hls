"""Bit-accurate Python reference model of ``oes32_triage_accelerator``.

Pure-Python (NumPy only) re-implementation of the streaming triage kernel in
``oes32_triage_stream.cpp``:

* :class:`FixedTriage` copies the kernel's ``ap_fixed`` widths, ``AP_RND``
  rounding and ``AP_SAT`` saturation by holding every value as a raw
  two's-complement integer. Its ``step`` returns ``(route, tau)``, where
  ``tau`` is a float and the raw ``tau_t`` integer is in ``self.tau``
  (14 fractional bits).
* :class:`FloatTriage` is the double-precision golden model of the same
  algorithm. It sees the same quantised ``data_t`` sample as the kernel.

Moved verbatim from the explainer notebook (``notebooks/oes32_triage_explainer.ipynb``)
in 0.3.0. Only this docstring and comments were added; the behaviour is unchanged. The model
is checked bit-for-bit against the compiled C++ kernel by
``tests/test_triage_bitexact.py`` (C++ via the ``oes32_triage`` pybind11
extension).

Evidence: SYNTHETIC stimuli only. FPGA synthesis and hardware: UNRUN.

Copyright (C) 2026 Jean-François Brisson, Spark AI NLP.
SPDX-License-Identifier: AGPL-3.0-only
"""

__all__ = [
    "q_from_float", "requant", "FixedTriage", "FloatTriage",
    "DATA", "TAU", "THR", "ACC", "FD", "FT", "FH", "FA",
]

import numpy as np

# ap_fixed<W,I,AP_RND,AP_SAT> helpers (integer "raw" representation, frac bits F = W - I)
def q_from_float(x, W, I):
    F = W - I
    raw = int(np.floor(x * (1 << F) + 0.5))          # AP_RND: round half toward +inf
    lo, hi = -(1 << (W - 1)), (1 << (W - 1)) - 1      # AP_SAT
    return min(max(raw, lo), hi)

def requant(raw, F_from, W, I):
    """Re-quantise a raw integer with F_from fractional bits into ap_fixed<W,I,AP_RND,AP_SAT>."""
    F_to = W - I
    s = F_from - F_to
    if s > 0:
        raw = (raw + (1 << (s - 1))) >> s             # round half up (Python >> floors)
    elif s < 0:
        raw = raw << (-s)
    lo, hi = -(1 << (W - 1)), (1 << (W - 1)) - 1
    return min(max(raw, lo), hi)

DATA = (18, 2); TAU = (18, 4); THR = (24, 6); ACC = (32, 6)
FD, FT, FH, FA = 16, 14, 18, 26

class FixedTriage:
    """Bit-accurate model of oes32_triage_accelerator (static state = object state)."""
    def __init__(self):
        self.tau = 1 << FT      # static tau_t adaptive_tau = 1
        self.init = False
    def _clamp(self, v_acc, lo, hi):
        lo_a, hi_a = lo << (FA - FT), hi << (FA - FT)
        if v_acc < lo_a: v_acc = lo_a
        if v_acc > hi_a: v_acc = hi_a
        return v_acc
    def step(self, thr, sens, tmin, tmax, reset, value=None, shock=False):
        thr_r = q_from_float(thr, *THR); sens_r = q_from_float(sens, *TAU)
        lo_r = q_from_float(tmin, *TAU); hi_r = q_from_float(tmax, *TAU)
        if (not self.init) or reset:
            self.tau = requant(self._clamp(sens_r << (FA - FT), lo_r, hi_r), FA, *TAU)
            self.init = True
        if value is None:
            return None, self.tau / (1 << FT)
        d = q_from_float(value, *DATA)
        mag = requant(abs(d) << (FA - FD), FA, *ACC)
        w = requant(mag * (self.tau << (FA - FT)), 2 * FA, *ACC)
        thr_a = thr_r << (FA - FH)
        over = w > thr_a
        tau_a = self.tau << (FA - FT)
        if over or shock:
            route = 'residual'
            excess = requant(w - thr_a, FA, *ACC) if over else 0
            nxt = requant(tau_a - (excess >> 4), FA, *ACC)
        else:
            route = 'primary'
            delta = requant((sens_r << (FA - FT)) - tau_a, FA, *ACC)
            nxt = requant(tau_a + (delta >> 4), FA, *ACC)
        self.tau = requant(self._clamp(nxt, lo_r, hi_r), FA, *TAU)
        return route, self.tau / (1 << FT)

class FloatTriage:
    def __init__(self):
        self.tau = 1.0; self.init = False
    def step(self, thr, sens, tmin, tmax, reset, value=None, shock=False):
        cl = lambda v: min(max(v, tmin), tmax)
        if (not self.init) or reset:
            self.tau = cl(sens); self.init = True
        if value is None:
            return None, self.tau
        v = q_from_float(value, *DATA) / (1 << FD)   # golden sees the quantised sample
        w = abs(v) * self.tau
        over = w > thr
        if over or shock:
            nxt = self.tau - ((w - thr) if over else 0.0) / 16.0; route = 'residual'
        else:
            nxt = self.tau + (sens - self.tau) / 16.0; route = 'primary'
        self.tau = cl(nxt)
        return route, self.tau
