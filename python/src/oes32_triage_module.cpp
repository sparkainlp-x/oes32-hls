// =====================================================================
// pybind11 bindings for the REAL C++ kernel oes32_triage_accelerator
// File: python/src/oes32_triage_module.cpp  ->  oes32_triage._core
//
// The kernel source (oes32_triage_stream.cpp) is compiled UNCHANGED into
// this extension, with the pinned open-source Xilinx ap_* headers and the
// testbench-only hls_stream shim (tb/include_shim/hls_stream.h). Nothing
// here is synthesised RTL; this is the same C++ that the g++ testbench
// runs. FPGA synthesis and hardware: UNRUN. Stimuli in tests: SYNTHETIC.
//
// ---------------------------------------------------------------------
// Function-static state, handled explicitly
// ---------------------------------------------------------------------
// The kernel keeps its state (adaptive_tau, init) in function-static
// variables, as an HLS top function does. One process (one loaded copy of
// this extension) therefore holds exactly ONE hardware kernel state.
//
// TriageKernel objects give each Python object its own logical state on
// top of that single static state, using only the kernel's own register
// interface:
//   * Fresh object: its first call is issued with reset_tau = 1. This is
//     behaviourally identical to the kernel's first-ever call, because
//     the init path and the reset path both load
//     clamp(tau_sensitivity, tau_min, tau_max). The static initial value
//     (tau = 1) is never observable.
//   * Context switch: if another TriageKernel used the kernel since this
//     object's last call, its saved tau is restored first with one extra
//     kernel invocation on an EMPTY input stream:
//     reset_tau = 1, tau_sensitivity = saved tau, tau_min = -8,
//     tau_max = 8 - 2^-14. The clamp is then a no-op, so tau is restored
//     bit-exactly and no packet is consumed or emitted.
// Calls are serialised by the GIL (it is never released here), so there
// is no concurrent access to the static state. The number of real kernel
// invocations, including restore calls, is exposed as
// kernel_invocations() for transparency.
//
// Copyright (C) 2026 Jean-François Brisson, Spark AI NLP.
// SPDX-License-Identifier: AGPL-3.0-only
// =====================================================================

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>

#include "oes32_triage_stream.h"

namespace py = pybind11;

