# OES-32 HLS Prototype

Profile A sidecar to [oes32-residual](https://github.com/sparkainlp-x/oes32-residual): a C++ High-Level Synthesis (HLS) research prototype of OES-32 telemetry-triage checks, with a `g++` testbench. FPGA synthesis is **UNRUN**.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![CI](https://github.com/sparkainlp-x/oes32-hls/actions/workflows/vitis-hls.yml/badge.svg)](https://github.com/sparkainlp-x/oes32-hls/actions/workflows/vitis-hls.yml)
[![Status: research prototype](https://img.shields.io/badge/status-research%20prototype-orange.svg)](#what-it-is-not)
[![FPGA synthesis: UNRUN](https://img.shields.io/badge/FPGA%20synthesis-UNRUN-lightgrey.svg)](#evidence-tags)
[![ADR-001: Profile A sidecar](https://img.shields.io/badge/ADR--001-Profile%20A%20sidecar-blue.svg)](#relationship-to-adr-001)
[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.22985525.svg)](https://doi.org/10.5281/zenodo.22985525)

## What it is

- C++14 HLS source (`oes32_hls_top.cpp/.h`) implementing three checks on a 32-element vector: a coherence floor, EVEN/ODD symmetry, and FOLD8 ring balance, behind an AXI4-Lite interface declaration.
- A portable `g++` testbench (6 test cases, 8 checks) that runs in CI without any AMD/Xilinx tools.
- A Vitis HLS script (`run_hls.tcl`) targeting the AMD Zynq UltraScale+ RFSoC **ZCU111** board as a **TARGET** for co-design exploration.

## What it is NOT

- **Not** synthesized, placed, routed, or run on an FPGA. No synthesis or timing report exists.
- **Not** a QPU, a quantum device, or a quantum-hardware result. This is classical telemetry-triage IP exploration.
- **Not** production, certified, medical, or field hardware.
- **Not** the normative OES-32 residual. That is [oes32-residual@b77b612](https://github.com/sparkainlp-x/oes32-residual/tree/b77b61254f15778c6ae221843dceac7a8571158e) (ADR-001); see the documented differences below.

## What this repository contains

| File | Purpose |
|------|---------|
| `oes32_hls_top.h` | Shared header: array size constant, `COHERENCE_TAU`, function prototype |
| `oes32_hls_top.cpp` | HLS source: coherence, symmetry, FOLD8 checks |
| `test_oes32_hls.cpp` | C++ testbench: 6 test cases / 8 checks, compiles with `g++`, no Vitis required |
| `run_hls.tcl` | Vitis HLS automation script (project creation, synthesis, IP export). Not yet run |
| `examples/kuramoto_oes32_bridge.py` | SYNTHETIC classical Kuramoto-oscillator example (order parameter R and phase-histogram entropy) |
| `tests/test_kuramoto_bridge.py` | pytest checks for the Python example (run in CI) |
| `.github/workflows/vitis-hls.yml` | CI: testbench build + run; synthesis step is a placeholder (UNRUN) |
| `LICENSE` | MIT License |
| `CITATION.cff` | Citation metadata |

## Prototype architecture

The top-level function `oes32_membrane_accelerator` is declared as an **AXI4-Lite slave** (`s_axilite`) so that, once synthesized, the ARM PS cores could invoke it without DMA. It applies three independent checks to a 32-element vector.

### 1. Coherence floor (`pass_coherence`), Profile A sidecar

Computes the maximum **squared** residual between the proposed and reference vectors.
If `max(|proposed[i] − reference[i]|²) ≥ τ`, the check fails (`pass_coherence = 0`).
**Threshold:** τ = 0.09 (squared units; equivalent to |d| ≥ 0.3).

### 2. EVEN / ODD symmetry (`pass_symmetry`), Profile A sidecar

Checks that the sum of even-indexed elements ≈ the sum of odd-indexed elements.
Fails when `|Σ_even − Σ_odd| ≥ τ`.

### 3. FOLD8 ring balance (`pass_fold8`), Profile A sidecar

Checks four 8-node groups (strided rings):

| Ring | Indices |
|------|---------|
| 0 | 0, 4, 8, 12, 16, 20, 24, 28 |
| 1 | 1, 5, 9, 13, 17, 21, 25, 29 |
| 2 | 2, 6, 10, 14, 18, 22, 26, 30 |
| 3 | 3, 7, 11, 15, 19, 23, 27, 31 |

Each ring sum must satisfy `|ring_sum| < τ`. The loops carry `#pragma HLS UNROLL`; whether this yields constant-time hardware is **TARGET** until a synthesis report exists.

### Hardware target (all TARGET)

| Parameter | Value | Tag |
|-----------|-------|-----|
| Part | xczu28dr-ffvg1517-2-e | TARGET |
| Board | ZCU111 Evaluation Board | TARGET |
| Clock | 100 MHz (10 ns period) | TARGET |
| Interface | AXI4-Lite (s_axilite) | TARGET |
| Latency | < 20 ns (pipelined, II = 1) | **TARGET**: design intent, not measured; synthesis UNRUN |

## Quickstart

Run the testbench (no Xilinx tools required; any C++14 compiler):

```bash
git clone https://github.com/sparkainlp-x/oes32-hls.git
cd oes32-hls
g++ -std=c++14 -o test_oes32_hls test_oes32_hls.cpp oes32_hls_top.cpp
./test_oes32_hls
```

Expected output ends with:

```text
Results: 8 / 8 tests passed
```

HLS synthesis (requires AMD Vitis HLS; **UNRUN**, never performed):

```bash
vitis_hls -f run_hls.tcl
```

Reports would be written to `oes32_hls_proj/solution1/syn/report/`. No such report has been published. Until one is attached, every latency, clock, and resource figure stays **TARGET**.

## Tests

The testbench `test_oes32_hls.cpp` covers 6 test cases with 8 individual checks and exits non-zero if any check fails.

### Continuous integration

`.github/workflows/vitis-hls.yml` runs on every push and pull request to `main`:

1. **Build & Run C++ Testbench:** builds with `g++` and runs it. The job fails if any check fails.
2. **HLS synthesis (UNRUN — placeholder echo):** confirms the HLS files exist and **only echoes** the `vitis_hls` command. It does **not** synthesize anything. A green result means "files present", not "synthesis passed".

## Evidence tags

| Item | Tag | Notes |
|---|---|---|
| C++ testbench (`g++`) | **SYNTHETIC** inputs; runs in CI | 6 test cases, 8 checks; prints `Results: 8 / 8 tests passed` |
| Vitis HLS synthesis | **UNRUN** | The CI synthesis job is a placeholder that only echoes the command |
| Latency < 20 ns | **TARGET** | Design intent, not measured; no synthesis report exists |
| 100 MHz clock, II = 1 | **TARGET** | Set in `run_hls.tcl` / pragmas; not verified by a timing report |
| Board deployment on ZCU111 | **UNRUN** | No bitstream, no board run |
| `examples/kuramoto_oes32_bridge.py` output | **SYNTHETIC** | Classical example; `coherence_score` = R × (1 − phase entropy) |

Tag definitions: [sparkainlp-x/.github](https://github.com/sparkainlp-x/.github#evidence-tags).

### Known issues

- `examples/kuramoto_oes32_bridge.py`: fixed on `main` (unreleased). `coherence_score` used the entropy of `|ψ_i|²`, which is uniform for unit amplitudes, so it was always 0 ([#5](https://github.com/sparkainlp-x/oes32-hls/issues/5)). It now uses the entropy of the phase histogram, and a regression test covers it.

## Relationship to ADR-001

The normative residual **R** lives in [`oes32-residual`](https://github.com/sparkainlp-x/oes32-residual) (pin `b77b61254f15778c6ae221843dceac7a8571158e`). The checks here are **Profile A sidecars** and differ from the normative definition in two documented ways (see [docs/ADR-001-oes32-tau-unification.md](docs/ADR-001-oes32-tau-unification.md)):

- **Squared difference:** this prototype compares `|proposed[i] − reference[i]|²` to τ = 0.09, which is equivalent to an absolute-difference threshold of 0.3.
- **`≥` fail rule:** this prototype fails when the squared residual is `≥ τ` (equality fails). The normative contract fails only when `R > tolerance` (equality passes).

The OES-512 weighted latch (S = 0.45·Peak + 0.35·RMS + 0.20·MeanAbs, τ = 0.50) is a **TARGET** and is not implemented here.

Related repositories:

- [sparkainlp-x/oes32-residual](https://github.com/sparkainlp-x/oes32-residual): normative residual contract (ADR-001)
- [sparkainlp-x/oes32_engine](https://github.com/sparkainlp-x/oes32_engine): Profile A software sidecars (Python)
- [sparkainlp-x/qldpc_decoder_cpp](https://github.com/sparkainlp-x/qldpc_decoder_cpp): C++/HLS qLDPC decoder scaffold (hardware path UNRUN)

## Citation

Archived on Zenodo: concept DOI [10.5281/zenodo.22985525](https://doi.org/10.5281/zenodo.22985525) (all versions; resolves to the latest). The v0.1.1 archive is [10.5281/zenodo.22985526](https://doi.org/10.5281/zenodo.22985526).

Citation metadata is in [CITATION.cff](CITATION.cff); GitHub shows a "Cite this repository" button.

## License

[MIT](LICENSE). Copyright (c) 2026 Jean-François Brisson, Spark AI NLP.
