# cmake/LogAnalyzerCompilerSettings.cmake
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Eser KUBALI

function(loganalyzer_apply_sanitizers_and_coverage target)
    if(ENABLE_ASAN)
        message(STATUS "Enabling AddressSanitizer (ASan) for target: ${target}")
        target_compile_options(${target} PUBLIC -fsanitize=address)
        target_link_options(${target} PUBLIC -fsanitize=address)
        # Apply ASAN_OPTIONS environment for test executables
        if(TARGET ${target} AND "${target}" MATCHES "test_") # Heuristic: targets starting with test_
             set_target_properties(${target} PROPERTIES
                ENVIRONMENT "ASAN_OPTIONS=quarantine_size_mb=64:thread_local_quarantine_size_kb=1024:malloc_context_size=10:symbolize=0:detect_leaks=0"
            )
        endif()
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
