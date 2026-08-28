// =====================================================================
// OES-32 Membrane Shield — Vitis HLS Accelerator
// File:   oes32_hls_top.cpp
// Target: AMD Xilinx Zynq UltraScale+ RFSoC ZCU111 (xczu28dr-ffvg1517-2-e)
//
// Purpose:
//   Hardware-accelerated telemetry triage for the 32-element OES membrane.
//   Implements the coherence-floor circuit breaker, bipartite symmetry check
//   (EVEN/ODD), and the interlaced FOLD8 cyclic-dependency validator.
//
//   The function is exposed as an AXI4-Lite slave so that the ARM PS can
//   invoke it without DMA: write the 32 proposed and 32 reference values,
//   start the accelerator, then read back the three status flags.
//
// Latency target: < 20 ns @ 100 MHz (fully pipelined, II = 1)
// =====================================================================

#include "oes32_hls_top.h"
#include <cmath>

// --------------------------------------------------------------------
// FOLD8 ring topology — four independent 8-node cyclic groups.
// Each row lists the eight membrane indices that share cyclic symmetry.
// --------------------------------------------------------------------
static const int FOLD8_RINGS[4][8] = {
    {  0,  4,  8, 12, 16, 20, 24, 28 },
    {  1,  5,  9, 13, 17, 21, 25, 29 },
    {  2,  6, 10, 14, 18, 22, 26, 30 },
    {  3,  7, 11, 15, 19, 23, 27, 31 }
};

// Coherence floor — use COHERENCE_TAU from the shared header.
// Aliased locally for readability inside this translation unit.
static constexpr float TAU = COHERENCE_TAU;

// --------------------------------------------------------------------
// oes32_membrane_accelerator
//
// proposed[N] — candidate state vector submitted for coherence check
// reference[N] — known-good reference state vector
// pass_coherence  — 1 if max residual variance < TAU, else 0
// pass_symmetry   — 1 if EVEN/ODD bipartite balance holds, else 0
// pass_fold8      — 1 if all four FOLD8 ring sums are balanced, else 0
// --------------------------------------------------------------------
void oes32_membrane_accelerator(
    float proposed[N],
    float reference[N],
    int  *pass_coherence,
    int  *pass_symmetry,
    int  *pass_fold8)
{
#pragma HLS INTERFACE s_axilite port=return        bundle=CTRL
#pragma HLS INTERFACE s_axilite port=proposed      bundle=CTRL
#pragma HLS INTERFACE s_axilite port=reference     bundle=CTRL
#pragma HLS INTERFACE s_axilite port=pass_coherence bundle=CTRL
#pragma HLS INTERFACE s_axilite port=pass_symmetry  bundle=CTRL
#pragma HLS INTERFACE s_axilite port=pass_fold8     bundle=CTRL

#pragma HLS ARRAY_PARTITION variable=proposed  complete dim=1
#pragma HLS ARRAY_PARTITION variable=reference complete dim=1

    // ----------------------------------------------------------------
    // 1. Coherence floor — compute max |proposed[i] - reference[i]|^2
    // ----------------------------------------------------------------
    float max_residual = 0.0f;
#ifdef __SYNTHESIS__
COHERENCE_LOOP:
#endif
    for (int i = 0; i < N; i++) {
#pragma HLS PIPELINE II=1
        float diff = proposed[i] - reference[i];
        float sq   = diff * diff;
        if (sq > max_residual) {
            max_residual = sq;
        }
    }
    *pass_coherence = (max_residual < TAU) ? 1 : 0;

    // ----------------------------------------------------------------
    // 2. EVEN / ODD bipartite symmetry
    //    Sum of even-indexed elements ≈ sum of odd-indexed elements
    //    (tolerance: same TAU, applied to |sum_even - sum_odd|)
    // ----------------------------------------------------------------
    float sum_even = 0.0f;
    float sum_odd  = 0.0f;
#ifdef __SYNTHESIS__
SYMMETRY_LOOP:
#endif
    for (int i = 0; i < N; i++) {
#pragma HLS PIPELINE II=1
        if (i % 2 == 0) {
            sum_even += proposed[i];
        } else {
            sum_odd += proposed[i];
        }
    }
    {
        float sym_diff = sum_even - sum_odd;
        if (sym_diff < 0.0f) sym_diff = -sym_diff;
        *pass_symmetry = (sym_diff < TAU) ? 1 : 0;
    }

    // ----------------------------------------------------------------
    // 3. FOLD8 cyclic-ring balance
    //    Each ring of 8 nodes must have a balanced sum (|ring_sum| < TAU)
    // ----------------------------------------------------------------
    int all_fold8_pass = 1;
#ifdef __SYNTHESIS__
FOLD8_OUTER:
#endif
    for (int r = 0; r < 4; r++) {
#pragma HLS PIPELINE
        float ring_sum = 0.0f;
#ifdef __SYNTHESIS__
FOLD8_INNER:
#endif
        for (int k = 0; k < 8; k++) {
#pragma HLS UNROLL
            ring_sum += proposed[FOLD8_RINGS[r][k]];
        }
        float abs_sum = (ring_sum < 0.0f) ? -ring_sum : ring_sum;
        if (abs_sum >= TAU) {
            all_fold8_pass = 0;
        }
    }
    *pass_fold8 = all_fold8_pass;
}
