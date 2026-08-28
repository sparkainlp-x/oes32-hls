# OES-32 HLS Accelerator

[![CI](https://github.com/sparkainlp-x/oes32-hls/actions/workflows/vitis-hls.yml/badge.svg)](https://github.com/sparkainlp-x/oes32-hls/actions/workflows/vitis-hls.yml)
[![Hardware](https://img.shields.io/badge/Hardware-AMD_Zynq_UltraScale%2B_RFSoC-blue?style=flat-square)](https://www.amd.com/en/products/adaptive-socs-and-fpgas/soc/zynq-ultrascale-plus-rfsoc.html)
[![Language](https://img.shields.io/badge/HLS-C%2B%2B14-informational?style=flat-square)](https://www.xilinx.com/products/design-tools/vitis/vitis-hls.html)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow?style=flat-square)](LICENSE)
[![Status: Experimental](https://img.shields.io/badge/Status-Experimental-orange?style=flat-square)](#project-status)

**Project status: Experimental / Engineering Prototype.**  
This repository contains a C++ High-Level Synthesis (HLS) source and testbench for a hardware accelerator kernel. It has **not** been validated on physical hardware. No synthesis timing or resource reports have been generated yet (see [Synthesis Reports](#synthesis-reports-pending)).

> ⚠️ **Safety notice:** This code is an engineering prototype for research purposes only. It is **not** certified for use in safety-critical, medical, life-support, or real-time control systems. See [SECURITY.md](SECURITY.md) for deployment guidance.

---

Hardware-accelerated telemetry triage for the **OES-32 Membrane Shield** — translated from Python software simulation to C++ High-Level Synthesis (HLS) for deployment on the AMD Xilinx Zynq UltraScale+ RFSoC **ZCU111** evaluation board.

---

## ⚙️ What This Repository Contains

| File | Purpose |
|------|---------|
| `oes32_hls_top.h` | Shared header — array size constant and function prototype |
| `oes32_hls_top.cpp` | Vitis HLS accelerator source — coherence, symmetry, FOLD8 checks |
| `test_oes32_hls.cpp` | C++ testbench — 11 test cases / 17 assertions, compiles with `g++`, no Vitis required |
| `run_hls.tcl` | Vitis HLS automation script — project creation, synthesis, IP export |
| `.github/workflows/vitis-hls.yml` | GitHub Actions CI — testbench compilation and execution |

---

## 🔬 Accelerator Architecture

The top-level function `oes32_membrane_accelerator` is exposed as an **AXI4-Lite slave** so the ARM PS cores can invoke it without DMA.  It applies three independent checks to a 32-element state vector:

### 1. Coherence Floor (`pass_coherence`)
Calculates the maximum squared residual between the proposed and reference state vectors.  
If `max(|proposed[i] − reference[i]|²) ≥ τ` the system latches and rejects the update.  
**Coherence threshold:** τ = 0.09

### 2. EVEN / ODD Bipartite Symmetry (`pass_symmetry`)
Checks that the sum of even-indexed elements ≈ sum of odd-indexed elements.  
A membrane with |Σ\_even − Σ\_odd| ≥ τ is considered geometrically imbalanced.

### 3. FOLD8 Cyclic-Ring Balance (`pass_fold8`)
Validates four independent 8-node cyclic groups (rings):

| Ring | Membrane Indices |
|------|-----------------|
| 0 | 0, 4, 8, 12, 16, 20, 24, 28 |
| 1 | 1, 5, 9, 13, 17, 21, 25, 29 |
| 2 | 2, 6, 10, 14, 18, 22, 26, 30 |
| 3 | 3, 7, 11, 15, 19, 23, 27, 31 |

Each ring sum must satisfy `|ring_sum| < τ`.

### Hardware Target (specification — not yet synthesized)

| Parameter | Value |
|-----------|-------|
| Part | xczu28dr-ffvg1517-2-e |
| Board | ZCU111 Evaluation Board |
| Clock | 100 MHz (10 ns period) |
| Interface | AXI4-Lite (s_axilite) |
| Latency **target** | < 20 ns (pipelined, II = 1) — *design intent, not a measured result* |

---

## 🚀 Quick Start

### Prerequisites

- C++14-compatible compiler (GCC ≥ 5, Clang ≥ 3.4)
- No Xilinx/AMD tools required for software testing

### Run Tests (no Xilinx tools required)

```bash
git clone https://github.com/sparkainlp-x/oes32-hls.git
cd oes32-hls
g++ -std=c++14 -Wall -Wextra -Wno-unknown-pragmas \
    -o test_oes32_hls test_oes32_hls.cpp oes32_hls_top.cpp
./test_oes32_hls
```

Expected output (last line):
```
Results: 17 / 17 tests passed
```

Test results verified on: GCC 11 / Ubuntu 22.04, 2026-08-28.

### Run Hardware Synthesis (Vitis HLS required)

> Requires AMD Vitis HLS 2022.1 or later. Not available on standard GitHub-hosted runners.

```bash
vitis_hls -f run_hls.tcl
```

Synthesis reports are written to `oes32_hls_proj/solution1/syn/report/`.  
The exported IP catalog is compatible with Vivado and the Vitis platform flow.

---

## 🔁 Continuous Integration

The GitHub Actions workflow (`.github/workflows/vitis-hls.yml`) runs on every push and pull request to `main`:

1. **Compile & Run Testbench** — builds with `g++`, executes all 11 test cases (17 assertions); job fails if any assertion fails.
2. **Synthesis stage** — echoes the synthesis command only; full Vitis HLS synthesis requires an AMD Vitis Docker image on a self-hosted runner.

---

## 📊 Synthesis Reports (Pending)

No synthesis has been run. The following metrics are **targets**, not measured results:

| Metric | Target | Status |
|--------|--------|--------|
| Latency (clock cycles) | ≤ 2 | Pending synthesis |
| Initiation interval (II) | 1 | Pending synthesis |
| LUTs | TBD | Pending synthesis |
| FFs | TBD | Pending synthesis |
| BRAM | 0 | Pending synthesis |
| DSPs | TBD | Pending synthesis |
| Timing slack | > 0 ns | Pending synthesis |
| Power | TBD | Pending synthesis |

To generate reports, run `vitis_hls -f run_hls.tcl` with Vitis HLS installed and inspect the output in `oes32_hls_proj/solution1/syn/report/`.

---

## ⚠️ Limitations

- All metrics are design targets based on architectural reasoning; no synthesis or place-and-route results have been obtained.
- The testbench exercises software-compiled behavior only. HLS pragma semantics (pipelining, unrolling) are not verified by `g++` compilation.
- `#pragma HLS INTERFACE s_axilite` with array ports has known area implications; verify resource usage after synthesis.
- No hardware-in-the-loop or board-level validation has been performed.
- Floating-point rounding differences between host and FPGA arithmetic are not tested.

---

## 🗂️ Related Repositories

- [sparkainlp-x/qldpc_decoder_cpp](https://github.com/sparkainlp-x/qldpc_decoder_cpp) — companion hardware-accelerated qLDPC decoder
- [sparkainlp-x/oes32-residual](https://github.com/sparkainlp-x/oes32-residual) — Python residual computation reference implementation
- [sparkainlp-x/oes32_engine](https://github.com/sparkainlp-x/oes32_engine) — Python OES-32 engine (software reference)

---

## 🏷️ Project Status

**Experimental** — Source compiles and all software tests pass. No synthesis, place-and-route, or hardware validation has been completed. Not suitable for production or safety-critical deployment.

---

## 📄 License

MIT — see [LICENSE](LICENSE).
