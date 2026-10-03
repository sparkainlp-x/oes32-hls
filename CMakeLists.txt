# =====================================================================
# CMake build for the oes32_triage Python extension (scikit-build-core).
# Compiles oes32_triage_stream.cpp UNCHANGED + pybind11 bindings, against
# the pinned Apache-2.0 Xilinx HLS_arbitrary_Precision_Types headers and
# the testbench-only hls_stream shim. Not used by Vitis HLS.
#
# Header lookup order:
#   1. -DOES32_AP_TYPES_DIR=<dir containing include/ap_fixed.h> or the
#      environment variable OES32_AP_TYPES_DIR;
#   2. the Makefile's .deps/HLS_arbitrary_Precision_Types-<sha> (if present);
#   3. download of the pinned commit tarball, verified by SHA-256.
# =====================================================================
cmake_minimum_required(VERSION 3.18)
project(oes32_triage LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(AP_TYPES_SHA 200a9aecaadf471592558540dc5a88256cbf880f)
set(AP_TYPES_URL
    https://github.com/Xilinx/HLS_arbitrary_Precision_Types/archive/${AP_TYPES_SHA}.tar.gz)
set(AP_TYPES_SHA256 4b9af967feb55c5086f31d3545c914dd4c0de5026e5c8554f4450f4340727092)

if(NOT OES32_AP_TYPES_DIR AND DEFINED ENV{OES32_AP_TYPES_DIR})
  set(OES32_AP_TYPES_DIR $ENV{OES32_AP_TYPES_DIR})
endif()
if(NOT OES32_AP_TYPES_DIR AND EXISTS
   ${CMAKE_CURRENT_SOURCE_DIR}/.deps/HLS_arbitrary_Precision_Types-${AP_TYPES_SHA}/include/ap_fixed.h)
  set(OES32_AP_TYPES_DIR
      ${CMAKE_CURRENT_SOURCE_DIR}/.deps/HLS_arbitrary_Precision_Types-${AP_TYPES_SHA})
endif()
if(NOT OES32_AP_TYPES_DIR)
  include(FetchContent)
  if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
  endif()
  # The archive has no CMakeLists.txt, so MakeAvailable only downloads,
  # verifies (SHA-256) and extracts it.
  FetchContent_Declare(ap_types URL ${AP_TYPES_URL} URL_HASH SHA256=${AP_TYPES_SHA256})
  FetchContent_MakeAvailable(ap_types)
  set(OES32_AP_TYPES_DIR ${ap_types_SOURCE_DIR})
endif()
if(NOT EXISTS ${OES32_AP_TYPES_DIR}/include/ap_fixed.h)
  message(FATAL_ERROR "ap_fixed.h not found under ${OES32_AP_TYPES_DIR}/include")
endif()
message(STATUS "oes32_triage: ap_* headers from ${OES32_AP_TYPES_DIR}")

find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(_core MODULE
    python/src/oes32_triage_module.cpp
    oes32_triage_stream.cpp)
target_include_directories(_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(_core SYSTEM PRIVATE
    ${OES32_AP_TYPES_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tb/include_shim)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  # Same rationale as the Makefile: warnings originate in the 2019
  # third-party ap_private.h, not in this repository's code.
  target_compile_options(_core PRIVATE -Wall -Wextra -Wno-unknown-pragmas
                                       -Wno-uninitialized -Wno-maybe-uninitialized)
endif()

install(TARGETS _core LIBRARY DESTINATION oes32_triage)
install(FILES python/oes32_triage_ref.py DESTINATION .)
# Apache-2.0 notice for the ap_* headers compiled into the extension.
if(EXISTS ${OES32_AP_TYPES_DIR}/LICENSE.TXT)
  install(FILES ${OES32_AP_TYPES_DIR}/LICENSE.TXT DESTINATION oes32_triage/third_party_licenses
          RENAME HLS_arbitrary_Precision_Types-LICENSE.txt)
endif()
