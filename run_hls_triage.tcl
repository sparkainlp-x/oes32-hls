# =====================================================================
# Vitis HLS Automation Script for the OES-32 Telemetry Triage Stream
# Top:    oes32_triage_accelerator (oes32_triage_stream.cpp)
# Target: AMD Xilinx Zynq UltraScale+ RFSoC (ZCU111 Evaluation Board)
#
# Status: UNRUN. This script has never been executed; no synthesis,
# timing, II, latency or resource report exists for this kernel.
#
# Note: csim/cosim are not invoked here. The g++ testbench
# (test_oes32_triage.cpp, `make test-triage`) uses a testbench-only
# hls_stream shim and is the only verification that has been run.
# Vitis HLS supplies its own ap_int.h / ap_fixed.h / hls_stream.h, so
# tb/include_shim is deliberately NOT added to the include path.
# =====================================================================

open_project -reset oes32_triage_proj
set_top oes32_triage_accelerator
add_files oes32_triage_stream.cpp
open_solution -reset solution1 -flow_target vivado
set_part {xczu28dr-ffvg1517-2-e}
create_clock -period 10.0 -name default
csynth_design
export_design -format ip_catalog -rtl verilog -vendor "sparkainlp-x" -library "ip"
exit
