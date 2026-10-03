"""Python bindings for the OES-32 streaming triage kernel ``oes32_triage_accelerator``.

``TriageKernel`` drives the real C++ kernel from ``oes32_triage_stream.cpp``.
The file is compiled unchanged into the ``_core`` extension, against the pinned
open-source Xilinx ``ap_*`` headers and the testbench-only ``hls_stream`` shim.
This is a C-simulation of the HLS source with g++/clang. It is **not**
synthesised RTL: FPGA synthesis, co-simulation and hardware are UNRUN.

One process holds one copy of the kernel's function-static state. Each
``TriageKernel`` object still behaves as an independent instance: its state is
saved after every call and restored through the kernel's own ``reset_tau``
register when another instance ran in between. See ``docs/TRIAGE_STREAM_DESIGN.md``.

Example
-------
>>> from oes32_triage import TriageKernel
>>> k = TriageKernel()
>>> k.step(0.1)                       # (route, tau_out)
('primary', 1.0)
>>> k.step(1.9, rate_limit_threshold=1.0)[0]
'residual'

The bit-accurate pure-Python reference model is the separate module
``oes32_triage_ref`` (``FixedTriage`` / ``FloatTriage``).

Copyright (C) 2026 Jean-François Brisson, Spark AI NLP.
SPDX-License-Identifier: AGPL-3.0-only
"""

from ._core import (  # noqa: F401
    DATA_FRAC,
    DATA_W,
    ROUTE_NONE,
    ROUTE_PRIMARY,
    ROUTE_RESIDUAL,
    TAU_FRAC,
    TAU_W,
    THR_FRAC,
    THR_W,
    TriageKernel,
    kernel_invocations,
    quantize_data,
    quantize_tau,
    quantize_thr,
)

__version__ = "0.3.0"

ROUTE_NAMES = {ROUTE_NONE: None, ROUTE_PRIMARY: "primary", ROUTE_RESIDUAL: "residual"}

__all__ = [
    "TriageKernel",
    "kernel_invocations",
    "quantize_data",
    "quantize_tau",
    "quantize_thr",
    "DATA_W", "DATA_FRAC", "TAU_W", "TAU_FRAC", "THR_W", "THR_FRAC",
    "ROUTE_NONE", "ROUTE_PRIMARY", "ROUTE_RESIDUAL", "ROUTE_NAMES",
    "__version__",
]
