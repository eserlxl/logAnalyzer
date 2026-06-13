# cmake/compiler_settings.cmake
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Eser KUBALI

# Options applied to test executables' ctest environment when ASan is enabled.
# Consumed in tests/CMakeLists.txt via set_tests_properties(... ENVIRONMENT ...),
# which is the property ctest actually reads (a target ENVIRONMENT property is not).
set(LOGANALYZER_TEST_ASAN_OPTIONS
    "quarantine_size_mb=64:thread_local_quarantine_size_kb=1024:malloc_context_size=10:symbolize=0:detect_leaks=0")

function(loganalyzer_apply_sanitizers_and_coverage target)
    if(ENABLE_ASAN)
        message(STATUS "Enabling AddressSanitizer (ASan) for target: ${target}")
        target_compile_options(${target} PUBLIC -fsanitize=address)
        target_link_options(${target} PUBLIC -fsanitize=address)
        # ASAN_OPTIONS for test executables is applied as a ctest test property in
        # tests/CMakeLists.txt (see LOGANALYZER_TEST_ASAN_OPTIONS); a target ENVIRONMENT
        # property set here would not be read by ctest.
    endif()

    if(ENABLE_UBSAN)
        message(STATUS "Enabling UndefinedBehaviorSanitizer (UBSan) for target: ${target}")
        target_compile_options(${target} PUBLIC -fsanitize=undefined)
        target_link_options(${target} PUBLIC -fsanitize=undefined)
    endif()

    if(ENABLE_COVERAGE)
        message(STATUS "Enabling code coverage instrumentation for target: ${target}")
        target_compile_options(${target} PUBLIC -fprofile-arcs -ftest-coverage)
        target_link_options(${target} PUBLIC -fprofile-arcs -ftest-coverage)
    endif()
endfunction()
