// =====================================================================
// TESTBENCH-ONLY SHIM — NOT the AMD/Xilinx hls_stream.h
// File: tb/include_shim/hls_stream.h
//
// Minimal std::queue-based stand-in for hls::stream<T> so that the
// streaming kernel and its testbench compile with plain g++ (no Vitis).
// Provides: read(), read(T&), write(), read_nb(), empty(), full(), size().
// Semantics differ from the real header: unbounded depth, no blocking
// (read() on an empty stream aborts the testbench), no hardware model.
//
// This directory is added to the include path ONLY by the Makefile /
// CI g++ build. run_hls_triage.tcl does not use it; Vitis HLS supplies
// its own hls_stream.h. Including this file under __SYNTHESIS__ is an
// error by design.
//
// Copyright (C) 2026 Jean-François Brisson, Spark AI NLP.
// SPDX-License-Identifier: AGPL-3.0-only
// =====================================================================
#ifndef OES32_TB_HLS_STREAM_SHIM_H
#define OES32_TB_HLS_STREAM_SHIM_H

#ifdef __SYNTHESIS__
#error "tb/include_shim/hls_stream.h is a g++ testbench shim; use the Vitis HLS hls_stream.h for synthesis."
#endif

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <queue>
#include <string>

namespace hls {

template <typename T>
class stream {
public:
    stream() {}
    explicit stream(const char *name) : name_(name ? name : "") {}

    // Blocking read in hardware; here an empty read is a testbench bug.
    T read() {
        if (q_.empty()) {
            std::fprintf(stderr, "hls_stream shim: read() on empty stream '%s'\n",
                         name_.c_str());
            std::abort();
        }
        T v = q_.front();
        q_.pop();
        return v;
    }
    void read(T &out) { out = read(); }

    // Non-blocking read: returns false (and leaves out untouched) if empty.
    bool read_nb(T &out) {
        if (q_.empty()) return false;
        out = q_.front();
        q_.pop();
        return true;
    }

    void write(const T &v) { q_.push(v); }
    bool write_nb(const T &v) { q_.push(v); return true; }

    bool empty() const { return q_.empty(); }
    bool full() const { return false; }  // unbounded in the shim
    std::size_t size() const { return q_.size(); }

private:
    stream(const stream &);             // non-copyable, like hls::stream
    stream &operator=(const stream &);
    std::queue<T> q_;
    std::string name_;
};

} // namespace hls

#endif // OES32_TB_HLS_STREAM_SHIM_H
