# Project Map — sparkainlp-x Repositories

**Generated:** 2026-08-28  
**Scope:** Six public repositories on the sparkainlp-x GitHub profile

---

## Overview

The six repositories form a layered hardware/software stack for a research project involving an "OES-32 Membrane Shield" concept and companion quantum/LDPC decoder hardware. The hierarchy runs from Python simulation at the top to C++ HLS hardware accelerators at the bottom.

```
┌──────────────────────────────────────────────────────────┐
│          High-level simulation / research                │
│  oes32_engine          — Python OES-32 engine core       │
│  oes32-residual        — Python residual checker         │
│  quantum-error-correction-demo — QEC demo / benchmark    │
│  Cosmos-Holographic-Membrane-Simulation — placeholder    │
└────────────────────┬─────────────────────────────────────┘
                     │ feeds / inspired by
┌────────────────────▼─────────────────────────────────────┐
│          Hardware prototype (C++ HLS / FPGA)             │
│  oes32-hls       — HLS accelerator for ZCU111 RFSoC      │
│  qldpc_decoder_cpp — qLDPC decoder (C++, FPGA-targeted)  │
└──────────────────────────────────────────────────────────┘
```

---

## Repository Summaries

### 1. `oes32_engine`
- **Language:** Python
- **License:** MIT
- **Status:** Experimental
- **Purpose:** Core software reference implementation of the OES-32 symbolic membrane engine. Defines the mathematical contracts that the HLS accelerator (`oes32-hls`) implements in hardware.
- **Relation:** Software reference for `oes32-hls` and `oes32-residual`.

### 2. `oes32-residual`
- **Language:** Python
- **License:** (to be confirmed)
- **Status:** Experimental
- **Purpose:** Python module for computing residuals between proposed and reference OES-32 state vectors. Provides a software-only path for validating the coherence check logic implemented in `oes32-hls`.
- **Relation:** Software reference / test oracle for `oes32-hls` coherence floor.

### 3. `oes32-hls` (this repository)
- **Language:** C++ (Vitis HLS)
- **License:** MIT
- **Status:** Experimental / Engineering Prototype
- **Purpose:** High-Level Synthesis implementation of the OES-32 accelerator for the AMD Xilinx ZCU111 RFSoC. Implements coherence floor, bipartite symmetry, and FOLD8 ring-balance checks in hardware.
- **Relation:** Hardware implementation of the logic defined in `oes32_engine` / `oes32-residual`. Companion to `qldpc_decoder_cpp`.

### 4. `qldpc_decoder_cpp`
- **Language:** C++
- **License:** (to be confirmed)
- **Status:** Experimental / Engineering Prototype
- **Purpose:** Quasi-cyclic LDPC decoder targeting the same ZCU111 board. Intended to complement the OES-32 accelerator in a combined telemetry-and-error-correction pipeline.
- **Relation:** Companion hardware accelerator to `oes32-hls`.

### 5. `quantum-error-correction-demo`
- **Language:** Python
- **License:** MIT (LICENSE file present; README may contain conflicting claims — see audit)
- **Status:** Experimental / Research Demo
- **Purpose:** Classical-simulation demonstration of quantum error correction concepts. Provides illustrative benchmarks and metric definitions that motivate the hardware designs.
- **Relation:** Research motivation and metric definitions for `oes32-hls` and `qldpc_decoder_cpp`.
- **Note:** License/README contradiction requires resolution before publication.

### 6. `Cosmos-Holographic-Membrane-Simulation`
- **Language:** Unknown (README only at time of audit)
- **License:** Unknown
- **Status:** Empty placeholder
- **Purpose:** Title-only README; no source code, tests, or documentation present.
- **Relation:** Conceptually related to the membrane simulation theme but no concrete dependency.
- **Note:** Should be either implemented with minimal working code or archived with a clear placeholder notice.

---

## Dependency / Data-Flow Summary

```
quantum-error-correction-demo   →  motivates metric definitions
oes32_engine                    →  defines symbolic contracts
oes32-residual                  →  implements residual computation (Python)
        ↓                               ↓
oes32-hls  (hardware: coherence, symmetry, FOLD8 checks on ZCU111)
qldpc_decoder_cpp  (hardware: LDPC decoding on ZCU111)
```

---

## Shared Constraints

- All repositories: no synthesis/hardware validation results have been published.
- All repositories: experimental status only; not suitable for production or safety-critical use.
- License consistency: `oes32-hls` and `oes32_engine` are MIT; others require verification.
