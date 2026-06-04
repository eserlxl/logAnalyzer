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
    "config/core/json/deserialization.cpp"
    "config/core/json/serialization.cpp"
    "config/core/validation.cpp"
    "config/core/features.cpp"
    "config/merge.cpp"
    "config/utils.cpp"
    "core/log/parser.cpp"
    "core/log/json_parser.cpp"
    "core/log/parser_utils.cpp"
    "core/log/types.cpp"
    "export/core.cpp"
    "export/json.cpp"
    "export/ndjson.cpp"
    "export/csv.cpp"
    "export/text.cpp"
    "export/field_mapping.cpp"
    "export/settings.cpp"
    "export/utils.cpp"
    "analyzer/core.cpp"
    "analyzer/stream.cpp"
    "analyzer/filter.cpp"
    "analyzer/export.cpp"
    "analyzer/filter_logic/sorting.cpp"
    "analyzer/filter_logic/expressions.cpp"
    "analyzer/filter_logic/criteria.cpp"
    "filter/legacy.cpp"
    "filter/fluent_api.cpp"
    "filter/evaluation.cpp"
    "filter/evaluation_extended.cpp"
    "filter/logical_negation.cpp"
    "filter/advanced.cpp"
    "filter/version.cpp"
    "filter/ip_address.cpp"
    "filter/core_json/basic/serialization.cpp"
    "filter/core_json/basic/serialization_advanced.cpp"
    "filter/core_json/basic/deserialization.cpp"
    "filter/core_json/condition/serialization.cpp"
    "filter/core_json/condition/deserialization.cpp"
    "filter/core_json/condition/factory_function.cpp"
    "filter/core_json/expression.cpp"
    "filter/core_json/rule.cpp"
    "filter/core/unit/advanced.cpp"
    "filter/core/unit/basic.cpp"
    "filter/core/unit/field.cpp"
    "filter/iteration/2.cpp"
    "filter/iteration/13.cpp"
    "filter/iteration/15.cpp"
    "stats/core.cpp"
    "utils/time.cpp"
    "utils/enum.cpp"
    "utils/string.cpp"
    "utils/dedup.cpp"
    "analyzer/types.cpp"
    "analyzer/log_writer.cpp"
    "analyzer/log_writer_correctness.cpp"
    "config/cli/new_feature.cpp"
)

# Loop through each test source file
foreach(TEST_SOURCE IN LISTS TEST_SOURCES)
    # Generate a unique test name based on the path to avoid collisions (e.g. export/Core.cpp vs filter/Core.cpp)
    # "export/Core.cpp" -> "export_Core"
    get_filename_component(FILE_NAME_WE "${TEST_SOURCE}" NAME_WE) # e.g., "input_output" from "config/cli/input_output.cpp"
    get_filename_component(PARENT_DIR "${TEST_SOURCE}" DIRECTORY) # e.g., "config/cli"
    string(REPLACE "/" "_" SANITIZED_PARENT_DIR "${PARENT_DIR}") # e.g., "config_cli"
    if(SANITIZED_PARENT_DIR) # If it's not in the root of tests/
        set(TEST_NAME "${SANITIZED_PARENT_DIR}_${FILE_NAME_WE}")
    else()
        set(TEST_NAME "${FILE_NAME_WE}")
    endif()
    


    # Create an executable for each test file
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    loganalyzer_apply_sanitizers_and_coverage(${TEST_NAME})

    # Add project root and tests/include to include path for 'tests/...' includes
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/support)

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
        GTest::gtest
        GTest::gtest_main
        LogAnalyzerLib
        # Add GMock if enabled
        $<$<BOOL:${ENABLE_GMOCK}>:GMock::gmock GMock::gmock_main>
    )

    # Register the test directly with add_test
    # Create a unique working directory for each test for isolation
    set(TEST_EXECUTION_DIR "${CMAKE_BINARY_DIR}/Testing/Temporary/${TEST_NAME}_working_dir")
    file(MAKE_DIRECTORY "${TEST_EXECUTION_DIR}")

    add_test(NAME ${TEST_NAME}
        COMMAND $<TARGET_FILE:${TEST_NAME}>
    )
endforeach()

