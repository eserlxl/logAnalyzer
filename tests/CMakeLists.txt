# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Eser KUBALI

include(CTest)

# Default C++ standard for tests if not explicitly set
if(NOT DEFINED LOGANALYZER_TEST_CXX_STANDARD)
    set(LOGANALYZER_TEST_CXX_STANDARD 23) # Default to C++23
endif()

# Default test data directory if not explicitly set
if(NOT DEFINED LOGANALYZER_TEST_DATA_DIR)
    # Default to a 'data' directory in the project's source root
    set(LOGANALYZER_TEST_DATA_DIR "${CMAKE_SOURCE_DIR}/test_data" CACHE PATH "Directory for shared test data")
endif()

# Find all test source files ending with _test.cpp
set(TEST_SOURCES
    "src/config/CLIConfig_InputOutput.cpp"
    "src/config/CLIConfig_Filtering.cpp"
    "src/config/CLIConfig_Formatting.cpp"
    "src/config/CLIConfig_Analysis.cpp"
    "src/config/CLIConfig_Errors.cpp"
    "src/config/CLIConfig_Defaults.cpp"
    "src/config/CoreBasic.cpp"
    "src/config/CoreJson.cpp"
    "src/config/CoreValidation.cpp"
    "src/config/CoreFeatures.cpp"
    "src/core/LogParser.cpp"
    "src/core/LogTypes.cpp"
    "src/export/Core.cpp"
    "src/export/Json.cpp"

    "src/export/Csv.cpp"
    "src/export/Text.cpp"
    "src/export/FieldMapping.cpp"
    "src/export/Settings.cpp"
    "src/export/TestUtils.cpp"
    "src/analyzer/Core.cpp"
    "src/filter/LegacyFilterTests.cpp"
    "src/filter/FluentApiTests.cpp"
    "src/filter/EvaluationTests.cpp"
    "src/filter/LogicalNegationTests.cpp"
    "src/filter/Advanced.cpp"
    "src/filter/Version.cpp"
    "src/filter/IpAddress.cpp"
    "src/filter/CoreJsonBasic.cpp"
    "src/filter/CoreJsonCondition.cpp"
    "src/filter/CoreJsonExpression.cpp"
    "src/filter/CoreJsonRule.cpp"
    "src/filter/CoreUnitAdvanced.cpp"
    "src/filter/CoreUnitBasic.cpp"
    "src/filter/CoreUnitField.cpp"
    "src/filter/Iteration2Tests.cpp"
    "src/filter/Iteration13Tests.cpp"
    "src/filter/Iteration15Tests.cpp"
    "src/stats/Statistics.cpp"
    "src/utils/Time.cpp"
    "src/utils/Enum.cpp"
    "src/utils/String.cpp"
    "src/analyzer/Types_test.cpp"
    "src/analyzer/LogWriter_test.cpp"
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
    set_target_properties(${TEST_NAME} PROPERTIES CXX_STANDARD ${LOGANALYZER_TEST_CXX_STANDARD} CXX_STANDARD_REQUIRED ON)

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
        # Removed WORKING_DIRECTORY argument
    )

    # Removed: set_property(TEST ${TEST_NAME} PROPERTY LABELS ${TEST_LABEL}) - potentially interfering
endforeach()
