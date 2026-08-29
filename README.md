# OES-32 HLS Accelerator

[![CI](https://github.com/sparkainlp-x/oes32-hls/actions/workflows/vitis-hls.yml/badge.svg)](https://github.com/sparkainlp-x/oes32-hls/actions/workflows/vitis-hls.yml)
[![Hardware](https://img.shields.io/badge/Hardware-AMD_Zynq_UltraScale%2B_RFSoC-blue?style=flat-square)](https://www.amd.com/en/products/adaptive-socs-and-fpgas/soc/zynq-ultrascale-plus-rfsoc.html)
[![Language](https://img.shields.io/badge/HLS-C%2B%2B14-informational?style=flat-square)](https://www.xilinx.com/products/design-tools/vitis/vitis-hls.html)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow?style=flat-square)](LICENSE)

Hardware-accelerated telemetry triage for the **OES-32 Membrane Shield** — translated from Python software simulation to C++ High-Level Synthesis (HLS) for deployment on the AMD Xilinx Zynq UltraScale+ RFSoC **ZCU111** evaluation board.

---

## ⚙️ What This Repository Contains

| File | Purpose |
|------|---------|
| `oes32_hls_top.h` | Shared header — array size constant and function prototype |
| `oes32_hls_top.cpp` | Vitis HLS accelerator source — modular coherence/symmetry/FOLD8 blocks |
| `test_oes32_hls.cpp` | C++ testbench — 6 test cases, compiles with `g++`, no Vitis required |
| `run_hls.tcl` | Vitis HLS automation script — project creation, synthesis, IP export |
| `.github/workflows/vitis-hls.yml` | GitHub Actions CI — file verification + testbench compilation |

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

Each ring sum must satisfy `|ring_sum| < τ`.  The full FOLD8 check is processed in **O(1) time** through spatial unrolling (`#pragma HLS UNROLL`).

### ⚡ Efficiency-oriented implementation details

- The top-level function is split into dedicated compute blocks for coherence, symmetry, and FOLD8 checks.
- EVEN/ODD symmetry uses pairwise accumulation (`i += 2`) to avoid per-iteration modulo/branch checks.
- Ring dimensions are compile-time constants (`FOLD8_RING_COUNT`, `FOLD8_RING_SIZE`) to keep loop structure synthesis-friendly.

### Hardware Target

| Parameter | Value |
|-----------|-------|
| Part | xczu28dr-ffvg1517-2-e |
| Board | ZCU111 Evaluation Board |
| Clock | 100 MHz (10 ns period) |
| Interface | AXI4-Lite (s_axilite) |
| Latency target | < 20 ns (pipelined, II = 1) |

---

## 🚀 Quick Start

### Run Tests (no Xilinx tools required)

```bash
git clone https://github.com/sparkainlp-x/oes32-hls.git
cd oes32-hls
g++ -std=c++14 -o test_oes32_hls test_oes32_hls.cpp oes32_hls_top.cpp
./test_oes32_hls
```

Expected output:
```
Results: 8 / 8 tests passed
```

### Run Hardware Synthesis (Vitis HLS required)

```bash
vitis_hls -f run_hls.tcl
```

Synthesis reports are written to `oes32_hls_proj/solution1/syn/report/`.  
The exported IP catalog is compatible with Vivado and the Vitis platform flow.

---

## 🔁 Continuous Integration

The GitHub Actions workflow (`.github/workflows/vitis-hls.yml`) runs on every push and pull request to `main`:

1. **Verify HLS Files** — confirms `oes32_hls_top.cpp` and `run_hls.tcl` are present.
2. **Compile & Run Testbench** — builds `test_oes32_hls.cpp` with `g++` and executes it; the job fails if any test case fails.
3. **Synthesis stage** — echoes the synthesis command (full Vitis HLS synthesis requires an AMD Vitis Docker image; see the workflow file for instructions to enable it).

---

## 🗂️ Related Repositories

- [sparkainlp-x/oes32-membrane-shield](https://github.com/sparkainlp-x/oes32-membrane-shield) — Python simulation and Monte Carlo benchmark
- [sparkainlp-x/qldpc_decoder_cpp](https://github.com/sparkainlp-x/qldpc_decoder_cpp) — companion hardware-accelerated qLDPC decoder

---

## 📄 License

MIT — see [LICENSE](LICENSE).
