# =====================================================================
# OES-32 HLS — g++ testbench build (no AMD/Xilinx tools required)
#
#   make test            # all three testbenches
#   make test-membrane   # test_oes32_hls (AXI4-Lite membrane checks)
#   make test-triage     # test_oes32_triage (streaming triage kernel)
#   make test-edges      # tb/test_kernel_edges.cpp (edge cases, both kernels)
#   make py-install      # build + install the oes32_triage Python extension
#   make py-test         # pytest suite (needs the extension; see README)
#   make notebook        # execute notebooks/oes32_triage_explainer.ipynb
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
# SHA-256 of $(AP_TYPES_REPO)/archive/$(AP_TYPES_SHA).tar.gz (same value as CMakeLists.txt)
AP_TYPES_TGZ_SHA256 := 4b9af967feb55c5086f31d3545c914dd4c0de5026e5c8554f4450f4340727092
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

.PHONY: all test test-membrane test-triage test-edges deps clean distclean py-install py-test notebook

all: test

test: test-membrane test-triage test-edges

deps: $(AP_TYPES_STAMP)

$(AP_TYPES_STAMP):
	mkdir -p .deps
	curl -fsSL -o .deps/ap_types.tar.gz $(AP_TYPES_REPO)/archive/$(AP_TYPES_SHA).tar.gz
	echo "$(AP_TYPES_TGZ_SHA256)  .deps/ap_types.tar.gz" | sha256sum -c -
	tar -xzf .deps/ap_types.tar.gz -C .deps
	rm -f .deps/ap_types.tar.gz
	test -f $@

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_oes32_hls: test_oes32_hls.cpp oes32_hls_top.cpp oes32_hls_top.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ test_oes32_hls.cpp oes32_hls_top.cpp

$(BUILD)/test_oes32_triage: test_oes32_triage.cpp oes32_triage_stream.cpp oes32_triage_stream.h tb/include_shim/hls_stream.h $(AP_TYPES_STAMP) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(TRIAGE_FLAGS) $(TRIAGE_INC) -o $@ test_oes32_triage.cpp oes32_triage_stream.cpp

$(BUILD)/test_kernel_edges: tb/test_kernel_edges.cpp oes32_hls_top.cpp oes32_hls_top.h oes32_triage_stream.cpp oes32_triage_stream.h tb/include_shim/hls_stream.h $(AP_TYPES_STAMP) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(TRIAGE_FLAGS) $(TRIAGE_INC) -o $@ tb/test_kernel_edges.cpp oes32_hls_top.cpp oes32_triage_stream.cpp

test-edges: $(BUILD)/test_kernel_edges
	./$(BUILD)/test_kernel_edges

test-membrane: $(BUILD)/test_oes32_hls
	./$(BUILD)/test_oes32_hls

test-triage: $(BUILD)/test_oes32_triage
	./$(BUILD)/test_oes32_triage $(TRIAGE_CSV)

# --- Python bindings (pybind11; real C++ kernel) --------------------------
PYTHON ?= python3

py-install: deps
	$(PYTHON) -m pip install -v ".[test]"

py-test:
	OES32_REQUIRE_EXT=1 $(PYTHON) -m pytest -v tests

notebook:
	$(PYTHON) scripts/check_notebook.py notebooks/oes32_triage_explainer.ipynb

clean:
	rm -rf $(BUILD)

distclean: clean
	rm -rf .deps
