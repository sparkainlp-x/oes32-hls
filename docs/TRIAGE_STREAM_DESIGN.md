# Design note: `oes32_triage_accelerator` (streaming telemetry triage, v2)

- **Status:** Added in 0.2.0; kernel source unchanged in 0.3.0. g++ testbench with **SYNTHETIC** stimuli passes (19/19 checks). 0.3.0 (unreleased) adds Python bindings around the same C++ source and a pytest + Hypothesis suite (§8). Vitis HLS synthesis, C/RTL co-simulation, timing, II, latency and resource use: **UNRUN**.
- **Files:** `oes32_triage_stream.h`, `oes32_triage_stream.cpp`, `test_oes32_triage.cpp`, `tb/include_shim/hls_stream.h`, `run_hls_triage.tcl`, `Makefile`. Since 0.3.0 also: `python/src/oes32_triage_module.cpp`, `python/oes32_triage/`, `python/oes32_triage_ref.py`, `tests/test_triage_*.py`, `notebooks/oes32_triage_explainer.ipynb`, `CMakeLists.txt`, `pyproject.toml`.
- **Owner:** Jean-François Brisson / Spark AI NLP.
- **Relationship to ADR-001:** none normative. This kernel is an adaptive packet router for telemetry triage; it does not compute or replace the OES-32 residual R defined in `oes32-residual`.

## 1. Purpose

Split one AXI4-Stream of telemetry samples into two streams:

- `primaryStream`: nominal samples;
- `residualStream`: samples whose scaled magnitude exceeds a rate-limit threshold, plus every sample flagged `is_shock` upstream.

An adaptive gain `tau` makes the router more conservative after large excursions (tau decreases) and relaxes back toward a configured sensitivity during quiet periods.

## 2. Algorithm

Per packet `(x, is_shock)`, with registers `thr` (`rate_limit_threshold`), `s` (`tau_sensitivity`), `tau_min`, `tau_max`:

```
if first call or reset_tau:  tau = clamp(s, tau_min, tau_max)
w = |x| * tau
if w > thr or is_shock:
    route -> residualStream
    excess = (w > thr) ? (w - thr) : 0        # shock-only: excess = 0
    tau    = tau - excess / 16                 # (excess >> 4)
else:
    route -> primaryStream
    tau    = tau + (s - tau) / 16              # ((s - tau) >> 4)
tau = clamp(tau, tau_min, tau_max)
tau_out = tau
```

Properties (checked by the testbench, SYNTHETIC stimuli):

- tau never rises on the residual path; for shock-only packets it is unchanged.
- tau is within `[tau_min, tau_max]` after every call, including straight after a reset (the reload is clamped too).
- Negative samples are treated like positive samples of the same magnitude; `-2.0` (most negative `data_t`) is handled without overflow.
- On the primary path, tau converges to `s` geometrically (factor 15/16 per packet), down to a residual offset of at most 8 LSB of `tau_t` (see §4).

If `tau_min > tau_max` (misconfiguration), `tau_max` wins because it is applied last. `reset_tau` is a level register: while it is 1, tau is reloaded on every call, so adaptation is frozen. Software should write 1, then 0.

## 3. Interface

| Port | Direction | Interface | Type | Notes |
|---|---|---|---|---|
| `inputStream` | in | AXI4-Stream (`axis`), AGGREGATE | `telemetry_packet` | `{data_t data_value; ap_uint<1> is_shock;}` (19 payload bits) |
| `primaryStream` | out | AXI4-Stream, AGGREGATE | `telemetry_packet` | packet forwarded unchanged |
| `residualStream` | out | AXI4-Stream, AGGREGATE | `telemetry_packet` | packet forwarded unchanged |
| `rate_limit_threshold` | in | `s_axilite` `CONTROL_BUS` | `thr_t` = `ap_fixed<24,6,AP_RND,AP_SAT>` | raw = value × 2^18, two's complement |
| `tau_sensitivity` | in | `s_axilite` `CONTROL_BUS` | `tau_t` = `ap_fixed<18,4,AP_RND,AP_SAT>` | raw = value × 2^14 |
| `tau_min`, `tau_max` | in | `s_axilite` `CONTROL_BUS` | `tau_t` | clamp bounds |
| `reset_tau` | in | `s_axilite` `CONTROL_BUS` | `ap_uint<1>` | level-sensitive reload |
| `tau_out` | out | `s_axilite` `CONTROL_BUS` | `tau_t` | current tau (added in v2 for observability) |
| `return` | – | `ap_ctrl_none` | – | free-running, no start/done handshake |

