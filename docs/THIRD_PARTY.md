# Third-party components

| Component | Used for | How it is obtained | Licence | Redistributed here? |
|---|---|---|---|---|
| [Xilinx/HLS_arbitrary_Precision_Types](https://github.com/Xilinx/HLS_arbitrary_Precision_Types) (`ap_int.h`, `ap_fixed.h`, `etc/ap_private.h`, …) at commit `200a9aecaadf471592558540dc5a88256cbf880f` (2019-03-18) | g++ build of the streaming triage testbench | `make deps` / `make test-triage` downloads the pinned commit tarball into `.deps/` (git-ignored). The Python build (`CMakeLists.txt`) reuses `.deps/` or downloads the same tarball, verified by SHA-256 `4b9af967feb55c5086f31d3545c914dd4c0de5026e5c8554f4450f4340727092` | Apache License 2.0 (`LICENSE.TXT` in that repository) | **No.** Fetched at build time, not vendored |
| `tb/include_shim/hls_stream.h` | g++ testbench stand-in for `hls::stream` | Written for this repository (std::queue based) | AGPL-3.0-only (this repository) | Yes. Not AMD/Xilinx code; `#error` under `__SYNTHESIS__` |
| [pybind11](https://github.com/pybind/pybind11) (≥ 2.13) | Python bindings (`oes32_triage._core`) | Build-time dependency from PyPI (`pyproject.toml` `build-system.requires`); header-only, compiled into the extension | BSD-3-Clause | No (not vendored; its headers are compiled into built wheels, whose distribution must keep the BSD notice) |
| [scikit-build-core](https://github.com/scikit-build/scikit-build-core) (≥ 0.10) | Build backend | PyPI, build time only | Apache-2.0 | No |
| [NumPy](https://numpy.org) (≥ 1.23) | Runtime dependency (arrays, reference model) | PyPI | BSD-3-Clause | No |
| [Hypothesis](https://hypothesis.works) (≥ 6.100) | Property-based tests (`[test]` extra) | PyPI, test time only | MPL-2.0 | No |
| [pytest](https://pytest.org) | Test runner (`[test]` extra) | PyPI, test time only | MIT | No |
| matplotlib, nbclient, nbformat, ipykernel | Notebook execution (`[notebook]` extra) | PyPI | matplotlib licence (PSF-based), BSD-3-Clause | No |
| AMD Vitis HLS headers (`hls_stream.h`, `ap_*`) | Synthesis (UNRUN) | Provided by a local AMD Vitis installation | AMD tool licence | No |

Apache-2.0 is one-way compatible with GPLv3/AGPLv3, so a combined binary built from this repository and the fetched headers can be distributed under AGPL-3.0-only, provided the Apache-2.0 notice is kept with the headers.

The Python extension compiles the Apache-2.0 `ap_*` headers and the BSD-3-Clause pybind11 headers into a binary distributed under AGPL-3.0-only. Both licences are compatible with this, provided their notices are kept with any binary wheel that is redistributed. A locally built wheel includes `LICENSE`, `COMMERCIAL-LICENSE.md` and this file (in `*.dist-info/licenses/`), plus the Apache-2.0 text of the `ap_*` headers (`oes32_triage/third_party_licenses/`). The pybind11 BSD-3-Clause notice is referenced here but not copied into the wheel. No wheels are published by this repository.
