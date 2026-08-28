# =====================================================================
# Vitis HLS Automation Script for OES-32 Membrane Shield Accelerator
# Target: AMD Xilinx Zynq UltraScale+ RFSoC (ZCU111 Evaluation Board)
# =====================================================================

open_project -reset oes32_hls_proj
set_top oes32_membrane_accelerator
add_files oes32_hls_top.cpp
open_solution -reset solution1 -flow_target vivado
set_part {xczu28dr-ffvg1517-2-e}
create_clock -period 10.0 -name default
csynth_design
export_design -format ip_catalog -rtl verilog -vendor "sparkainlp-x" -library "ip"
exit
