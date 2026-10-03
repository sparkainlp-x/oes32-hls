// =====================================================================
// OES-32 Telemetry Triage — g++ Testbench (SYNTHETIC stimuli)
// File:   test_oes32_triage.cpp
// DUT:    oes32_triage_accelerator (oes32_triage_stream.cpp)
//
// Compares the fixed-point kernel, packet by packet, with a
// double-precision golden model of the same algorithm. All stimuli are
// SYNTHETIC and deterministic (fixed-seed LCG). No Vitis tools needed.
//
// Build & run (see Makefile; `make test-triage` fetches the pinned
// Apache-2.0 ap_int/ap_fixed headers and uses the testbench-only
// hls_stream shim in tb/include_shim/):
//
//   make test-triage
//
// Pass criteria:
//   * routing (primary vs residual) matches the golden model EXACTLY
//     for every packet;
//   * |tau_fixed - tau_golden| <= TAU_TOL (16 LSB of tau_t = 2^-10)
//     after every packet (quantisation tolerance, see README);
//   * tau stays within [tau_min, tau_max] after every call;
//   * packets are forwarded unchanged, in order, none lost/duplicated;
//   * scenario-specific checks (reset, clamping, shock burst, ...).
//
// Output: a CSV trace of tau over time (default triage_tau_trace.csv,
// or argv[1]). Exit code 0 only if every check passes.
//
// Copyright (C) 2026 Jean-François Brisson, Spark AI NLP.
// SPDX-License-Identifier: AGPL-3.0-only
// =====================================================================

#include "oes32_triage_stream.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ---------------------------------------------------------------------
// Check bookkeeping
// ---------------------------------------------------------------------
static int checks_run = 0;
static int checks_passed = 0;

static void check(const char *name, bool ok, const char *detail = "") {
    checks_run++;
    if (ok) checks_passed++;
    std::printf("  %s  %s%s%s\n", ok ? "PASS" : "FAIL", name,
                detail[0] ? "  " : "", detail);
}

// ---------------------------------------------------------------------
// Tolerance: 16 LSB of tau_t (LSB = 2^-14).
// ---------------------------------------------------------------------
static const double TAU_LSB = 1.0 / 16384.0;
static const double TAU_TOL = 16.0 * TAU_LSB;   // 2^-10 ~ 9.77e-4

// ---------------------------------------------------------------------
// Deterministic SYNTHETIC stimulus source: 32-bit LCG (Numerical
// Recipes constants), fixed seed.
// ---------------------------------------------------------------------
static uint32_t lcg_state = 20261002u;
static double lcg_uniform() {            // [0, 1)
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return (double)(lcg_state >> 8) / 16777216.0;
}

// ---------------------------------------------------------------------
// Register file shared by DUT and golden model
// ---------------------------------------------------------------------
struct Regs {
    double thr, sens, tmin, tmax;
};

// ---------------------------------------------------------------------
// Golden model: same algorithm in double precision.
// ---------------------------------------------------------------------
struct Golden {
    bool   init = false;
    double tau  = 1.0;

    static double clamp(double v, double lo, double hi) {
        if (v < lo) v = lo;
        if (v > hi) v = hi;
        return v;
    }
    // Returns true if routed to residual. 'has_pkt' false = empty call.
    bool step(const Regs &r, bool reset, bool has_pkt, double v, bool shock) {
        if (!init || reset) { tau = clamp(r.sens, r.tmin, r.tmax); init = true; }
        if (!has_pkt) return false;
        double mag = std::fabs(v);
        double w = mag * tau;
        bool over = w > r.thr;
        double next;
        bool residual = over || shock;
        if (residual) {
            double excess = over ? (w - r.thr) : 0.0;
            next = tau - excess / 16.0;
        } else {
            next = tau + (r.sens - tau) / 16.0;
        }
        tau = clamp(next, r.tmin, r.tmax);
        return residual;
    }
};

// ---------------------------------------------------------------------
// Harness state
// ---------------------------------------------------------------------
static hls::stream<telemetry_packet> s_in("inputStream");
static hls::stream<telemetry_packet> s_pri("primaryStream");
static hls::stream<telemetry_packet> s_res("residualStream");
static Golden golden;
static FILE *csv = nullptr;
static long  step_no = 0;

