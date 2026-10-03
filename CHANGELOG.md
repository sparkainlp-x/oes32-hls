# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Documentation
- README: new "Related work" section linking multi-quantum-oes (concept DOI 10.5281/zenodo.23113851). Its Python `stream32` model follows the earlier v1 logic and differs from this kernel.
- `.zenodo.json` (takes effect at the next release): adds the keywords "anomaly detection", "reproducible research" and "telemetry", and a `references` related identifier for the multi-quantum-oes concept DOI. Zenodo's relation vocabulary has no `isRelatedTo`.

## [0.3.0] - 2026-10-02

The kernel source `oes32_triage_stream.cpp/.h` is unchanged. Synthesis, II, timing and board results remain **UNRUN** for every kernel. All new test stimuli are **SYNTHETIC**.

### Added
- **Python bindings** `oes32_triage` (pybind11 + scikit-build-core; `pyproject.toml`, `CMakeLists.txt`, `python/src/oes32_triage_module.cpp`). They compile the **unchanged** C++ kernel with the pinned `ap_*` headers (downloaded and SHA-256 verified, or reused from `.deps/`) and the testbench-only `hls_stream` shim.
  - `TriageKernel` provides `step`, `step_raw`, `step_detail`, a vectorised `run` (per-call register arrays, empty-call mask), `reset`, and `tau` / `tau_raw`.
  - Also exposed: the C++ quantisers `quantize_data/tau/thr` and `kernel_invocations()`.
  - Function-static kernel state is handled explicitly. A fresh object's first call uses `reset_tau`. When another object ran in between, the saved tau is restored bit-exactly through one empty-stream `reset_tau` call. One process holds one kernel state, and this is documented.
- **Reference model** `python/oes32_triage_ref.py` (`FixedTriage`, `FloatTriage`), moved verbatim from the explainer notebook with no behaviour change.
- **pytest + Hypothesis suite** (`tests/test_triage_reference.py`, `test_triage_bitexact.py`, `test_triage_invariants.py`, `test_triage_golden.py`; 36 tests, 13 Hypothesis properties):
  - C++ vs reference bit-exactness on routing and raw tau bits;
  - replay of `docs/evidence/triage_tau_trace.csv` with zero mismatches;
  - invariants (bounds, `tau_min > tau_max`, one output per packet, payload unchanged, shock-only keeps tau, monotone relaxation without overshoot, reset, ±2.0, sign symmetry);
  - golden tolerance (16 LSB).
  - Hypothesis uses a derandomised `ci` profile, plus `dev` and `thorough` profiles.
- **Notebook** `notebooks/oes32_triage_explainer.ipynb` (SYNTHETIC; the membrane framing is a metaphor only). It imports the reference module, adds an optional cross-check against the compiled C++ kernel, and is re-executed. `scripts/check_notebook.py` runs it in CI.
- **CI:** a "Python bindings + pytest" job on Python 3.10–3.13 and a "Notebook execution check" job. The Python example job now also runs the reference-model replay tests.
- `Makefile` targets `py-install`, `py-test`, `notebook`.
- Docs: README "Python bindings" section (install, example, API, test matrix); design note §8; `docs/THIRD_PARTY.md` entries for pybind11, scikit-build-core, NumPy, Hypothesis, pytest and the notebook dependencies.

### Changed
- `CITATION.cff` / `.zenodo.json`: version 0.3.0 and new keywords. `date-released` set to 2026-10-02.
- `.gitignore`: Python build artefacts, `.hypothesis/`, `.ipynb_checkpoints/`.

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
