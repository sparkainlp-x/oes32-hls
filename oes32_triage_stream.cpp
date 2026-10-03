// =====================================================================
// OES-32 Telemetry Triage — Streaming HLS Kernel (v2)
// File:   oes32_triage_stream.cpp
// Top:    oes32_triage_accelerator
// Target: AMD Zynq UltraScale+ RFSoC ZCU111 (xczu28dr-ffvg1517-2-e) — TARGET
//
// Purpose:
//   Route each telemetry packet to primaryStream (nominal) or
//   residualStream (rate-limited / shock) using an adaptive gain tau:
//
//     w = |value| * tau
//     if (w > rate_limit_threshold || is_shock):
//         -> residual;  tau -= max(w - threshold, 0) / 16   (shock-only: 0)
//     else:
//         -> primary;   tau += (tau_sensitivity - tau) / 16
//     tau = clamp(tau, tau_min, tau_max)
//
// Evidence: g++ testbench (test_oes32_triage.cpp) with SYNTHETIC stimuli
// compares this kernel with a double-precision golden model. Vitis HLS
// synthesis, timing, II and resource use: UNRUN.
//
// Copyright (C) 2026 Jean-François Brisson, Spark AI NLP.
// SPDX-License-Identifier: AGPL-3.0-only
// =====================================================================

#include "oes32_triage_stream.h"

// Clamp helper, applied in the wide accumulator type before the final
// rounding into tau_t. If tau_min > tau_max (misconfiguration), tau_max wins.
static acc_t clamp_tau(acc_t v, tau_t lo, tau_t hi) {
#pragma HLS INLINE
    if (v < (acc_t)lo) v = lo;
    if (v > (acc_t)hi) v = hi;
    return v;
}

void oes32_triage_accelerator(
    hls::stream<telemetry_packet> &inputStream,
    hls::stream<telemetry_packet> &primaryStream,
    hls::stream<telemetry_packet> &residualStream,
    thr_t       rate_limit_threshold,
    tau_t       tau_sensitivity,
    tau_t       tau_min,
    tau_t       tau_max,
    ap_uint<1>  reset_tau,
    tau_t      &tau_out)
{
#pragma HLS INTERFACE mode=axis      port=inputStream
#pragma HLS INTERFACE mode=axis      port=primaryStream
#pragma HLS INTERFACE mode=axis      port=residualStream
#pragma HLS AGGREGATE variable=inputStream
#pragma HLS AGGREGATE variable=primaryStream
#pragma HLS AGGREGATE variable=residualStream
#pragma HLS INTERFACE mode=s_axilite port=rate_limit_threshold bundle=CONTROL_BUS
#pragma HLS INTERFACE mode=s_axilite port=tau_sensitivity      bundle=CONTROL_BUS
#pragma HLS INTERFACE mode=s_axilite port=tau_min              bundle=CONTROL_BUS
#pragma HLS INTERFACE mode=s_axilite port=tau_max              bundle=CONTROL_BUS
#pragma HLS INTERFACE mode=s_axilite port=reset_tau            bundle=CONTROL_BUS
#pragma HLS INTERFACE mode=s_axilite port=tau_out              bundle=CONTROL_BUS
#pragma HLS INTERFACE mode=ap_ctrl_none port=return

    // Constant static initialisers only (v1 used a non-constant one).
    // The first call, or reset_tau == 1, loads tau from tau_sensitivity,
    // clamped to [tau_min, tau_max] so tau is in bounds from the start.
    static tau_t      adaptive_tau = 1;
    static ap_uint<1> init         = 0;

    if (!init || reset_tau) {
        adaptive_tau = clamp_tau((acc_t)tau_sensitivity, tau_min, tau_max);
        init = 1;
    }

    telemetry_packet pkt;
    if (!inputStream.read_nb(pkt)) {
        // No packet this call: publish tau (reflects a reset) and return.
        // Under ap_ctrl_none this relies on continuous re-invocation.
        tau_out = adaptive_tau;
        return;
    }

    // |value|: computed in acc_t so -(-2.0) = +2.0 is exact (no wrap) and
    // negative samples are treated symmetrically with positive ones.
    acc_t mag = (pkt.data_value < 0) ? (acc_t)(-(acc_t)pkt.data_value)
                                     : (acc_t)pkt.data_value;

    acc_t w    = mag * (acc_t)adaptive_tau;        // |w| < 16: cannot wrap
    acc_t thr  = rate_limit_threshold;
    bool  over = (w > thr);

    acc_t next;
    if (over || pkt.is_shock) {
        residualStream.write(pkt);
        // Shock-only packets: excess clamped at 0, so tau never rises on
        // the residual path (v1 let shock packets raise tau).
        acc_t excess = over ? (acc_t)(w - thr) : (acc_t)0;
        next = (acc_t)adaptive_tau - (excess >> 4);
    } else {
        primaryStream.write(pkt);
        // Relax toward tau_sensitivity. The difference is formed and
        // shifted in acc_t (26 fractional bits vs 14 in tau_t), then
        // rounded once into tau_t (AP_RND): no one-sided truncation.
        acc_t delta = (acc_t)tau_sensitivity - (acc_t)adaptive_tau;
        next = (acc_t)adaptive_tau + (delta >> 4);
    }

    adaptive_tau = clamp_tau(next, tau_min, tau_max);
    tau_out      = adaptive_tau;
}