// Aggregate statistics over every packet
static long   n_packets = 0, n_route_mismatch = 0, n_tau_out_of_tol = 0;
static long   n_bounds_violation = 0, n_payload_mismatch = 0;
static long   n_primary = 0, n_residual = 0;
static double max_tau_err = 0.0;
static double min_margin = 1e9;   // min |w_golden - thr| over non-shock packets

struct StepResult {
    bool   had_output;
    bool   residual;   // DUT routing
    double tau;        // DUT tau_out
};

// One kernel call (optionally with a packet) plus golden comparison.
static StepResult run_step(const char *scenario, const Regs &r, bool reset,
                           bool has_pkt, double v_in = 0.0, bool shock = false) {
    data_t dv = v_in;                        // quantise stimulus (AP_RND, AP_SAT)
    double v = dv.to_double();               // golden sees the same sample
    if (has_pkt) {
        telemetry_packet p;
        p.data_value = dv;
        p.is_shock = shock ? 1 : 0;
        s_in.write(p);
    }
    tau_t tau_out = -1;
    double tau_before_golden = golden.tau;
    bool g_init_before = golden.init;

    oes32_triage_accelerator(s_in, s_pri, s_res,
                             (thr_t)r.thr, (tau_t)r.sens, (tau_t)r.tmin,
                             (tau_t)r.tmax, (ap_uint<1>)(reset ? 1 : 0), tau_out);

    // Golden decision margin (computed from golden pre-step state).
    if (has_pkt && !shock) {
        double gt = (!g_init_before || reset)
                        ? Golden::clamp(r.sens, r.tmin, r.tmax) : tau_before_golden;
        double m = std::fabs(std::fabs(v) * gt - r.thr);
        if (m < min_margin) min_margin = m;
    }
    bool g_res = golden.step(r, reset, has_pkt, v, shock);

    StepResult sr;
    sr.tau = tau_out.to_double();
    sr.had_output = false;
    sr.residual = false;

    int n_out = (int)s_pri.size() + (int)s_res.size();
    if (has_pkt) {
        n_packets++;
        if (n_out != 1) n_payload_mismatch++;
        telemetry_packet o;
        if (s_res.read_nb(o)) { sr.residual = true; sr.had_output = true; n_residual++; }
        else if (s_pri.read_nb(o)) { sr.had_output = true; n_primary++; }
        if (sr.had_output &&
            (o.data_value != dv || o.is_shock != (ap_uint<1>)(shock ? 1 : 0)))
            n_payload_mismatch++;
        if (sr.residual != g_res) n_route_mismatch++;
    } else if (n_out != 0) {
        n_payload_mismatch++;
    }
    // drain anything unexpected
    telemetry_packet junk;
    while (s_pri.read_nb(junk)) {}
    while (s_res.read_nb(junk)) {}

    double err = std::fabs(sr.tau - golden.tau);
    if (err > max_tau_err) max_tau_err = err;
    if (err > TAU_TOL) n_tau_out_of_tol++;
    if (sr.tau < r.tmin || sr.tau > r.tmax) n_bounds_violation++;

    if (csv) {
        std::fprintf(csv, "%ld,%s,%d,%.8f,%d,%d,%s,%s,%.8f,%.8f,%.3e,%.6f,%.6f,%.6f,%.6f\n",
                     step_no, scenario, has_pkt ? 1 : 0, v, shock ? 1 : 0,
                     reset ? 1 : 0,
                     has_pkt ? (sr.residual ? "residual" : "primary") : "none",
                     has_pkt ? (g_res ? "residual" : "primary") : "none",
                     sr.tau, golden.tau, err, r.thr, r.sens, r.tmin, r.tmax);
    }
    step_no++;
    return sr;
}

// ---------------------------------------------------------------------
// Scenarios (all SYNTHETIC)
// ---------------------------------------------------------------------
static const Regs DEF = {1.0, 1.0, 0.25, 4.0};

static void sc_init_first_call() {
    std::printf("SC1  first call, empty stream (init loads tau_sensitivity):\n");
    StepResult s = run_step("init", DEF, false, false);
    check("tau_out == tau_sensitivity on first call", s.tau == DEF.sens);
    check("no packet emitted on empty call", s_pri.empty() && s_res.empty());
}

