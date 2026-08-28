// =====================================================================
// OES-32 Membrane Shield — HLS Accelerator Header
// File:   oes32_hls_top.h
// =====================================================================

#ifndef OES32_HLS_TOP_H
#define OES32_HLS_TOP_H

// Array size constant
static constexpr int N = 32;

// Coherence tolerance (must match oes32_hls_top.cpp)
static constexpr float COHERENCE_TAU = 0.09f;

// Top-level function declaration
void oes32_membrane_accelerator(
    float proposed[N],
    float reference[N],
    int  *pass_coherence,
    int  *pass_symmetry,
    int  *pass_fold8);

#endif // OES32_HLS_TOP_H