Register **offsets** are assigned by Vitis HLS and appear in the generated driver header (`x<top>_hw.h`). Because synthesis is UNRUN, no offsets are published here, and none should be assumed.

`data_t = ap_fixed<18,2,AP_RND,AP_SAT>`: range [-2, 2 − 2^-16].

## 4. Fixed-point choices

| Type | Format | Range | LSB | Why |
|---|---|---|---|---|
| `data_t` | `ap_fixed<18,2>` RND/SAT | [-2, 2) | 2^-16 | specified by the user |
| `tau_t` | `ap_fixed<18,4>` RND/SAT | [-8, 8) | 2^-14 | gain up to 8; fits a 32-bit AXI-Lite word |
| `thr_t` | `ap_fixed<24,6>` RND/SAT | [-32, 32) | 2^-18 | covers every reachable `w` |
| `acc_t` | `ap_fixed<32,6>` RND/SAT | [-32, 32) | 2^-26 | internal; `|x|·tau < 16`, so nothing can wrap |

All arithmetic (magnitude, product, excess, shift, relaxation) is done in `acc_t`. The result is rounded once into `tau_t` after clamping. Because the clamp bounds are `tau_t` values, rounding cannot push tau outside them.

**Rounding dead zone.** The relaxation step `(s − tau)/16` rounds to zero once `|s − tau| < 8 LSB` of `tau_t` (2^-11 ≈ 4.9e-4). tau can therefore settle up to 8 LSB from `s`. This offset is symmetric and bounded, unlike v1's one-sided truncation. In the testbench the largest fixed-vs-golden tau error, 4.27e-4 (7 LSB), comes from exactly this effect. The stated tolerance is 16 LSB (2^-10 ≈ 9.77e-4). A fully dead-zone-free variant would need a minimum ±1 LSB step whenever `s ≠ tau`. That was not done, to stay close to the reference algorithm.

## 5. Execution model and the `read_nb` concern

The kernel is a **per-call** function. Each invocation does at most one non-blocking `read_nb`. With no packet it publishes `tau_out` and returns. The function-level protocol is `ap_ctrl_none`.

- **Concern:** under `ap_ctrl_none`, this relies on the generated block being re-invoked continuously (free-running). If the tool or the integration does not re-trigger the function every cycle, packets are only consumed once per invocation, and the throughput is whatever the invocation rate is. With `read_nb`, an empty input costs one invocation but does not stall. Whether Vitis HLS produces an II = 1 free-running core from this form is **UNRUN**/unknown until a synthesis report exists.
- **Why it is kept:** the per-call form makes the C testbench trivial. One call equals one packet (or one idle cycle), and it maps 1:1 onto the golden model.
- **Alternative (not implemented):** a free-running loop:

  ```cpp
  void oes32_triage_accelerator(...) {
      #pragma HLS INTERFACE mode=ap_ctrl_none port=return
      tau_t tau = ...;
      while (1) {
      #pragma HLS PIPELINE II=1
          if (reset_tau) tau = clamp(tau_sensitivity, tau_min, tau_max);
          telemetry_packet pkt;
          if (inputStream.read_nb(pkt)) { /* same body */ }
          tau_out = tau;
      }
  }
  ```

  This makes the streaming behaviour explicit, but it cannot be called from a C testbench without a bounded-loop build flag (for example `#ifdef __SYNTHESIS__ while(1) #else for (n packets) #endif`). The loop-carried dependency on `tau` (multiply → compare → subtract → clamp) may also prevent II = 1 at 100 MHz. Either option should be evaluated with a real synthesis report before any II or throughput figure is claimed.

