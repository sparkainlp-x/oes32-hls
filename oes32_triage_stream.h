// =====================================================================
// OES-32 Telemetry Triage — Streaming HLS Kernel Header
// File:   oes32_triage_stream.h
// Top:    oes32_triage_accelerator
// Target: AMD Zynq UltraScale+ RFSoC ZCU111 (xczu28dr-ffvg1517-2-e) — TARGET
//
// Evidence: g++ testbench with SYNTHETIC stimuli only. Vitis HLS
// synthesis, timing, II and resource figures: UNRUN.
//
// Copyright (C) 2026 Jean-François Brisson, Spark AI NLP.
// SPDX-License-Identifier: AGPL-3.0-only
// =====================================================================

#ifndef OES32_TRIAGE_STREAM_H
#define OES32_TRIAGE_STREAM_H

#include <ap_int.h>
#include <ap_fixed.h>
#include <hls_stream.h>  // Vitis HLS header; under g++ the testbench uses
                         // tb/include_shim/hls_stream.h (testbench-only).

// --------------------------------------------------------------------
// Fixed-point types
// --------------------------------------------------------------------
// Sample: 18 bits, 2 integer bits (incl. sign) -> range [-2, 2 - 2^-16].
typedef ap_fixed<18, 2, AP_RND, AP_SAT> data_t;

// Adaptive tau: 18 bits, 4 integer bits -> range [-8, 8 - 2^-14],
// LSB = 2^-14 (~6.1e-5). Always clamped to [tau_min, tau_max].
typedef ap_fixed<18, 4, AP_RND, AP_SAT> tau_t;

// Threshold register: 24 bits, 6 integer bits -> range [-32, 32).
typedef ap_fixed<24, 6, AP_RND, AP_SAT> thr_t;

// Internal accumulator: 32 bits, 6 integer bits -> range [-32, 32),
// 26 fractional bits. |sample| < 2 and |tau| < 8, so |sample * tau| < 16
// and no intermediate can wrap; saturation is the only overflow mode.
typedef ap_fixed<32, 6, AP_RND, AP_SAT> acc_t;

// --------------------------------------------------------------------
// Stream packet (AXI4-Stream payload; AGGREGATE'd to one TDATA word)
// --------------------------------------------------------------------
struct telemetry_packet {
    data_t     data_value;  // signed telemetry sample
    ap_uint<1> is_shock;    // upstream shock flag: force residual routing
};

// --------------------------------------------------------------------
// Top-level function
//
// Per-call design: each invocation processes at most one packet
// (non-blocking read). Under ap_ctrl_none the core relies on being
// re-invoked continuously; see docs/TRIAGE_STREAM_DESIGN.md for the
// while(1) / PIPELINE II=1 alternative.
//
// Control registers (s_axilite, bundle CONTROL_BUS):
//   rate_limit_threshold  W  |value| * tau above this -> residual
//   tau_sensitivity       W  tau reset value and relaxation target
//   tau_min, tau_max      W  clamp bounds for tau
//   reset_tau             W  1 = reload tau from tau_sensitivity (level)
//   tau_out               R  current tau (observability; added in v2)
// --------------------------------------------------------------------
void oes32_triage_accelerator(
    hls::stream<telemetry_packet> &inputStream,
    hls::stream<telemetry_packet> &primaryStream,
    hls::stream<telemetry_packet> &residualStream,
    thr_t       rate_limit_threshold,
    tau_t       tau_sensitivity,
    tau_t       tau_min,
    tau_t       tau_max,
    ap_uint<1>  reset_tau,
    tau_t      &tau_out);

#endif // OES32_TRIAGE_STREAM_H
