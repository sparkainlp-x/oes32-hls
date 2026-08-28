// =====================================================================
// OES-32 Membrane Shield — HLS Accelerator Testbench
// File:   test_oes32_hls.cpp
//
// Builds and runs without Vitis HLS using a standard C++ compiler:
//
//   g++ -std=c++14 -o test_oes32_hls test_oes32_hls.cpp oes32_hls_top.cpp
//   ./test_oes32_hls
//
// Tests covered:
//   TC1  — Identical vectors: all three checks must pass.
//   TC2  — Large coherence violation: pass_coherence must be 0.
//   TC3  — EVEN/ODD imbalance: pass_symmetry must be 0.
//   TC4  — FOLD8 ring imbalance: pass_fold8 must be 0.
//   TC5  — Boundary value at exactly TAU: coherence must be 0.
//   TC6  — Residual just below TAU: coherence must be 1.
//   TC7  — All elements negative identical: all checks must pass.
//   TC8  — Non-zero proposed, zero reference, coherence fails.
//   TC9  — Negative symmetry imbalance (odd elements negative): symmetry fails.
//   TC10 — FOLD8 ring 3 imbalance: pass_fold8 must be 0.
//   TC11 — Uniform offset: all elements equal, coherence and fold8 pass,
//           symmetry depends on sum magnitude.
// =====================================================================

#include "oes32_hls_top.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>

// Simple test helpers ------------------------------------------------
static int tests_run    = 0;
static int tests_passed = 0;

static void check(const char *name, int expected, int actual) {
    tests_run++;
    if (expected == actual) {
        tests_passed++;
        printf("  PASS  %s (expected=%d got=%d)\n", name, expected, actual);
    } else {
        printf("  FAIL  %s (expected=%d got=%d)\n", name, expected, actual);
    }
}

// --------------------------------------------------------------------
// Macro to suppress HLS-specific pragmas when compiled outside Vitis
// (the stubs in oes32_hls_top.cpp compile cleanly with g++ because
//  #pragma HLS ... lines are treated as unknown pragmas and ignored).
// --------------------------------------------------------------------

// TC1 — Identical vectors -------------------------------------------
static void tc1_identical() {
    float p[N], r[N];
    for (int i = 0; i < N; i++) p[i] = r[i] = 0.0f;
    int coh = -1, sym = -1, f8 = -1;
    oes32_membrane_accelerator(p, r, &coh, &sym, &f8);
    printf("TC1 — identical zero vectors:\n");
    check("pass_coherence", 1, coh);
    check("pass_symmetry",  1, sym);
    check("pass_fold8",     1, f8);
}

// TC2 — Coherence floor violation -----------------------------------
static void tc2_coherence_fail() {
    float p[N], r[N];
    for (int i = 0; i < N; i++) p[i] = r[i] = 0.0f;
    // Introduce a residual > sqrt(TAU) so variance > TAU
    p[0] = 0.5f;   // diff=0.5, sq=0.25 > 0.09
    int coh = -1, sym = -1, f8 = -1;
    oes32_membrane_accelerator(p, r, &coh, &sym, &f8);
    printf("TC2 — coherence violation (p[0]=0.5, r[0]=0.0):\n");
    check("pass_coherence", 0, coh);
}

// TC3 — EVEN/ODD symmetry violation ---------------------------------
static void tc3_symmetry_fail() {
    float p[N], r[N];
    for (int i = 0; i < N; i++) p[i] = r[i] = 0.0f;
    // All even elements = 0.01, so sum_even = 16*0.01 = 0.16
    // |sum_even - sum_odd| = 0.16 > TAU (0.09) → fail
    for (int i = 0; i < N; i += 2) p[i] = 0.01f;
    int coh = -1, sym = -1, f8 = -1;
    oes32_membrane_accelerator(p, r, &coh, &sym, &f8);
    printf("TC3 — symmetry violation (even elements=0.01):\n");
    check("pass_symmetry", 0, sym);
}

// TC4 — FOLD8 ring imbalance ----------------------------------------
static void tc4_fold8_fail() {
    float p[N], r[N];
    for (int i = 0; i < N; i++) p[i] = r[i] = 0.0f;
    // Ring 0 = indices {0,4,8,12,16,20,24,28}; set each to 0.02
    // ring_sum = 8 * 0.02 = 0.16 > TAU → fail
    int ring0[8] = { 0, 4, 8, 12, 16, 20, 24, 28 };
    for (int k = 0; k < 8; k++) p[ring0[k]] = 0.02f;
    int coh = -1, sym = -1, f8 = -1;
    oes32_membrane_accelerator(p, r, &coh, &sym, &f8);
    printf("TC4 — FOLD8 ring-0 imbalance (each node=0.02):\n");
    check("pass_fold8", 0, f8);
}

// TC5 — Boundary: residual exactly at TAU → must fail ---------------
static void tc5_boundary_at_tau() {
    float p[N], r[N];
    for (int i = 0; i < N; i++) p[i] = r[i] = 0.0f;
    // diff = sqrt(TAU) → sq == TAU, condition is strict <, so fail
    p[0] = sqrtf(COHERENCE_TAU);
    int coh = -1, sym = -1, f8 = -1;
    oes32_membrane_accelerator(p, r, &coh, &sym, &f8);
    printf("TC5 — boundary residual == TAU:\n");
    check("pass_coherence", 0, coh);  // strict less-than
}