static void sc_ramps() {
    std::printf("SC2  positive and negative ramps 0 -> +/-1.9 (200 samples each):\n");
    int res_pos = 0, res_neg = 0;
    for (int i = 0; i < 200; i++)
        res_pos += run_step("ramp_pos", DEF, false, true, 1.9 * i / 199.0).residual;
    for (int i = 0; i < 200; i++)
        res_neg += run_step("ramp_neg", DEF, false, true, -1.9 * i / 199.0).residual;
    char d[96];
    std::snprintf(d, sizeof d, "(residual: pos=%d neg=%d of 200)", res_pos, res_neg);
    check("both ramps reach the residual path", res_pos > 0 && res_neg > 0, d);
}

static void sc_spikes() {
    std::printf("SC3  seeded noise (|x|<=0.2) with +/- spikes every 25 samples:\n");
    run_step("spike_reset", DEF, true, false);
    int spikes = 0, spikes_res = 0, neg_spikes_res = 0, neg_spikes = 0;
    for (int i = 0; i < 400; i++) {
        double x = 0.4 * lcg_uniform() - 0.2;
        bool spike = (i % 25 == 12);
        if (spike) {
            int k = (i / 25) % 4;
            x = (k == 0) ? 1.8 : (k == 1) ? -1.8 : (k == 2) ? 1.99 : -2.0;
        }
        StepResult s = run_step("spikes", DEF, false, true, x);
        if (spike) {
            spikes++; spikes_res += s.residual;
            if (x < 0) { neg_spikes++; neg_spikes_res += s.residual; }
        }
    }
    char d[96];
    std::snprintf(d, sizeof d, "(%d/%d spikes residual, %d/%d negative)",
                  spikes_res, spikes, neg_spikes_res, neg_spikes);
    check("every spike routed to residual", spikes_res == spikes, d);
    check("negative spikes trigger residual (incl. -2.0 saturating min)",
          neg_spikes > 0 && neg_spikes_res == neg_spikes);
}

static void sc_shock_burst() {
    std::printf("SC4  shock-only burst (32 small samples, is_shock=1):\n");
    run_step("shock_reset", DEF, true, false);
    // pull tau below tau_sensitivity first
    for (int i = 0; i < 4; i++) run_step("shock_pre", DEF, false, true, 1.9);
    double tau0 = run_step("shock_pre", DEF, false, false).tau;
    bool all_res = true, never_rises = true;
    double prev = tau0;
    for (int i = 0; i < 32; i++) {
        StepResult s = run_step("shock_burst", DEF, false, true,
                                0.05 * (i % 2 ? -1 : 1), true);
        all_res = all_res && s.residual;
        if (s.tau > prev) never_rises = false;
        prev = s.tau;
    }
    char d[96];
    std::snprintf(d, sizeof d, "(tau before=%.6f after=%.6f)", tau0, prev);
    check("all shock-only packets routed to residual", all_res);
    check("tau does not rise during shock-only burst", never_rises, d);
    check("tau below tau_sensitivity before burst (burst is meaningful)",
          tau0 < DEF.sens);
}

static void sc_reset() {
    std::printf("SC5  reset_tau reloads tau_sensitivity (level, held 2 calls):\n");
    for (int i = 0; i < 6; i++) run_step("reset_pre", DEF, false, true, -1.95);
    double before = run_step("reset_pre", DEF, false, false).tau;
    StepResult a = run_step("reset_hold", DEF, true, false);
    StepResult b = run_step("reset_hold", DEF, true, true, 0.1);
    char d[96];
    std::snprintf(d, sizeof d, "(before=%.6f after=%.6f)", before, a.tau);
    check("tau was pulled away from tau_sensitivity", before < DEF.sens);
    check("reset_tau=1 (empty call) -> tau_out == tau_sensitivity", a.tau == DEF.sens, d);
    // With reset held and a small primary packet, tau reloads then relaxes
    // toward sens: it must stay at sens.
    check("reset_tau=1 with packet -> tau stays at tau_sensitivity", b.tau == DEF.sens);
    run_step("reset_release", DEF, false, true, 0.1);
}

static void sc_clamp_max() {
    std::printf("SC6  tau_max clamp (tau_sensitivity=6.0 > tau_max=3.0):\n");
    Regs r = {10.0, 6.0, 0.25, 3.0};
    StepResult s = run_step("clamp_max", r, true, false);
    check("reload is clamped to tau_max", s.tau == r.tmax);
    bool at_max = true;
    for (int i = 0; i < 50; i++)
        at_max = at_max && (run_step("clamp_max", r, false, true, 0.3).tau == r.tmax);
    check("relaxation toward 6.0 is held at tau_max for 50 packets", at_max);
}

