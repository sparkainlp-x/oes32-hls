# =====================================================================
# OES-32 HLS — g++ testbench build (no AMD/Xilinx tools required)
#
#   make test            # both testbenches
#   make test-membrane   # test_oes32_hls (AXI4-Lite membrane checks)
#   make test-triage     # test_oes32_triage (streaming triage kernel)
#   make clean
#
# The triage kernel needs ap_int.h / ap_fixed.h. They are fetched from
# the open-source Xilinx HLS_arbitrary_Precision_Types repository
# (Apache License 2.0) at a pinned commit into .deps/ and are NOT
# vendored in this repository. hls_stream.h is replaced, for g++ only,
# by the testbench shim in tb/include_shim/. See docs/THIRD_PARTY.md.
# =====================================================================

CXX      ?= g++
CXXFLAGS ?= -std=c++14 -O2 -Wall -Wextra -Wno-unknown-pragmas

AP_TYPES_REPO := https://github.com/Xilinx/HLS_arbitrary_Precision_Types
AP_TYPES_SHA  := 200a9aecaadf471592558540dc5a88256cbf880f
AP_TYPES_DIR  := .deps/HLS_arbitrary_Precision_Types-$(AP_TYPES_SHA)
AP_TYPES_STAMP := $(AP_TYPES_DIR)/include/ap_fixed.h

BUILD     := build
TRIAGE_CSV := $(BUILD)/triage_tau_trace.csv

# ap_* headers and the shim are -isystem so their warnings do not mask ours.
# -Wno-(maybe-)uninitialized: GCC >= 12 reports warnings from
# inside the third-party ap_private.h default constructors (inlined into
# our code, so -isystem does not hide them). Applied to the triage build only.
TRIAGE_INC   := -isystem $(AP_TYPES_DIR)/include -isystem tb/include_shim -I.
TRIAGE_FLAGS := -Wno-uninitialized -Wno-maybe-uninitialized

.PHONY: all test test-membrane test-triage deps clean distclean

all: test

test: test-membrane test-triage

deps: $(AP_TYPES_STAMP)

$(AP_TYPES_STAMP):
	mkdir -p .deps
	curl -fsSL $(AP_TYPES_REPO)/archive/$(AP_TYPES_SHA).tar.gz | tar -xz -C .deps
	test -f $@

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_oes32_hls: test_oes32_hls.cpp oes32_hls_top.cpp oes32_hls_top.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ test_oes32_hls.cpp oes32_hls_top.cpp

$(BUILD)/test_oes32_triage: test_oes32_triage.cpp oes32_triage_stream.cpp oes32_triage_stream.h tb/include_shim/hls_stream.h $(AP_TYPES_STAMP) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(TRIAGE_FLAGS) $(TRIAGE_INC) -o $@ test_oes32_triage.cpp oes32_triage_stream.cpp

test-membrane: $(BUILD)/test_oes32_hls
	./$(BUILD)/test_oes32_hls

test-triage: $(BUILD)/test_oes32_triage
	./$(BUILD)/test_oes32_triage $(TRIAGE_CSV)

clean:
	rm -rf $(BUILD)

distclean: clean
	rm -rf .deps