// TC6 — Coherence just below TAU → must pass ------------------------
static void tc6_just_below_tau() {
    float p[N], r[N];
    for (int i = 0; i < N; i++) p[i] = r[i] = 0.0f;
    // sq = TAU - epsilon < TAU → pass
    p[0] = sqrtf(COHERENCE_TAU - 0.001f);
    int coh = -1, sym = -1, f8 = -1;
    oes32_membrane_accelerator(p, r, &coh, &sym, &f8);
    printf("TC6 — residual just below TAU:\n");
    check("pass_coherence", 1, coh);
}

// TC7 — All elements negative, identical proposed and reference ------
static void tc7_negative_identical() {
    float p[N], r[N];
    for (int i = 0; i < N; i++) p[i] = r[i] = -0.001f;
    int coh = -1, sym = -1, f8 = -1;
    oes32_membrane_accelerator(p, r, &coh, &sym, &f8);
    printf("TC7 — negative identical vectors:\n");
    // residual = 0, coherence passes
    check("pass_coherence", 1, coh);
    // sum_even = sum_odd = 16 * (-0.001) = -0.016; diff = 0 < TAU, passes
    check("pass_symmetry",  1, sym);
    // each ring sum = 8 * (-0.001) = -0.008; |ring_sum| < TAU, passes
    check("pass_fold8",     1, f8);
}

// TC8 — Non-zero proposed, zero reference: coherence fails -----------
static void tc8_nonzero_vs_zero() {
    float p[N], r[N];
    for (int i = 0; i < N; i++) { p[i] = 0.4f; r[i] = 0.0f; }
    int coh = -1, sym = -1, f8 = -1;
    oes32_membrane_accelerator(p, r, &coh, &sym, &f8);
    printf("TC8 — all proposed=0.4, reference=0.0:\n");
    // max residual = 0.16 >= TAU(0.09) → fails
    check("pass_coherence", 0, coh);
}

// TC9 — Negative odd elements: symmetry fails -----------------------
static void tc9_negative_odd_symmetry() {
    float p[N], r[N];
    for (int i = 0; i < N; i++) p[i] = r[i] = 0.0f;
    // odd elements = -0.02: sum_odd = 16 * (-0.02) = -0.32
    // |sum_even - sum_odd| = 0.32 > TAU → fail
    for (int i = 1; i < N; i += 2) p[i] = -0.02f;
    int coh = -1, sym = -1, f8 = -1;
    oes32_membrane_accelerator(p, r, &coh, &sym, &f8);
    printf("TC9 — negative odd elements symmetry violation:\n");
    check("pass_symmetry", 0, sym);
}

// TC10 — FOLD8 ring 3 imbalance -------------------------------------
static void tc10_fold8_ring3_fail() {
    float p[N], r[N];
    for (int i = 0; i < N; i++) p[i] = r[i] = 0.0f;
    // Ring 3 = indices {3,7,11,15,19,23,27,31}; set each to 0.02
    // ring_sum = 8 * 0.02 = 0.16 >= TAU → fail
    int ring3[8] = { 3, 7, 11, 15, 19, 23, 27, 31 };
    for (int k = 0; k < 8; k++) p[ring3[k]] = 0.02f;
    int coh = -1, sym = -1, f8 = -1;
    oes32_membrane_accelerator(p, r, &coh, &sym, &f8);
    printf("TC10 — FOLD8 ring-3 imbalance (each node=0.02):\n");
    check("pass_fold8", 0, f8);
}

// TC11 — Uniform tiny offset: coherence and fold8/symmetry pass -----
static void tc11_uniform_tiny() {
    float p[N], r[N];
    // Both proposed and reference are small equal values; residual = 0
    for (int i = 0; i < N; i++) p[i] = r[i] = 0.001f;
    int coh = -1, sym = -1, f8 = -1;
    oes32_membrane_accelerator(p, r, &coh, &sym, &f8);
    printf("TC11 — uniform tiny value (0.001), identical p/r:\n");
    check("pass_coherence", 1, coh);
    // sum_even = sum_odd = 16 * 0.001 = 0.016; |diff| = 0 < TAU
    check("pass_symmetry",  1, sym);
    // each ring sum = 8 * 0.001 = 0.008 < TAU
    check("pass_fold8",     1, f8);
}

// --------------------------------------------------------------------
int main() {
    printf("OES-32 HLS Accelerator Testbench\n");
    printf("==================================\n\n");

    tc1_identical();
    printf("\n");
    tc2_coherence_fail();
    printf("\n");
    tc3_symmetry_fail();
    printf("\n");
    tc4_fold8_fail();
    printf("\n");
    tc5_boundary_at_tau();
    printf("\n");
    tc6_just_below_tau();
    printf("\n");
    tc7_negative_identical();
    printf("\n");
    tc8_nonzero_vs_zero();
    printf("\n");
    tc9_negative_odd_symmetry();
    printf("\n");
    tc10_fold8_ring3_fail();
    printf("\n");
    tc11_uniform_tiny();

    printf("\n==================================\n");
    printf("Results: %d / %d tests passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