namespace {

// ---------------------------------------------------------------------
// Raw <-> ap_fixed helpers (two's complement, W bits)
// ---------------------------------------------------------------------
template <int W>
constexpr int64_t raw_min() { return -(int64_t(1) << (W - 1)); }
template <int W>
constexpr int64_t raw_max() { return (int64_t(1) << (W - 1)) - 1; }

template <typename T, int W>
int64_t to_raw(const T &v) {
    uint64_t u = (uint64_t)v.range(W - 1, 0).to_uint64();
    int64_t s = (int64_t)u;
    if (s & (int64_t(1) << (W - 1))) s -= (int64_t(1) << W);
    return s;
}

template <typename T, int W>
T from_raw(int64_t raw, const char *what) {
    if (raw < raw_min<W>() || raw > raw_max<W>())
        throw py::value_error(std::string(what) + ": raw value out of range for " +
                              std::to_string(W) + "-bit two's complement");
    T v;
    v.range(W - 1, 0) = (uint64_t)raw & ((uint64_t(1) << W) - 1);
    return v;
}

template <typename T>
T from_float(double x, const char *what) {
    if (!std::isfinite(x))
        throw py::value_error(std::string(what) + ": value must be finite");
    return T(x);  // ap_fixed conversion: AP_RND (half toward +inf), AP_SAT
}

constexpr int DATA_W = 18, DATA_F = 16;
constexpr int TAU_W = 18, TAU_F = 14;
constexpr int THR_W = 24, THR_F = 18;

enum Route : int8_t { ROUTE_NONE = -1, ROUTE_PRIMARY = 0, ROUTE_RESIDUAL = 1 };

// ---------------------------------------------------------------------
// Process-wide kernel bookkeeping
// ---------------------------------------------------------------------
uint64_t g_owner = 0;         // id of the TriageKernel whose state is loaded
uint64_t g_next_id = 1;
uint64_t g_invocations = 0;   // real kernel calls, incl. restore calls

struct CallResult {
    int8_t  route;
    int64_t tau_raw;
    int64_t out_value_raw;  // forwarded packet payload (0 if none)
    int     out_is_shock;
    int     n_primary;      // number of packets on primaryStream
    int     n_residual;     // number of packets on residualStream
    int     n_input_left;   // packets left unread on inputStream
};

CallResult invoke(bool has_pkt, data_t value, bool shock, thr_t thr,
                  tau_t sens, tau_t tmin, tau_t tmax, bool reset) {
    hls::stream<telemetry_packet> in("inputStream"), pri("primaryStream"),
        res("residualStream");
    if (has_pkt) {
        telemetry_packet p;
        p.data_value = value;
        p.is_shock = shock ? 1 : 0;
        in.write(p);
    }
    tau_t tau_out = 0;
    oes32_triage_accelerator(in, pri, res, thr, sens, tmin, tmax,
                             (ap_uint<1>)(reset ? 1 : 0), tau_out);
    ++g_invocations;

    CallResult r{};
    r.n_primary = (int)pri.size();
    r.n_residual = (int)res.size();
    r.n_input_left = (int)in.size();
    r.tau_raw = to_raw<tau_t, TAU_W>(tau_out);
    r.route = ROUTE_NONE;
    telemetry_packet o;
    if (res.read_nb(o)) r.route = ROUTE_RESIDUAL;
    else if (pri.read_nb(o)) r.route = ROUTE_PRIMARY;
    if (r.route != ROUTE_NONE) {
        r.out_value_raw = to_raw<data_t, DATA_W>(o.data_value);
        r.out_is_shock = (int)o.is_shock.to_uint();
    }
    return r;
}

// ---------------------------------------------------------------------
// TriageKernel: one logical kernel instance
// ---------------------------------------------------------------------
class TriageKernel {
public:
    TriageKernel() : id_(g_next_id++) {}

    // Forget this object's state: the next call behaves like the kernel's
    // first-ever call (loads clamp(tau_sensitivity)).
    void reset() { fresh_ = true; }

    bool initialized() const { return !fresh_; }
    std::optional<int64_t> tau_raw() const {
        if (fresh_) return std::nullopt;
        return tau_raw_;
    }
    std::optional<double> tau() const {
        if (fresh_) return std::nullopt;
        return (double)tau_raw_ / (double)(1 << TAU_F);
    }

    CallResult call(bool has_pkt, data_t v, bool shock, thr_t thr, tau_t sens,
                    tau_t tmin, tau_t tmax, bool reset) {
        bool force_reset = fresh_;
        if (!fresh_ && g_owner != id_) restore();
        CallResult r = invoke(has_pkt, v, shock, thr, sens, tmin, tmax,
                              reset || force_reset);
        g_owner = id_;
        fresh_ = false;
        tau_raw_ = r.tau_raw;
        return r;
    }

private:
    void restore() {
        tau_t saved = from_raw<tau_t, TAU_W>(tau_raw_, "tau");
        tau_t lo = from_raw<tau_t, TAU_W>(raw_min<TAU_W>(), "tau_min");
        tau_t hi = from_raw<tau_t, TAU_W>(raw_max<TAU_W>(), "tau_max");
        CallResult r = invoke(false, data_t(0), false, thr_t(0), saved, lo, hi, true);
        if (r.tau_raw != tau_raw_ || r.n_primary || r.n_residual)
            throw std::runtime_error("oes32_triage: state restore failed");
        g_owner = id_;
    }

