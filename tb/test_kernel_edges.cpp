// =====================================================================
// OES-32 HLS — edge-case g++ testbench (SYNTHETIC stimuli)
// File:   tb/test_kernel_edges.cpp
// DUTs:   oes32_membrane_accelerator (oes32_hls_top.cpp)
//         oes32_triage_accelerator   (oes32_triage_stream.cpp)
//
// Complements test_oes32_hls.cpp and test_oes32_triage.cpp with
// targeted edge cases:
//   membrane: strict tau boundaries for symmetry and FOLD8, sign
//             symmetry of the coherence check, all-zero input, call
//             independence (no state carried between calls), and the
//             current handling of NaN inputs (reported, see below);
//   triage:   empty input stream, stream ordering across the primary
//             and residual outputs, reset, the w == threshold boundary,
//             exact tau update arithmetic, and fixed-point rounding and
//             saturation of data_t and tau clamping.
//
// Software-only evidence (g++). Says nothing about Vitis HLS C/RTL
// co-simulation, synthesis, timing or hardware behaviour (UNRUN).
//
//   make test-edges
//
// Copyright (C) 2026 Jean-François Brisson, Spark AI NLP.
// SPDX-License-Identifier: AGPL-3.0-only
// =====================================================================

#include "oes32_hls_top.h"
#include "oes32_triage_stream.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

static int checks_run = 0;
static int checks_passed = 0;

static void check(const char *name, bool ok) {
    checks_run++;
    if (ok) checks_passed++;
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", name);
}

// ---------------------------------------------------------------------
// Membrane kernel helpers
// ---------------------------------------------------------------------
struct Flags { int coh, sym, fold; };

static Flags membrane(const float *p, const float *r) {
    float pp[N], rr[N];
    for (int i = 0; i < N; i++) { pp[i] = p[i]; rr[i] = r[i]; }
    Flags f = {-1, -1, -1};
    oes32_membrane_accelerator(pp, rr, &f.coh, &f.sym, &f.fold);
    return f;
}

static void membrane_tests() {
    std::printf("M1  all-zero input (no residual, balanced):\n");
    float z[N] = {0};
    Flags f = membrane(z, z);
    check("coherence, symmetry and FOLD8 all pass", f.coh == 1 && f.sym == 1 && f.fold == 1);

    std::printf("M2  strict boundary at TAU for symmetry and FOLD8:\n");
    float p[N] = {0};
    p[0] = COHERENCE_TAU;                                  // |sum_even - sum_odd| == TAU
    f = membrane(p, p);
    check("symmetry fails at exactly TAU (strict <)", f.sym == 0);
    check("FOLD8 fails at exactly TAU (>= TAU fails)", f.fold == 0);
    p[0] = std::nextafter(COHERENCE_TAU, 0.0f);            // one float ULP below TAU
    f = membrane(p, p);
    check("symmetry passes one ULP below TAU", f.sym == 1);
    check("FOLD8 passes one ULP below TAU", f.fold == 1);
    p[0] = -COHERENCE_TAU;
    f = membrane(p, p);
    check("negative sums use the absolute value (symmetry fails at -TAU)", f.sym == 0);
    check("negative ring sum fails FOLD8 at -TAU", f.fold == 0);

    std::printf("M3  coherence: squared residual against TAU, sign-symmetric:\n");
    float ref[N] = {0}, q[N] = {0};
    q[31] = 0.29f;                                          // 0.0841 < 0.09
    check("|diff| 0.29 passes", membrane(q, ref).coh == 1);
    q[31] = -0.29f;
    check("diff -0.29 passes (sign-symmetric)", membrane(q, ref).coh == 1);
    q[31] = 0.31f;                                          // 0.0961 >= 0.09
    check("|diff| 0.31 fails", membrane(q, ref).coh == 0);
    q[31] = -0.31f;
    check("diff -0.31 fails (sign-symmetric)", membrane(q, ref).coh == 0);
    float x = std::sqrt(COHERENCE_TAU);
    q[31] = x;
    check("result follows the strict float rule (x*x < TAU)", membrane(q, ref).coh == ((x * x < COHERENCE_TAU) ? 1 : 0));

    std::printf("M4  ring isolation (index 31 belongs only to ring 3):\n");
    float s[N] = {0};
    s[31] = 0.5f;
    s[27] = -0.5f;                                          // same ring, cancels
    f = membrane(s, s);
    check("balanced within ring 3 -> FOLD8 passes", f.fold == 1);
    s[27] = 0.0f; s[30] = -0.5f;                            // different ring: no cancellation
    f = membrane(s, s);
    check("imbalance in two rings -> FOLD8 fails", f.fold == 0);

    std::printf("M5  calls are independent (no state between calls):\n");
    float big[N];
    for (int i = 0; i < N; i++) big[i] = 1.0f;
    Flags a = membrane(z, z);
    (void)membrane(big, z);
    Flags b = membrane(z, z);
    check("same input gives the same flags after an unrelated call",
          a.coh == b.coh && a.sym == b.sym && a.fold == b.fold);

    std::printf("M6  NaN input (current behaviour, reported not enforced):\n");
    float n[N] = {0};
    n[0] = std::numeric_limits<float>::quiet_NaN();
    f = membrane(n, z);
    // Comparisons with NaN are false, so max_residual stays 0 and the
    // ring check never sees abs_sum >= TAU. Recorded as a known limit.
    std::printf("  NOTE  NaN -> coherence=%d symmetry=%d fold8=%d "
                "(NaN is not rejected; see REVIEW_GUIDE.md)\n", f.coh, f.sym, f.fold);
    check("symmetry flag rejects NaN (NaN < TAU is false)", f.sym == 0);
}