static void sc_clamp_min() {
    std::printf("SC7  tau_min clamp (repeated 1.9 spikes, thr=0.5, tau_min=0.5):\n");
    Regs r = {0.5, 1.0, 0.5, 4.0};
    run_step("clamp_min", r, true, false);
    int reached = -1;
    double t = 0;
    for (int i = 0; i < 200; i++) {
        t = run_step("clamp_min", r, false, true, (i % 2) ? 1.9 : -1.9).tau;
        if (reached < 0 && t == r.tmin) reached = i;
    }
    char d[96];
    std::snprintf(d, sizeof d, "(reached at packet %d, final tau=%.6f)", reached, t);
    check("tau reaches and holds tau_min", reached >= 0 && t == r.tmin, d);
}

static void sc_relax() {
    std::printf("SC8  relaxation back to tau_sensitivity after spikes:\n");
    run_step("relax_reset", DEF, true, false);
    for (int i = 0; i < 8; i++) run_step("relax_pre", DEF, false, true, 1.9);
    double t = 0;
    for (int i = 0; i < 300; i++) t = run_step("relax", DEF, false, true, 0.05).tau;
    char d[96];
    std::snprintf(d, sizeof d, "(final tau=%.6f, |tau - sens|=%.2e, limit 8 LSB=%.2e)",
                  t, std::fabs(t - DEF.sens), 8 * TAU_LSB);
    check("tau returns to within 8 LSB of tau_sensitivity (rounding dead zone bounded)",
          std::fabs(t - DEF.sens) <= 8 * TAU_LSB, d);
}

int main(int argc, char **argv) {
    const char *csv_path = (argc > 1) ? argv[1] : "triage_tau_trace.csv";
    csv = std::fopen(csv_path, "w");
    if (!csv) { std::perror("CSV open"); return 2; }
    std::fprintf(csv, "# SYNTHETIC stimuli; oes32_triage_accelerator vs double golden model; seed=20261002\n");
    std::fprintf(csv, "step,scenario,has_packet,value,is_shock,reset_tau,route_fixed,route_golden,"
                      "tau_fixed,tau_golden,abs_err,rate_limit_threshold,tau_sensitivity,tau_min,tau_max\n");

    std::printf("OES-32 triage stream testbench (SYNTHETIC stimuli, g++; synthesis UNRUN)\n");
    std::printf("tau_t LSB = 2^-14 = %.3e; tau tolerance = 16 LSB = %.3e\n\n", TAU_LSB, TAU_TOL);

    sc_init_first_call();
    sc_ramps();
    sc_spikes();
    sc_shock_burst();
    sc_reset();
    sc_clamp_max();
    sc_clamp_min();
    sc_relax();

    std::fclose(csv);

    std::printf("\nGLOBAL  per-packet comparison against golden model:\n");
    char d[160];
    std::snprintf(d, sizeof d, "(%ld packets: %ld primary, %ld residual; mismatches=%ld)",
                  n_packets, n_primary, n_residual, n_route_mismatch);
    check("routing matches golden exactly for every packet", n_route_mismatch == 0, d);
    std::snprintf(d, sizeof d, "(max |err| = %.3e, %ld steps out of tolerance)",
                  max_tau_err, n_tau_out_of_tol);
    check("tau within 16-LSB tolerance of golden after every call", n_tau_out_of_tol == 0, d);
    std::snprintf(d, sizeof d, "(%ld violations)", n_bounds_violation);
    check("tau within [tau_min, tau_max] after every call", n_bounds_violation == 0, d);
    std::snprintf(d, sizeof d, "(%ld mismatches)", n_payload_mismatch);
    check("packets forwarded unchanged, exactly one output each", n_payload_mismatch == 0, d);
    std::printf("  INFO  min golden decision margin |w - thr| (non-shock) = %.3e\n", min_margin);
    std::printf("  INFO  tau trace written to %s (%ld rows)\n", csv_path, step_no);

    std::printf("\nResults: %d / %d checks passed\n", checks_passed, checks_run);
    return (checks_passed == checks_run) ? 0 : 1;
}
