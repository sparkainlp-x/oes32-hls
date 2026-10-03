# OES-32 HLS Prototype

Profile A sidecar to [oes32-residual](https://github.com/sparkainlp-x/oes32-residual): a C++ High-Level Synthesis (HLS) research prototype of OES-32 telemetry-triage checks, with a `g++` testbench. FPGA synthesis is **UNRUN**.

[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![CI](https://github.com/sparkainlp-x/oes32-hls/actions/workflows/vitis-hls.yml/badge.svg)](https://github.com/sparkainlp-x/oes32-hls/actions/workflows/vitis-hls.yml)
[![Status: research prototype](https://img.shields.io/badge/status-research%20prototype-orange.svg)](#what-it-is-not)
[![FPGA synthesis: UNRUN](https://img.shields.io/badge/FPGA%20synthesis-UNRUN-lightgrey.svg)](#evidence-tags)
[![ADR-001: Profile A sidecar](https://img.shields.io/badge/ADR--001-Profile%20A%20sidecar-blue.svg)](#relationship-to-adr-001)
[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.22985525.svg)](https://doi.org/10.5281/zenodo.22985525)

## What it is

- C++14 HLS source (`oes32_hls_top.cpp/.h`) implementing three checks on a 32-element vector: a coherence floor, EVEN/ODD symmetry, and FOLD8 ring balance, behind an AXI4-Lite interface declaration.
- A portable `g++` testbench (6 test cases, 8 checks) that runs in CI without any AMD/Xilinx tools.
- A Vitis HLS script (`run_hls.tcl`) targeting the AMD Zynq UltraScale+ RFSoC **ZCU111** board as a **TARGET** for co-design exploration.
- **New in 0.2.0:** a streaming AXI4-Stream telemetry-triage kernel (`oes32_triage_accelerator`) with an adaptive, clamped gain tau. It ships with a `g++` testbench that compares it against a double-precision golden model (**SYNTHETIC** stimuli, 19 checks). Synthesis **UNRUN**. See [Streaming triage kernel](#streaming-triage-kernel-oes32_triage_accelerator).

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
| `oes32_triage_stream.h` / `.cpp` | Streaming triage kernel `oes32_triage_accelerator` (AXI4-Stream + AXI4-Lite, `ap_fixed`) |
| `test_oes32_triage.cpp` | g++ testbench: kernel vs double golden model, SYNTHETIC seeded stimuli, 19 checks, writes a tau CSV trace |
| `tb/include_shim/hls_stream.h` | **Testbench-only** std::queue stand-in for `hls::stream` (g++ only; `#error` under `__SYNTHESIS__`) |
| `run_hls_triage.tcl` | Vitis HLS script for the triage kernel. Not yet run |
| `Makefile` | `make test` builds and runs both testbenches; fetches pinned Apache-2.0 `ap_*` headers into `.deps/` |
| `docs/TRIAGE_STREAM_DESIGN.md` | Design note: algorithm, fixed-point choices, `read_nb`/`ap_ctrl_none` concern, v1→v2 fixes |
| `docs/THIRD_PARTY.md` | Third-party components and licences |
| `docs/evidence/` | Recorded testbench log and tau trace CSV (SYNTHETIC) |
| `examples/kuramoto_oes32_bridge.py` | SYNTHETIC classical Kuramoto-oscillator example (order parameter R and phase-histogram entropy) |
| `tests/test_kuramoto_bridge.py` | pytest checks for the Python example (run in CI) |
| `.github/workflows/vitis-hls.yml` | CI: testbench build + run; synthesis step is a placeholder (UNRUN) |
| `LICENSE` | GNU AGPL-3.0-only license text (see also `COMMERCIAL-LICENSE.md`) |
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

## Streaming triage kernel (`oes32_triage_accelerator`)

A streaming telemetry-triage core. Each packet `{data_value, is_shock}` is routed to `primaryStream` (nominal) or `residualStream` (rate-limited or shock), using an adaptive gain tau. It is **not** the ADR-001 residual. Full design note: [docs/TRIAGE_STREAM_DESIGN.md](docs/TRIAGE_STREAM_DESIGN.md).

### Algorithm

```
first call or reset_tau:  tau = clamp(tau_sensitivity, tau_min, tau_max)
w = |data_value| * tau
if w > rate_limit_threshold or is_shock:
    -> residualStream;  tau -= max(w - rate_limit_threshold, 0) / 16   # >> 4; shock-only: unchanged
else:
    -> primaryStream;   tau += (tau_sensitivity - tau) / 16            # >> 4
tau = clamp(tau, tau_min, tau_max);  tau_out = tau
```

### Interface

| Port | Dir | Interface | Type |
|---|---|---|---|
| `inputStream` | in | AXI4-Stream, AGGREGATE | `telemetry_packet { data_t data_value; ap_uint<1> is_shock; }` |
| `primaryStream` | out | AXI4-Stream, AGGREGATE | `telemetry_packet` (unchanged) |
| `residualStream` | out | AXI4-Stream, AGGREGATE | `telemetry_packet` (unchanged) |
| control registers | in/out | AXI4-Lite `s_axilite`, bundle `CONTROL_BUS` | see register map |
| `return` | – | `ap_ctrl_none` (free-running, no start/done) | – |

`data_t = ap_fixed<18,2,AP_RND,AP_SAT>` (range [-2, 2)).

### Register map (`CONTROL_BUS`)

| Register | Access | Type | Encoding | Meaning |
|---|---|---|---|---|
| `rate_limit_threshold` | W | `ap_fixed<24,6,AP_RND,AP_SAT>` | raw = value × 2^18 | `|x|·tau` above this → residual |
| `tau_sensitivity` | W | `ap_fixed<18,4,AP_RND,AP_SAT>` | raw = value × 2^14 | tau reload value and relaxation target |
| `tau_min` | W | `ap_fixed<18,4,...>` | raw = value × 2^14 | lower clamp |
| `tau_max` | W | `ap_fixed<18,4,...>` | raw = value × 2^14 | upper clamp (wins if `tau_min > tau_max`) |
| `reset_tau` | W | `ap_uint<1>` | 1 = reload | level-sensitive: write 1, then 0 |
| `tau_out` | R | `ap_fixed<18,4,...>` | raw = value × 2^14 | current tau (observability) |

Byte offsets are generated by Vitis HLS (`x<top>_hw.h`). Synthesis is **UNRUN**, so no offsets are published.

### Verification status

| Item | Tag | Notes |
|---|---|---|
| g++ testbench vs double golden model | **SYNTHETIC**; runs in CI | 1402 packets / 1411 calls, seed 20261002; routing must match exactly; tau within 16 LSB (2^-10 ≈ 9.77e-4) of golden (max observed 4.27e-4); tau always in `[tau_min, tau_max]`; prints `Results: 19 / 19 checks passed` |
| Recorded run | **SYNTHETIC** | [docs/evidence/triage_testbench_log.txt](docs/evidence/triage_testbench_log.txt), [docs/evidence/triage_tau_trace.csv](docs/evidence/triage_tau_trace.csv) |
| Vitis HLS csim / cosim | **UNRUN** | the testbench uses a std::queue `hls_stream.h` shim, not the AMD header |
| Synthesis, II, latency, 100 MHz timing, resources | **UNRUN** / **TARGET** | `run_hls_triage.tcl` (part `xczu28dr-ffvg1517-2-e`, 10 ns clock) never executed |

### Limitations

- **Per-call design under `ap_ctrl_none`:** each invocation reads at most one packet with `read_nb` and returns if the input is empty. Throughput therefore depends on the core being re-invoked continuously. A `while(1)` + `PIPELINE II=1` form is the documented alternative ([design note §5](docs/TRIAGE_STREAM_DESIGN.md#5-execution-model-and-the-read_nb-concern)). Neither form has been synthesised.
- **Rounding dead zone:** after relaxation, tau can settle up to 8 LSB of `tau_t` (≈ 4.9e-4) away from `tau_sensitivity`.
- `reset_tau` is level-sensitive: while held at 1, adaptation is frozen.
- The testbench checks the algorithm on quantised inputs. It does not stress exact decision ties (smallest margin 2.0e-3), and it does not model AXI handshakes or back-pressure.
- The g++ build uses the 2019 open-source `ap_*` headers, which emit compiler warnings and UBSan shift diagnostics inside third-party code. They are suppressed for this target; see `Makefile` and [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md).

### How to run

```bash
make test-triage        # fetches pinned ap_* headers (Apache-2.0) into .deps/, builds, runs
# CSV trace: build/triage_tau_trace.csv
```

Expected output ends with:

```text
Results: 19 / 19 checks passed
```

HLS synthesis (requires AMD Vitis HLS; **UNRUN**): `vitis_hls -f run_hls_triage.tcl`.

## Tests

The testbench `test_oes32_hls.cpp` covers 6 test cases with 8 individual checks and exits non-zero if any check fails. `test_oes32_triage.cpp` covers the streaming kernel (8 scenarios, 19 checks). `make test` runs both.

### Continuous integration

`.github/workflows/vitis-hls.yml` runs on every push and pull request to `main`:

1. **Build & Run C++ Testbench:** builds with `g++` and runs it. The job fails if any check fails.
2. **Triage stream testbench (g++, SYNTHETIC):** `make test-triage`, then uploads the tau CSV trace as an artifact. This is not synthesis or co-simulation.
3. **Python example tests (SYNTHETIC):** pytest plus the Kuramoto example.
4. **HLS synthesis (UNRUN — placeholder echo):** confirms the HLS files for both kernels exist and **only echoes** the `vitis_hls` commands. It does **not** synthesize anything. A green result means "files present", not "synthesis passed".

## Evidence tags

| Item | Tag | Notes |
|---|---|---|
| C++ testbench (`g++`) | **SYNTHETIC** inputs; runs in CI | 6 test cases, 8 checks; prints `Results: 8 / 8 tests passed` |
| Vitis HLS synthesis | **UNRUN** | The CI synthesis job is a placeholder that only echoes the command |
| Latency < 20 ns | **TARGET** | Design intent, not measured; no synthesis report exists |
| 100 MHz clock, II = 1 | **TARGET** | Set in `run_hls.tcl` / pragmas; not verified by a timing report |
| Board deployment on ZCU111 | **UNRUN** | No bitstream, no board run |
| Triage stream testbench (`g++`) | **SYNTHETIC** inputs; runs in CI | 19 checks vs double golden model; prints `Results: 19 / 19 checks passed` |
| Triage stream synthesis / II / timing | **UNRUN** | `run_hls_triage.tcl` never executed |
| `examples/kuramoto_oes32_bridge.py` output | **SYNTHETIC** | Classical example; `coherence_score` = R × (1 − phase entropy) |

Tag definitions: [sparkainlp-x/.github](https://github.com/sparkainlp-x/.github#evidence-tags).

### Known issues

- `examples/kuramoto_oes32_bridge.py`: fixed in 0.2.0. `coherence_score` used the entropy of `|ψ_i|²`, which is uniform for unit amplitudes, so it was always 0 ([#5](https://github.com/sparkainlp-x/oes32-hls/issues/5)). It now uses the entropy of the phase histogram, and a regression test covers it.

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

Archived on Zenodo: concept DOI [10.5281/zenodo.22985525](https://doi.org/10.5281/zenodo.22985525) (all versions; resolves to the latest). Version DOIs: v0.2.0 (streaming triage kernel) is [10.5281/zenodo.23113675](https://doi.org/10.5281/zenodo.23113675); v0.1.1 is [10.5281/zenodo.22985526](https://doi.org/10.5281/zenodo.22985526).

Citation metadata is in [CITATION.cff](CITATION.cff); GitHub shows a "Cite this repository" button.

## License

This software is available under the GNU Affero General Public License v3.0 only (AGPL-3.0-only); see [LICENSE](LICENSE).

Organizations that want to use it in proprietary products or services without AGPL obligations can contact the author about a commercial license via https://sparkainlpx.xyz.

Versions published before 2026-09-29 were released under the MIT License and remain available under those terms.

Copyright (C) 2026 Jean-François Brisson, Spark AI NLP.