    uint64_t id_;
    bool fresh_ = true;
    int64_t tau_raw_ = 0;
};

py::object route_obj(int8_t r) {
    if (r == ROUTE_PRIMARY) return py::str("primary");
    if (r == ROUTE_RESIDUAL) return py::str("residual");
    return py::none();
}

py::dict detail_dict(const CallResult &r) {
    py::dict d;
    d["route"] = route_obj(r.route);
    d["tau_raw"] = r.tau_raw;
    d["tau"] = (double)r.tau_raw / (double)(1 << TAU_F);
    d["out_value_raw"] = r.route == ROUTE_NONE ? py::object(py::none())
                                               : py::object(py::int_(r.out_value_raw));
    d["out_is_shock"] = r.route == ROUTE_NONE ? py::object(py::none())
                                              : py::object(py::bool_(r.out_is_shock != 0));
    d["n_primary"] = r.n_primary;
    d["n_residual"] = r.n_residual;
    d["n_input_left"] = r.n_input_left;
    return d;
}

// Broadcast accessor for 1-element-or-N arrays.
template <typename T>
struct Bcast {
    py::array_t<T, py::array::c_style | py::array::forcecast> a;
    const T *p = nullptr;
    py::ssize_t n = 0;
    Bcast(py::handle h, const char *name, py::ssize_t N) {
        a = py::array_t<T, py::array::c_style | py::array::forcecast>::ensure(h);
        if (!a) throw py::type_error(std::string(name) + ": not convertible to an array");
        if (a.ndim() > 1) throw py::value_error(std::string(name) + ": must be scalar or 1-D");
        n = a.size();
        if (n != 1 && n != N)
            throw py::value_error(std::string(name) + ": length must be 1 or len(values)");
        p = a.data();
    }
    T operator[](py::ssize_t i) const { return p[n == 1 ? 0 : i]; }
};

}  // namespace

