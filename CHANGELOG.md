# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

## [0.2.0] - 2026-10-02

Synthesis, II, timing and board results remain **UNRUN** for every kernel.

### Fixed
- `examples/kuramoto_oes32_bridge.py`: `coherence_score` was always 0 because the entropy of `|ψ_i|²` is uniform for unit amplitudes ([#5](https://github.com/sparkainlp-x/oes32-hls/issues/5)). It now uses the normalized entropy of the phase histogram (`phase_entropy`). The old `probabilities` argument is accepted and ignored. Output remains SYNTHETIC.

### Added
- Streaming AXI4-Stream telemetry-triage kernel `oes32_triage_accelerator` (`oes32_triage_stream.cpp/.h`, v2 algorithm). It routes packets to `primaryStream` / `residualStream` using an adaptive gain tau that is clamped to `[tau_min, tau_max]`, plus a level-sensitive `reset_tau`, a `tau_out` observability register, AXI4-Lite bundle `CONTROL_BUS`, and `ap_ctrl_none`. Fixes the reported v1 issues: wrapping overflow, shock packets raising tau, unbounded tau, negative samples never triggering, truncation dead zone, non-constant static init, and misplaced `LOOP_TRIPCOUNT`.
- `test_oes32_triage.cpp`: g++ testbench comparing the fixed-point kernel with a double-precision golden model on deterministic **SYNTHETIC** stimuli (seed 20261002): ramps, ± spikes including −2.0, a shock-only burst, `reset_tau`, and `tau_min` / `tau_max` clamping. Routing must match exactly, and tau must stay within 16 LSB (2^-10) of golden and inside its bounds. Result: 19 / 19 checks pass. Writes a tau CSV trace.
- `tb/include_shim/hls_stream.h`: testbench-only std::queue `hls::stream` stand-in (`#error` under `__SYNTHESIS__`).
- `Makefile` (`make test`, `make test-triage`): fetches the Apache-2.0 Xilinx `HLS_arbitrary_Precision_Types` headers at pinned commit `200a9ae` into `.deps/`. They are not vendored.
- `run_hls_triage.tcl` (part `xczu28dr-ffvg1517-2-e`, 10 ns). **UNRUN**.
- CI job "Triage stream testbench (g++, SYNTHETIC)", which uploads the tau trace as an artifact. The synthesis placeholder job now also lists the triage files and still only echoes commands (UNRUN).
- Docs: README section, `docs/TRIAGE_STREAM_DESIGN.md` (covers the `read_nb` + `ap_ctrl_none` re-invocation concern and the `while(1)`/`PIPELINE II=1` alternative), `docs/THIRD_PARTY.md`, and `docs/evidence/` (recorded log and CSV, SYNTHETIC).
- `tests/test_kuramoto_bridge.py` (pytest) and a CI job that runs it together with the example.

### Changed
- **Licence:** relicensed to AGPL-3.0-only with a commercial licensing option (`COMMERCIAL-LICENSE.md`). Versions published before 2026-09-29, including 0.1.0 and 0.1.1, remain available under MIT.
- Founder ORCID iD (0009-0000-9778-5374) added to citation metadata (#12).
- `.zenodo.json` adds the `spark-ai-nlp` Zenodo community.
- ADR-001: the "Engine/HLS aligned to A" bit is set to 1; removed the stale "PRs in flight" note.
- `CREATION_RECORD.md` moved to `docs/CREATION_RECORD.md`.
- CI actions bumped to `actions/checkout@v7` (Node 24).
- `CITATION.cff` / `.zenodo.json`: version 0.2.0, new keywords, updated description; `.zenodo.json` adds a `requires` relation to the `ap_*` header repository.

### Removed
- Committed Python bytecode (`examples/__pycache__/`, `tests/__pycache__/`); `.gitignore` now covers `__pycache__/`, `*.py[cod]`, `.pytest_cache/`, `.deps/` and the triage build outputs. It also un-ignores the top-level `Makefile`.

## [0.1.1] - 2026-09-26

### Changed
- `CITATION.cff` version bump for the Zenodo archival release (DOI 10.5281/zenodo.22985526; concept DOI 10.5281/zenodo.22985525). Later: DOI badge and `.zenodo.json`.

## [0.1.0] - 2026-09-26

### Added
- Profile A HLS prototype (coherence floor, EVEN/ODD symmetry, FOLD8) with a g++ testbench; synthesis UNRUN, latency < 20 ns is a TARGET.
- Claim-hygiene pass: factual [`docs/CREATION_RECORD.md`](docs/CREATION_RECORD.md), MIT license, TARGET/UNRUN tags, placeholder CI job labelled as such; README skeleton, `CITATION.cff`.
