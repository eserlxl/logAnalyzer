# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Eser KUBALI

include(CTest)

# Default C++ standard for tests if not explicitly set
if(NOT DEFINED LOGANALYZER_TEST_CXX_STANDARD)
    set(LOGANALYZER_TEST_CXX_STANDARD 23) # Default to C++23
endif()

# Default test data directory if not explicitly set
if(NOT DEFINED LOGANALYZER_TEST_DATA_DIR)
    set(_default_test_data_dir "${CMAKE_SOURCE_DIR}/tests/data")
    set(_legacy_test_data_dir "${CMAKE_SOURCE_DIR}/test_data")

    if(EXISTS "${_default_test_data_dir}")
        set(LOGANALYZER_TEST_DATA_DIR "${_default_test_data_dir}" CACHE PATH "Directory for shared test data")
    elseif(EXISTS "${_legacy_test_data_dir}")
        # Keep compatibility with historical layout in case fixtures are moved.
        set(LOGANALYZER_TEST_DATA_DIR "${_legacy_test_data_dir}" CACHE PATH "Directory for shared test data")
        message(WARNING "Using legacy test data directory '${_legacy_test_data_dir}'. Prefer '${_default_test_data_dir}'.")
    else()
        message(FATAL_ERROR
            "No test data directory found. Expected '${_default_test_data_dir}'"
            " (or legacy '${_legacy_test_data_dir}')."
            " Set LOGANALYZER_TEST_DATA_DIR explicitly.")
    endif()
endif()

if(NOT EXISTS "${LOGANALYZER_TEST_DATA_DIR}")
    set(_default_test_data_dir "${CMAKE_SOURCE_DIR}/tests/data")
    if(EXISTS "${_default_test_data_dir}")
        message(WARNING
            "LOGANALYZER_TEST_DATA_DIR='${LOGANALYZER_TEST_DATA_DIR}' does not exist."
            " Resetting to '${_default_test_data_dir}'.")
        set(LOGANALYZER_TEST_DATA_DIR "${_default_test_data_dir}" CACHE PATH "Directory for shared test data" FORCE)
    else()
        message(FATAL_ERROR
            "LOGANALYZER_TEST_DATA_DIR='${LOGANALYZER_TEST_DATA_DIR}' does not exist."
            " Update LOGANALYZER_TEST_DATA_DIR or restore the test fixtures directory.")
    endif()
endif()

# Find all test source files
set(TEST_SOURCES
    "config/cli/input_output.cpp"
    "config/cli/filtering.cpp"
    "config/cli/formatting.cpp"
    "config/cli/analysis.cpp"
    "config/cli/errors.cpp"
    "config/cli/defaults.cpp"
    "config/core/basic.cpp"
    "config/core/json.cpp"
    "config/core/validation.cpp"
    "config/core/features.cpp"
    "config/merge.cpp"
    "config/utils.cpp"
    "core/log/parser.cpp"
    "core/log/json_parser.cpp"
    "core/log/parser_utils_tests.cpp"
    "core/log/types.cpp"
    "export/core.cpp"
    "export/json.cpp"
    "export/csv.cpp"
    "export/text.cpp"
    "export/field_mapping.cpp"
    "export/settings.cpp"
    "export/utils.cpp"
    "analyzer/core.cpp"
    "filter/legacy.cpp"
    "filter/fluent_api.cpp"
    "filter/evaluation.cpp"
    "filter/logical_negation.cpp"
    "filter/advanced.cpp"
    "filter/version.cpp"
    "filter/ip_address.cpp"
    "filter/core_json/basic_serialization.cpp"
    "filter/core_json/basic_deserialization.cpp"
    "filter/core_json/condition_serialization.cpp"
    "filter/core_json/condition_deserialization.cpp"
    "filter/core_json/expression.cpp"
    "filter/core_json/rule.cpp"
    "filter/core/unit/advanced.cpp"
    "filter/core/unit/basic.cpp"
    "filter/core/unit/field.cpp"
    "filter/iteration/iteration_2.cpp"
    "filter/iteration/iteration_13.cpp"
    "filter/iteration/iteration_15.cpp"
    "stats/core.cpp"
    "utils/time.cpp"
    "utils/enum.cpp"
    "utils/string.cpp"
    "analyzer/types.cpp"
    "analyzer/log_writer.cpp"
    "analyzer/log_writer_correctness.cpp"
)

