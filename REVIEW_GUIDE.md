# Review guide

A short path for reviewers. Nothing here needs AMD/Xilinx tools.

## 1. What this is

A C++ HLS research prototype of OES-32 telemetry-triage checks, and a Profile A sidecar to the normative [oes32-residual@b77b612](https://github.com/sparkainlp-x/oes32-residual/tree/b77b61254f15778c6ae221843dceac7a8571158e) (ADR-001). It is tested in software only.

## 2. Status

| Layer | Status |
|---|---|
| g++ testbenches (membrane, triage vs golden model, edge cases), ASan + UBSan build | Runs in CI; **SYNTHETIC** stimuli |
| pybind11 C-simulation, pytest + Hypothesis, notebook | Runs in CI; **SYNTHETIC** stimuli |
| Vitis HLS csim / cosim | **UNRUN** |
| Synthesis, II, latency, timing, resources | **UNRUN**; figures are **TARGET** |
| Bitstream and board run (ZCU111) | **UNRUN** |

In CI, "HLS source files present (synthesis NOT RUN)" only checks that files exist. "Vitis HLS synthesis" is disabled and shows as skipped.

## 3. Prerequisites

- g++ (C++14) and `make`; `curl` and `sha256sum`; network access for the pinned `ap_*` headers on first build.
- For the Python bindings: Python 3.10–3.13 **with development headers** (`python3-dev` / `python3-devel`, or a python.org / `setup-python` / `uv` Python), a C++17 compiler.
- For `make sanitize`: a GCC or Clang with ASan/UBSan runtimes.

## 4. Five-minute check

```bash
make test        # expected: 8 / 8, 19 / 19 and 37/37
cmp build/triage_tau_trace.csv docs/evidence/triage_tau_trace.csv   # prints nothing
make sanitize    # optional; same three results under ASan + UBSan
```

`make deps` stops with `sha256sum: WARNING: 1 computed checksum did NOT match` if the downloaded header tarball differs from the pinned SHA-256.

## 5. Python check

```bash
python -m pip install ".[test,notebook]"
OES32_REQUIRE_EXT=1 python -m pytest -v tests     # expected: 41 passed
python examples/kuramoto_oes32_bridge.py
python scripts/check_notebook.py notebooks/oes32_triage_explainer.ipynb
```

## 6. What bit-exactness shows

The pybind11 module compiles the unchanged kernel source; the tests compare its routing and raw tau bits with a pure-Python fixed-point reference model, and its tau with a double-precision golden model (within 16 LSB). This shows that the C++ source behaves as specified when compiled with g++ and the open-source `ap_*` headers. It does not show that Vitis HLS would produce the same RTL behaviour, meet timing, or fit the device.

## 7. Recording a hardware run

Before any status above changes: run the Tcl scripts on a Vitis machine, keep the csim/cosim logs and the synthesis, timing and utilisation reports, record the tool version and part, attach them as artifacts or files in `docs/evidence/`, and only then update the label for that row.

## 8. Known limits

- `hls_stream.h` under g++ is a testbench-only `std::queue` shim; it does not model AXI handshakes or back-pressure.
- The 2019 open-source `ap_*` headers emit warnings and one UBSan shift diagnostic in third-party code (suppressed only in `tb/sanitize/ubsan.supp`).
- The membrane kernel does not reject NaN inputs: the coherence and FOLD8 flags still read 1. `tb/test_kernel_edges.cpp` prints this as a NOTE; the kernel is unchanged.
- See the README "Limitations" and "Evidence tags" sections for the rest.
