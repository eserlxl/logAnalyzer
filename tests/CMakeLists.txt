# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Eser KUBALI

include(CTest)

# Default C++ standard for tests if not explicitly set
if(NOT DEFINED LOGANALYZER_TEST_CXX_STANDARD)
    set(LOGANALYZER_TEST_CXX_STANDARD 23) # Default to C++23
endif()

# Default test data directory if not explicitly set
if(NOT DEFINED LOGANALYZER_TEST_DATA_DIR)
    # Default to tests/data in the project's source tree.
    set(LOGANALYZER_TEST_DATA_DIR "${CMAKE_SOURCE_DIR}/tests/data" CACHE PATH "Directory for shared test data")
endif()

# Find all test source files
set(TEST_SOURCES
    "config/CLI/InputOutput.cpp"
    "config/CLI/Filtering.cpp"
    "config/CLI/Formatting.cpp"
    "config/CLI/Analysis.cpp"
    "config/CLI/Errors.cpp"
    "config/CLI/Defaults.cpp"
    "config/Core/Basic.cpp"
    "config/Core/Json.cpp"
    "config/Core/Validation.cpp"
    "config/Core/Features.cpp"
    "config/Merge.cpp"
    "config/Utils.cpp"
    "core/Log/Parser.cpp"
    "core/Log/ParserUtilsTests.cpp"
    "core/Log/Types.cpp"
    "export/Core.cpp"
    "export/Json.cpp"
    "export/Csv.cpp"
    "export/Text.cpp"
    "export/FieldMapping.cpp"
    "export/Settings.cpp"
    "export/Utils.cpp"
    "analyzer/Core.cpp"
    "filter/Legacy.cpp"
    "filter/FluentApi.cpp"
    "filter/Evaluation.cpp"
    "filter/LogicalNegation.cpp"
    "filter/Advanced.cpp"
    "filter/Version.cpp"
    "filter/IpAddress.cpp"
    "filter/CoreJson/Basic_Serialization.cpp"
    "filter/CoreJson/Basic_Deserialization.cpp"
    "filter/CoreJson/Condition_Serialization.cpp"
    "filter/CoreJson/Condition_Deserialization.cpp"
    "filter/CoreJson/Expression.cpp"
    "filter/CoreJson/Rule.cpp"
    "filter/Core/Unit/Advanced.cpp"
    "filter/Core/Unit/Basic.cpp"
    "filter/Core/Unit/Field.cpp"
    "filter/Iteration2.cpp"
    "filter/Iteration13.cpp"
    "filter/Iteration15.cpp"
    "stats/Core.cpp"
    "utils/Time.cpp"
    "utils/Enum.cpp"
    "utils/String.cpp"
    "analyzer/Types.cpp"
    "analyzer/LogWriter.cpp"
    "analyzer/LogWriterCorrectness.cpp"
)

# Loop through each test source file
foreach(TEST_SOURCE IN LISTS TEST_SOURCES)
    # Generate a unique test name based on the path to avoid collisions (e.g. export/Core.cpp vs filter/Core.cpp)
    # "export/Core.cpp" -> "export_Core"
    string(REPLACE "/" "_" TEST_NAME_FULL "${TEST_SOURCE}")
    string(REPLACE ".cpp" "" TEST_NAME "${TEST_NAME_FULL}")
    
    set(TEST_LABEL "${TEST_NAME}")

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
endforeach()
