# Changelog

All notable changes to this project will be documented in this file.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).  
This project does not yet have a stable release; versioning will follow [Semantic Versioning](https://semver.org/) once a v1.0.0 is tagged.

---

## [Unreleased]

### Added
- `LICENSE` file (MIT) — was referenced in README but absent from the repository.
- `CONTRIBUTING.md` — contribution guidelines, test instructions, code style.
- `SECURITY.md` — vulnerability reporting procedure and deployment safety guidance.
- `CHANGELOG.md` — this file.
- `AUDIT_REPORT.md` — audit summary per project-wide review.
- `PROJECT_MAP.md` — relationship map across the six sparkainlp-x repositories.
- Extended testbench (`test_oes32_hls.cpp`): added TC7–TC11 covering negative identical
  vectors, non-zero vs zero reference, negative symmetry imbalance, FOLD8 ring-3 imbalance,
  and uniform tiny offset. Total: 11 test cases / 17 assertions (up from 6 / 8).
- README: clarified test case vs assertion count, added project status badge (Experimental),
  corrected latency target label to distinguish specification from measured result, added
  Synthesis Reports Pending table, Limitations section, and safety notice.

### Changed
- README: updated related repositories list to reflect actual existing repositories.
- README: replaced ambiguous "8 / 8 tests" language with explicit "11 test cases / 17 assertions".

### Fixed
- Missing LICENSE file (MIT was declared but file was absent).