## 6. v1 issues fixed in v2

| v1 issue (as reported by the author) | v2 fix |
|---|---|
| Wrapping overflow | all intermediates in `acc_t` with enough integer bits (`|x|·tau < 16`); `AP_SAT` on every type |
| Shock packets raising tau | excess clamped at 0 for shock-only packets, so tau is unchanged |
| Unbounded tau | clamp to `[tau_min, tau_max]` after every update and on reload |
| Negative samples never triggering | explicit magnitude computed in `acc_t`; `-2.0` handled without overflow |
| Truncation dead zone | shift in 26-fractional-bit `acc_t`, single `AP_RND` rounding into `tau_t`; residual offset ≤ 8 LSB, symmetric |
| Non-constant static initialiser | constant static init + `init` flag; first call / `reset_tau` loads tau |
| Misplaced `LOOP_TRIPCOUNT` | removed (the per-call body has no loop); the `while(1)` alternative is documented in §5 |

The v1 source is not in this repository. The left column lists the issues as the author described them when supplying v2. They were not re-derived here.

## 7. Verification

| What | Tag | Evidence |
|---|---|---|
| Fixed-point kernel vs double golden model, g++ | **SYNTHETIC** | `test_oes32_triage.cpp`; log `docs/evidence/triage_testbench_log.txt`; trace `docs/evidence/triage_tau_trace.csv`; CI job "Triage stream testbench" |
| C++ kernel (pybind11) vs bit-accurate Python model, bit-exact; invariants; golden tolerance | **SYNTHETIC** (seeded + Hypothesis) | `tests/test_triage_*.py`; CI job "Python bindings + pytest" on Python 3.10–3.13 (§8) |
| Vitis HLS C simulation / co-simulation | **UNRUN** | no Vitis run; the testbench uses a std::queue `hls_stream.h` shim |
| Synthesis, II, latency, timing at 100 MHz, resources | **UNRUN** / **TARGET** | `run_hls_triage.tcl` never executed |
| Board (ZCU111) | **UNRUN** | – |

Stimuli (seed 20261002, 32-bit LCG): first-call init on an empty stream; ±ramps 0→±1.9; noise |x| ≤ 0.2 with spikes of +1.8, −1.8, +1.99 and −2.0; a 32-packet shock-only burst; `reset_tau` held for two calls; a `tau_max` clamp (s = 6 > tau_max = 3); a `tau_min` clamp (repeated ±1.9 at thr = 0.5); and relaxation after spikes. In total: 1402 packets and 1411 calls.

Testbench limits: the golden model sees the same quantised `data_t` samples as the kernel (it checks the algorithm, not input quantisation). The smallest golden decision margin `|w − thr|` was 2.0e-3. Routing near an exact tie could differ between fixed point and double, and that case is not stressed. Running the testbench under UBSan reports shift-of-negative-value diagnostics inside the third-party `ap_private.h`. None of them come from this repository's code, and ASan is clean.

## 8. Python bindings and reference model

Added in 0.3.0 (unreleased). The kernel source is not modified.

**Build.** `CMakeLists.txt` and `pyproject.toml` (scikit-build-core + pybind11) compile `oes32_triage_stream.cpp` together with `python/src/oes32_triage_module.cpp` into `oes32_triage._core`. The build uses the pinned open-source `ap_*` headers and `tb/include_shim/hls_stream.h`, exactly as the g++ testbench does. CMake looks for the headers in this order:
1. `OES32_AP_TYPES_DIR`;
2. the Makefile's `.deps/`;
3. a download of the pinned commit tarball, verified against a recorded SHA-256 (`4b9af967…7092`).

