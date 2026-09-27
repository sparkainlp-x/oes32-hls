# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Fixed
- `examples/kuramoto_oes32_bridge.py`: `coherence_score` was always 0 because the entropy of `|ψ_i|²` is uniform for unit amplitudes ([#5](https://github.com/sparkainlp-x/oes32-hls/issues/5)). It now uses the normalized entropy of the phase histogram (`phase_entropy`). The old `probabilities` argument is accepted and ignored. Output remains SYNTHETIC.

### Added
- `tests/test_kuramoto_bridge.py` (pytest) and a CI job that runs it together with the example.

### Changed
- `.zenodo.json` adds the `spark-ai-nlp` Zenodo community.
- ADR-001: the "Engine/HLS aligned to A" bit is set to 1; removed the stale "PRs in flight" note.
- `CREATION_RECORD.md` moved to `docs/CREATION_RECORD.md`.
- CI actions bumped to `actions/checkout@v7` (Node 24).

## [0.1.1] - 2026-09-26

### Changed
- `CITATION.cff` version bump for the Zenodo archival release (DOI 10.5281/zenodo.22985526; concept DOI 10.5281/zenodo.22985525). Later: DOI badge and `.zenodo.json`.

## [0.1.0] - 2026-09-26

### Added
- Profile A HLS prototype (coherence floor, EVEN/ODD symmetry, FOLD8) with a g++ testbench; synthesis UNRUN, latency < 20 ns is a TARGET.
- Claim-hygiene pass: factual [`docs/CREATION_RECORD.md`](docs/CREATION_RECORD.md), MIT license, TARGET/UNRUN tags, placeholder CI job labelled as such; README skeleton, `CITATION.cff`.