PYBIND11_MODULE(_core, m) {
    m.doc() = "pybind11 bindings for the C++ kernel oes32_triage_accelerator "
              "(oes32_triage_stream.cpp compiled unchanged; g++ C-simulation only, "
              "synthesis UNRUN).";

    m.attr("DATA_W") = DATA_W; m.attr("DATA_FRAC") = DATA_F;
    m.attr("TAU_W") = TAU_W;   m.attr("TAU_FRAC") = TAU_F;
    m.attr("THR_W") = THR_W;   m.attr("THR_FRAC") = THR_F;
    m.attr("ROUTE_NONE") = (int)ROUTE_NONE;
    m.attr("ROUTE_PRIMARY") = (int)ROUTE_PRIMARY;
    m.attr("ROUTE_RESIDUAL") = (int)ROUTE_RESIDUAL;

    m.def("kernel_invocations", []() { return g_invocations; },
          "Total number of real oes32_triage_accelerator calls in this process, "
          "including state-restore calls made on context switches.");

    m.def("quantize_data", [](double x) { return to_raw<data_t, DATA_W>(from_float<data_t>(x, "value")); },
          py::arg("x"), "Raw data_t (ap_fixed<18,2,AP_RND,AP_SAT>) integer for float x, via the C++ conversion.");
    m.def("quantize_tau", [](double x) { return to_raw<tau_t, TAU_W>(from_float<tau_t>(x, "tau")); },
          py::arg("x"), "Raw tau_t (ap_fixed<18,4,AP_RND,AP_SAT>) integer for float x.");
    m.def("quantize_thr", [](double x) { return to_raw<thr_t, THR_W>(from_float<thr_t>(x, "threshold")); },
          py::arg("x"), "Raw thr_t (ap_fixed<24,6,AP_RND,AP_SAT>) integer for float x.");

    py::class_<TriageKernel>(m, "TriageKernel", R"doc(
One logical instance of the C++ kernel oes32_triage_accelerator.

The kernel's function-static state is shared process-wide; each TriageKernel
saves its tau after every call and, if another instance ran in between,
restores it bit-exactly through the kernel's own reset_tau register on an
empty stream (see module source). A fresh instance's first call behaves like
the kernel's first-ever call.

Register arguments mirror the AXI4-Lite CONTROL_BUS registers and are applied
on every call (as in hardware, they may change between calls).
)doc")
        .def(py::init<>())
        .def("reset", &TriageKernel::reset,
             "Forget state: the next call loads clamp(tau_sensitivity, tau_min, tau_max).")
        .def_property_readonly("initialized", &TriageKernel::initialized)
        .def_property_readonly("tau", &TriageKernel::tau, "Current tau (float) or None if fresh.")
        .def_property_readonly("tau_raw", &TriageKernel::tau_raw,
                               "Current tau as raw tau_t integer (14 fractional bits) or None if fresh.")
        .def("step",
             [](TriageKernel &k, std::optional<double> value, bool is_shock, double tau_sensitivity,
                double rate_limit_threshold, double tau_min, double tau_max, bool reset_tau) {
                 data_t v = value ? from_float<data_t>(*value, "value") : data_t(0);
                 CallResult r = k.call(value.has_value(), v, is_shock,
                                       from_float<thr_t>(rate_limit_threshold, "rate_limit_threshold"),
                                       from_float<tau_t>(tau_sensitivity, "tau_sensitivity"),
                                       from_float<tau_t>(tau_min, "tau_min"),
                                       from_float<tau_t>(tau_max, "tau_max"), reset_tau);
                 return py::make_tuple(route_obj(r.route), (double)r.tau_raw / (double)(1 << TAU_F));
             },
             py::arg("value"), py::arg("is_shock") = false, py::arg("tau_sensitivity") = 1.0,
             py::arg("rate_limit_threshold") = 1.0, py::arg("tau_min") = 0.25,
             py::arg("tau_max") = 4.0, py::arg("reset_tau") = false,
             R"doc(One kernel call. value=None is an empty-stream call (no packet).

Floats are quantised by the C++ ap_fixed conversion (AP_RND, AP_SAT).
Returns (route, tau_out) with route in {'primary', 'residual', None}.)doc")
        .def("step_raw",
             [](TriageKernel &k, std::optional<int64_t> value_raw, bool is_shock,
                int64_t tau_sensitivity_raw, int64_t rate_limit_threshold_raw, int64_t tau_min_raw,
                int64_t tau_max_raw, bool reset_tau) {
                 data_t v = value_raw ? from_raw<data_t, DATA_W>(*value_raw, "value_raw") : data_t(0);
                 CallResult r = k.call(value_raw.has_value(), v, is_shock,
                                       from_raw<thr_t, THR_W>(rate_limit_threshold_raw, "rate_limit_threshold_raw"),
                                       from_raw<tau_t, TAU_W>(tau_sensitivity_raw, "tau_sensitivity_raw"),
                                       from_raw<tau_t, TAU_W>(tau_min_raw, "tau_min_raw"),
                                       from_raw<tau_t, TAU_W>(tau_max_raw, "tau_max_raw"), reset_tau);
                 return detail_dict(r);
             },
             py::arg("value_raw"), py::arg("is_shock"), py::arg("tau_sensitivity_raw"),
             py::arg("rate_limit_threshold_raw"), py::arg("tau_min_raw"), py::arg("tau_max_raw"),
             py::arg("reset_tau") = false,
             R"doc(One kernel call with raw two's-complement register/sample integers
(data_t: 16 frac bits, tau_t: 14, thr_t: 18). Returns a dict with route,
tau_raw, tau, out_value_raw, out_is_shock, n_primary, n_residual, n_input_left.)doc")
        .def("step_detail",
             [](TriageKernel &k, std::optional<double> value, bool is_shock, double tau_sensitivity,
                double rate_limit_threshold, double tau_min, double tau_max, bool reset_tau) {
                 data_t v = value ? from_float<data_t>(*value, "value") : data_t(0);
                 CallResult r = k.call(value.has_value(), v, is_shock,
                                       from_float<thr_t>(rate_limit_threshold, "rate_limit_threshold"),
                                       from_float<tau_t>(tau_sensitivity, "tau_sensitivity"),
                                       from_float<tau_t>(tau_min, "tau_min"),
                                       from_float<tau_t>(tau_max, "tau_max"), reset_tau);
                 return detail_dict(r);
             },
             py::arg("value"), py::arg("is_shock") = false, py::arg("tau_sensitivity") = 1.0,
             py::arg("rate_limit_threshold") = 1.0, py::arg("tau_min") = 0.25,
             py::arg("tau_max") = 4.0, py::arg("reset_tau") = false,
             "Like step() but returns the full detail dict (see step_raw).")
        .def("run",
             [](TriageKernel &k, py::handle values, py::handle is_shock, py::handle tau_sensitivity,
                py::handle rate_limit_threshold, py::handle tau_min, py::handle tau_max,
                py::handle reset_tau, py::handle has_packet) {
                 auto vals = py::array_t<double, py::array::c_style | py::array::forcecast>::ensure(values);
                 if (!vals || vals.ndim() != 1) throw py::value_error("values: must be a 1-D array");
                 const py::ssize_t N = vals.size();
                 const double *vp = vals.data();
                 Bcast<bool> sh(is_shock, "is_shock", N), rs(reset_tau, "reset_tau", N),
                     hp(has_packet, "has_packet", N);
                 Bcast<double> se(tau_sensitivity, "tau_sensitivity", N),
                     th(rate_limit_threshold, "rate_limit_threshold", N), lo(tau_min, "tau_min", N),
                     hi(tau_max, "tau_max", N);
                 py::array_t<int8_t> route(N);
                 py::array_t<int32_t> tau_raw(N), out_raw(N);
                 py::array_t<double> tau(N);
                 auto R = route.mutable_unchecked<1>();
                 auto T = tau_raw.mutable_unchecked<1>();
                 auto O = out_raw.mutable_unchecked<1>();
                 auto F = tau.mutable_unchecked<1>();
                 for (py::ssize_t i = 0; i < N; ++i) {
                     bool has = hp[i];
                     data_t v = has ? from_float<data_t>(vp[i], "values") : data_t(0);
                     CallResult r = k.call(has, v, sh[i], from_float<thr_t>(th[i], "rate_limit_threshold"),
                                           from_float<tau_t>(se[i], "tau_sensitivity"),
                                           from_float<tau_t>(lo[i], "tau_min"),
                                           from_float<tau_t>(hi[i], "tau_max"), rs[i]);
                     R(i) = r.route;
                     T(i) = (int32_t)r.tau_raw;
                     O(i) = r.route == ROUTE_NONE ? 0 : (int32_t)r.out_value_raw;
                     F(i) = (double)r.tau_raw / (double)(1 << TAU_F);
                 }
                 py::dict d;
                 d["route"] = route;
                 d["tau"] = tau;
                 d["tau_raw"] = tau_raw;
                 d["out_value_raw"] = out_raw;
                 return d;
             },
             py::arg("values"), py::arg("is_shock") = false, py::arg("tau_sensitivity") = 1.0,
             py::arg("rate_limit_threshold") = 1.0, py::arg("tau_min") = 0.25,
             py::arg("tau_max") = 4.0, py::arg("reset_tau") = false, py::arg("has_packet") = true,
             R"doc(Vectorised loop over len(values) kernel calls, executed in C++.

Every other argument is a scalar or an array of len(values) (per-call
register values). has_packet=False entries are empty-stream calls (value
ignored). Returns a dict of NumPy arrays: route (int8: -1 none, 0 primary,
1 residual), tau (float64), tau_raw (int32, 14 frac bits), out_value_raw
(int32 forwarded payload, 0 when no packet).)doc");
}