The result is a C-simulation of the HLS source. It is not RTL, and none of the UNRUN tags change.

**Function-static state.** `adaptive_tau` and `init` are function-static, as in any HLS top function. One process (one loaded copy of the extension) therefore holds exactly one kernel state. The bindings do not hide this. They build per-object state on top of it using only the kernel's own register interface:

1. A fresh `TriageKernel` issues its first call with `reset_tau = 1`. The init path and the reset path execute the same statement (`adaptive_tau = clamp(tau_sensitivity, tau_min, tau_max)`), and the static initial value `tau = 1` is overwritten before it can be observed. So this is behaviourally identical to the kernel's first-ever call in a new process.
2. Each object saves `tau_out` (raw bits) after every call. If another object used the kernel since, it first makes one extra call on an empty input stream with `reset_tau = 1`, `tau_sensitivity = saved tau`, `tau_min = −8`, `tau_max = 8 − 2⁻¹⁴`. The clamp is a no-op, so tau is restored bit-exactly. Because the input is empty, `read_nb` fails and no packet is consumed or emitted. The binding checks the restored value and raises if it differs.
3. Calls hold the GIL, so access to the static state is serialised. `kernel_invocations()` reports every real kernel call, including restore calls. A test checks the exact count for an interleaved run.

This relies on one property of the kernel: tau and the init flag are its only state, and `reset_tau` can load any representable tau. A future kernel with more state (counters, for example) would need the same treatment for each extra variable, or an explicit state port.

**Reference model.** `python/oes32_triage_ref.py` (`FixedTriage`, `FloatTriage`, `q_from_float`, `requant`) was moved verbatim from the explainer notebook. It models each `ap_fixed` assignment with raw integers: round-half-up (`AP_RND`) and saturation (`AP_SAT`), with `>> 4` as an arithmetic (flooring) shift in `acc_t`. A known limitation, deliberately left unchanged: `q_from_float` overflows (`x·2^F → inf`) for |x| ≳ 1e303.

**What the tests establish** (all stimuli SYNTHETIC):
- *Bit-exactness:* C++ and `FixedTriage` agree on routing and raw `tau_t` bits for every call. This holds on seeded scenarios, on the 1411 recorded g++ testbench calls, and on Hypothesis-generated streams with random and mid-stream-changing registers, empty calls, resets, saturating samples, and `tau_min > tau_max`. The C++ and Python quantisers agree on arbitrary finite floats and on exact half-LSB ties.
- *Invariants (C++ only):*
  - tau stays in `[tau_min, tau_max]`, or equals `tau_max` when `tau_min > tau_max`;
  - exactly one output per packet, none for an empty call;
  - the payload is forwarded unchanged;
  - the residual path never raises tau, and a shock-only packet keeps it;
  - the primary path moves tau monotonically toward `clamp(tau_sensitivity)` without overshoot;
  - a reset reloads the clamped sensitivity;
  - `−2.0` and a saturating `+2.0` are handled correctly;
  - sign symmetry holds for every representable `x ≠ −2.0`.
- *Golden:* routing matches exactly and tau is within 16 LSB on seeded scenarios. Random stimuli are not compared with the golden model, because near an exact tie the double and fixed-point models may legitimately route differently.

Hypothesis uses a derandomised `ci` profile in CI (200 examples × 13 properties). The `thorough` profile (10 000 × 13) passed locally on Python 3.13. As a sanity check that the bit-exact tests actually detect errors, two deliberate one-line mutations of the reference model were each caught: truncating instead of rounding in `requant` failed 5 tests, and dropping the shock-only excess clamp failed 4. These mutations are not part of the repository.

**Speed (indicative only, build box):** `TriageKernel.run` processes about 3 × 10⁶ calls/s, about 15× the per-call Python reference model. This is a host CPU C-simulation figure. It says nothing about FPGA throughput, which is UNRUN.