# Loop through each test source file
foreach(TEST_SOURCE IN LISTS TEST_SOURCES)
    # Generate a unique test name based on the path to avoid collisions (e.g. export/Core.cpp vs filter/Core.cpp)
    # "export/Core.cpp" -> "export_Core"
    string(REPLACE "/" "_" TEST_NAME_FULL "${TEST_SOURCE}")
    string(REPLACE ".cpp" "" TEST_NAME "${TEST_NAME_FULL}")
    


    # Create an executable for each test file
    add_executable(${TEST_NAME} ${TEST_SOURCE})

    # Add project root and tests/include to include path for 'tests/...' includes
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_NAME}_options.cmake")
        message(STATUS "Including test-specific options for ${TEST_NAME} from ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_NAME}_options.cmake")
        include("${CMAKE_CURRENT_SOURCE_DIR}/${TEST_NAME}_options.cmake")
    endif()

    # Set C++ standard for the test executable
    target_compile_features(${TEST_NAME} PRIVATE cxx_std_${LOGANALYZER_TEST_CXX_STANDARD})
    set_target_properties(${TEST_NAME} PROPERTIES
        CXX_STANDARD ${LOGANALYZER_TEST_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )

    # Pass LOGANALYZER_TEST_DATA_DIR as a compile definition
    target_compile_definitions(${TEST_NAME} PRIVATE "LOGANALYZER_TEST_DATA_DIR=\"${LOGANALYZER_TEST_DATA_DIR}\"")

    # Link common libraries
    target_link_libraries(${TEST_NAME} PRIVATE
        GTest::GTest
        GTest::Main
        LogAnalyzerLib
        # Add GMock if enabled
        $<$<BOOL:${ENABLE_GMOCK}>:GMock::gmock GMock::gmock_main>
    )

    # Add build options (sanitizers, coverage)
    target_compile_options(${TEST_NAME} PRIVATE
        $<$<BOOL:${ENABLE_ASAN}>:-fsanitize=address -fno-omit-frame-pointer>
        $<$<BOOL:${ENABLE_UBSAN}>:-fsanitize=undefined>
        $<$<BOOL:${ENABLE_COVERAGE}>:--coverage>
    )
    target_link_options(${TEST_NAME} PRIVATE
        $<$<BOOL:${ENABLE_ASAN}>:-fsanitize=address>
        $<$<BOOL:${ENABLE_UBSAN}>:-fsanitize=undefined>
        $<$<BOOL:${ENABLE_COVERAGE}>:--coverage>
    )

    # Register the test directly with add_test
    # Create a unique working directory for each test for isolation
    set(TEST_EXECUTION_DIR "${CMAKE_BINARY_DIR}/Testing/Temporary/${TEST_NAME}_working_dir")
    file(MAKE_DIRECTORY "${TEST_EXECUTION_DIR}")

    add_test(NAME ${TEST_NAME}
        COMMAND $<TARGET_FILE:${TEST_NAME}>
    )

    if(ENABLE_ASAN)
        # Some constrained CI/sandbox runtimes execute tests under ptrace-like supervision
        # where LeakSanitizer cannot operate. Keep ASAN checks active while disabling LSan.
        set_tests_properties(${TEST_NAME} PROPERTIES
            ENVIRONMENT "ASAN_OPTIONS=quarantine_size_mb=64:thread_local_quarantine_size_kb=1024:malloc_context_size=10:symbolize=0:detect_leaks=0"
        )
    endif()
endforeach()
