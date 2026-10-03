# Third-party components

| Component | Used for | How it is obtained | Licence | Redistributed here? |
|---|---|---|---|---|
| [Xilinx/HLS_arbitrary_Precision_Types](https://github.com/Xilinx/HLS_arbitrary_Precision_Types) (`ap_int.h`, `ap_fixed.h`, `etc/ap_private.h`, …) at commit `200a9aecaadf471592558540dc5a88256cbf880f` (2019-03-18) | g++ build of the streaming triage testbench | `make deps` / `make test-triage` downloads the pinned commit tarball into `.deps/` (git-ignored) | Apache License 2.0 (`LICENSE.TXT` in that repository) | **No.** Fetched at build time, not vendored |
| `tb/include_shim/hls_stream.h` | g++ testbench stand-in for `hls::stream` | Written for this repository (std::queue based) | AGPL-3.0-only (this repository) | Yes. Not AMD/Xilinx code; `#error` under `__SYNTHESIS__` |
| AMD Vitis HLS headers (`hls_stream.h`, `ap_*`) | Synthesis (UNRUN) | Provided by a local AMD Vitis installation | AMD tool licence | No |

Apache-2.0 is one-way compatible with GPLv3/AGPLv3, so a combined binary built from this repository and the fetched headers can be distributed under AGPL-3.0-only, provided the Apache-2.0 notice is kept with the headers.