// ---------------------------------------------------------------------
// Triage kernel helpers
// ---------------------------------------------------------------------
static hls::stream<telemetry_packet> s_in("inputStream");
static hls::stream<telemetry_packet> s_pri("primaryStream");
static hls::stream<telemetry_packet> s_res("residualStream");

struct Regs { double thr, sens, tmin, tmax; };
static const Regs DEF = {1.0, 1.0, 0.25, 4.0};

static tau_t call(const Regs &r, bool reset) {
    tau_t out = -1;
    oes32_triage_accelerator(s_in, s_pri, s_res, (thr_t)r.thr, (tau_t)r.sens,
                             (tau_t)r.tmin, (tau_t)r.tmax, (ap_uint<1>)(reset ? 1 : 0), out);
    return out;
}

static void push(double v, bool shock = false) {
    telemetry_packet p;
    p.data_value = v;
    p.is_shock = shock ? 1 : 0;
    s_in.write(p);
}

static void drain() {
    telemetry_packet junk;
    while (s_in.read_nb(junk)) {}
    while (s_pri.read_nb(junk)) {}
    while (s_res.read_nb(junk)) {}
}

static void triage_tests() {
    const double LSB = 1.0 / 65536.0;   // data_t LSB = 2^-16

    std::printf("T1  empty input stream:\n");
    drain();
    tau_t t0 = call(DEF, true);
    tau_t t1 = call(DEF, false);
    tau_t t2 = call(DEF, false);
    check("no packets emitted on empty calls", s_pri.empty() && s_res.empty());
    check("tau unchanged across empty calls", t0 == t1 && t1 == t2);

    std::printf("T2  stream ordering across both outputs:\n");
    drain();
    call(DEF, true);
    // Distinct values; |v| > 1 goes to residual at tau=1, thr=1.
    const double vals[] = {0.10, 1.50, 0.20, -1.25, 0.30, 0.40, 1.75, -0.50, 0.05, 1.10};
    const int n = sizeof(vals) / sizeof(vals[0]);
    for (int i = 0; i < n; i++) push(vals[i]);
    std::vector<int> route;    // 0 = primary, 1 = residual, per call
    for (int i = 0; i < n; i++) {
        call(DEF, false);
        route.push_back(s_res.size() > 0 ? 1 : 0);
        telemetry_packet o;
        bool got = s_res.read_nb(o) || s_pri.read_nb(o);
        if (!got || o.data_value != (data_t)vals[i]) { route.back() = -1; }
    }
    bool ordered = true;
    for (int r : route) if (r < 0) ordered = false;
    check("each call forwards exactly the next input packet, unchanged", ordered);
    check("input stream fully consumed", s_in.empty());
    tau_t te = call(DEF, false);
    check("extra call after the stream drains emits nothing", s_pri.empty() && s_res.empty());
    (void)te;

    std::printf("T3  w == threshold boundary (strict >):\n");
    drain();
    call(DEF, true);                    // tau = 1
    Regs half = {0.5, 1.0, 0.25, 4.0};
    push(0.5);
    call(half, false);
    check("|v|*tau == thr routes to primary", s_pri.size() == 1 && s_res.empty());
    drain();
    call(half, true);
    push(0.5 + LSB);
    call(half, false);
    check("one LSB above thr routes to residual", s_res.size() == 1 && s_pri.empty());

    std::printf("T4  exact tau update arithmetic:\n");
    drain();
    call(DEF, true);                    // tau = 1, thr = 1
    push(1.5);                          // excess 0.5 -> tau -= 0.5/16
    tau_t a = call(DEF, false);
    check("residual step: tau = 1 - 0.5/16 = 0.96875", a.to_double() == 0.96875);
    push(0.0);                          // primary: tau += (1 - 0.96875)/16
    tau_t b = call(DEF, false);
    double expect = 0.96875 + (1.0 - 0.96875) / 16.0;   // 0.998046875, exact in tau_t
    check("primary step relaxes towards tau_sensitivity by delta/16", b.to_double() == expect);
    push(0.0, true);                    // shock with no excess: tau unchanged
    tau_t c = call(DEF, false);
    check("shock flag routes to residual without changing tau", s_res.size() == 2 && c == b);

    std::printf("T5  reset:\n");
    drain();
    call(DEF, true);
    for (int i = 0; i < 20; i++) { push(1.9); call(DEF, false); }
    tau_t low = call(DEF, false);
    check("spikes lower tau", low.to_double() < 1.0);
    tau_t r = call(DEF, true);
    check("reset_tau reloads tau_sensitivity", r.to_double() == 1.0);
    Regs hi = {1.0, 6.0, 0.25, 4.0};
    check("reset clamps tau_sensitivity above tau_max", call(hi, true).to_double() == 4.0);
    Regs lo = {1.0, 0.1, 0.25, 4.0};
    check("reset clamps tau_sensitivity below tau_min", call(lo, true).to_double() == 0.25);
    drain();
    call(DEF, true);
    for (int i = 0; i < 400; i++) { push(1.99); call(DEF, false); }
    check("repeated spikes never push tau below tau_min", call(DEF, false).to_double() >= DEF.tmin);

    std::printf("T6  fixed-point rounding and saturation of data_t:\n");
    check("+LSB/2 rounds up to one LSB (AP_RND)", ((data_t)(LSB / 2)).to_double() == LSB);
    check("+LSB/4 rounds to zero", ((data_t)(LSB / 4)).to_double() == 0.0);
    check("-LSB/2 rounds towards +inf to zero (AP_RND)", ((data_t)(-LSB / 2)).to_double() == 0.0);
    check("+3.0 saturates to 2 - 2^-16 (AP_SAT)", ((data_t)3.0).to_double() == 2.0 - LSB);
    check("-3.0 saturates to -2 (AP_SAT)", ((data_t)-3.0).to_double() == -2.0);
    drain();
    call(DEF, true);
    push(-2.0);                         // most negative sample: magnitude 2 fits acc_t
    call(DEF, false);
    telemetry_packet o;
    check("most negative sample is routed and forwarded unchanged",
          s_res.read_nb(o) && o.data_value.to_double() == -2.0);
    drain();
}

int main() {
    std::printf("OES-32 kernel edge cases (SYNTHETIC; g++ only; Vitis/FPGA UNRUN)\n");
    membrane_tests();
    triage_tests();
    std::printf("\n%d/%d checks passed\n", checks_passed, checks_run);
    return checks_passed == checks_run ? 0 : 1;
}
