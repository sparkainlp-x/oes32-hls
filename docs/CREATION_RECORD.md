# Creation Record: oes32-hls

> **Status (rewritten 2026-09-26).** An earlier version of this file was drafted by an AI assistant (GitHub Copilot Chat) on 2026-08-28. It described pull request #2 as approved and merged, and listed files and results that are not on `main`. Those statements were incorrect and have been removed. This version states only what can be checked on GitHub. Evidence tags: SYNTHETIC / REPORTED / TARGET / UNRUN (see [oes32-residual CLAIM_HYGIENE](https://github.com/sparkainlp-x/oes32-residual)).

**Repository:** https://github.com/sparkainlp-x/oes32-hls
**Owner:** Jean-François Brisson, Spark AI NLP (GitHub: [@sparkainlp-x](https://github.com/sparkainlp-x))
**Contact:** open an issue in this repository.

---

## Current state of `main` (as of 2026-09-26)

| Item | State on `main` |
|---|---|
| HLS source, header, testbench, `run_hls.tcl`, CI workflow | Present |
| C++ testbench | 6 test cases / 8 checks, run with `g++` in CI |
| Vitis HLS synthesis | **UNRUN**. The CI synthesis job only echoes the command |
| Latency < 20 ns | **TARGET**, not measured |
| `LICENSE` file | **Not present on `main`** before the PR that adds this revision. The MIT license is added by that PR |
| `CONTRIBUTING.md`, `SECURITY.md`, `CHANGELOG.md`, `AUDIT_REPORT.md`, `PROJECT_MAP.md` | **Not present on `main`** |
| Code scanning (CodeQL) | **No claim.** No CodeQL result is published for this repository |

## Pull request #2

- **State: open, draft, not merged** (as of 2026-09-26).
- It proposes a LICENSE, documentation files, and an extended testbench (11 test cases / 17 checks). None of that content is on `main`. Test results quoted in PR #2 are **UNRUN** on `main` until the PR is reviewed and merged.

## Timeline

The commit list below is copied from the 2026-08-28 draft and has **not** been re-verified in this revision. Confirm it with `git log --reverse --format='%H %aI %s'` before relying on it.

| Date (UTC) | Event | Reference |
|---|---|---|
| 2026-08-28 03:15:57 | Repository created | `99abde1f98f01402026fedcd6954b187bdff5fb6` |
| 2026-08-28 03:26:52 | Added `run_hls.tcl` | `8ed935f390bad071f4891d1e213adb32c7f45e52` |
| 2026-08-28 03:29:48 | Added Vitis HLS GitHub Actions workflow | `a0ebea89f820d26bbde8bac55ec0a3a89be8266d` |
| 2026-08-28 03:35:03 | Initial planning commit | `50108f7a681ba5262cd011216ae8251b0e8acef7` |
| 2026-08-28 03:42:29 | Added HLS source, testbench, README | `81c760ed1845e18600b374173d862455fcd34f8c` |
| 2026-08-28 03:56:35 | Merged PR #1 (core implementation) | `e603b96ddf9a45f9fd2359030da54434e68c1d2c` |
| 2026-08-28 19:07:58 | PR #2 opened (draft) | PR #2, still open |

## Authorship

The source code and documentation are the work of the repository owner, with AI-assisted drafting where noted in commit or PR history. The authoritative record is the repository's git history, not this file.

## License

MIT. See [LICENSE](../LICENSE) once the licensing PR is merged. Copyright (c) 2026 Jean-François Brisson, Spark AI NLP.
