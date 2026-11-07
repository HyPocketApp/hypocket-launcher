cmake_minimum_required(VERSION 3.22.1)
project(hypocket_engine LANGUAGES C CXX)

set(GUEST_TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../guest-tests)

add_library(hypocket_engine SHARED
    hypocket_engine.cpp
    jni_bridge.cpp
    # Same workload as the x86-64 probe, built natively for comparison.
    ${GUEST_TESTS_DIR}/probe/probe_workload.c
)

target_include_directories(hypocket_engine PRIVATE ${GUEST_TESTS_DIR}/probe)
target_compile_options(hypocket_engine PRIVATE -Wall -Wextra -Werror)
# Match the optimization level of the x86-64 probe.
set_source_files_properties(${GUEST_TESTS_DIR}/probe/probe_workload.c PROPERTIES COMPILE_OPTIONS "-O2")
target_link_libraries(hypocket_engine PRIVATE log)
