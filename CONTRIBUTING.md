# Contributing to oes32-hls

Thank you for your interest in contributing to this project.

## Scope

This repository is an **experimental engineering prototype**. Contributions that improve correctness, documentation, test coverage, or tooling compatibility are welcome. Do not add claims of hardware validation or synthesis results unless you have run the synthesis and can include the dated report files.

## How to Contribute

1. Fork the repository and create a feature branch from `main`.
2. Make your changes following the guidelines below.
3. Run the software testbench locally before opening a pull request:

   ```bash
   g++ -std=c++14 -Wall -Wextra -Wno-unknown-pragmas \
       -o test_oes32_hls test_oes32_hls.cpp oes32_hls_top.cpp
   ./test_oes32_hls
   ```

4. All assertions must pass (`Results: N / N tests passed`).
5. Open a pull request against `main` with a clear description of what changed and why.

## Adding Tests

- Add new test cases to `test_oes32_hls.cpp` as `static void tcNN_description()` functions.
- Update the test count comment at the top of the file.
- Document the expected behavior and the rationale for each new case.
- Deterministic test vectors are preferred over random inputs.

## Code Style

- C++14; no external dependencies beyond the standard library.
- Keep HLS pragmas inside `#ifdef __SYNTHESIS__` guards where practical so that the testbench compiles cleanly with `g++`.
- One logical change per commit; use present-tense commit messages.

## Reporting Issues

Use the GitHub Issues tracker. Include:
- The exact compiler version and OS.
- The full command you ran.
- The complete output.

## Synthesis Reports

If you run Vitis HLS synthesis, please include the dated report files (latency, resource, timing) in a `reports/` directory and reference them from your pull request description.

## Code of Conduct

Be respectful and constructive. This project follows the [Contributor Covenant](https://www.contributor-covenant.org/version/2/1/code_of_conduct/).
