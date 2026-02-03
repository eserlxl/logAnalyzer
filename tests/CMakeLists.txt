include(CTest)

# Define a macro to add GTest tests with advanced features
macro(loganalyzer_gtest_discover_tests TEST_NAME)
    cmake_parse_arguments(
        "LAGTD"                               # Prefix for arguments (LOGANALYZER_GTEST_DISCOVER_TESTS)
        ""                                    # Options (boolean flags)
        "WORKING_DIRECTORY;TEST_DATA_SUBDIR"  # One-value arguments
        "LABELS;PROPERTIES;CATEGORIES;ENVIRONMENT;TEST_DATA_FILES" # Multi-value arguments
        ${ARGN}
    )

    # Determine the actual working directory for this specific test.
    # The design specifies a unique, temporary location for true isolation.
    set(TEST_ACTUAL_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/Testing/Temporary/${TEST_NAME}_working_dir")

    # --- Handle TEST_DATA_FILES and TEST_DATA_SUBDIR ---
    set(TEST_COMMAND_PRELUDE "")
    if(LAGTD_TEST_DATA_FILES)
        set(SETUP_SCRIPT_CONTENT "")
        list(APPEND SETUP_SCRIPT_CONTENT "cmake_minimum_required(VERSION 3.15)\n")
        list(APPEND SETUP_SCRIPT_CONTENT "message(STATUS \"Setting up test data for ${TEST_NAME}\")\n")

        # Create the test's isolated working directory
        list(APPEND SETUP_SCRIPT_CONTENT "file(MAKE_DIRECTORY \"${TEST_ACTUAL_WORKING_DIRECTORY}\")\n")

        set(DATA_DESTINATION_DIR "${TEST_ACTUAL_WORKING_DIRECTORY}")
        if(LAGTD_TEST_DATA_SUBDIR)
            set(DATA_DESTINATION_DIR "${TEST_ACTUAL_WORKING_DIRECTORY}/${LAGTD_TEST_DATA_SUBDIR}")
            list(APPEND SETUP_SCRIPT_CONTENT "file(MAKE_DIRECTORY \"${DATA_DESTINATION_DIR}\")\n")
        endif()

        foreach(DATA_FILE IN LISTS LAGTD_TEST_DATA_FILES)
            if(NOT EXISTS "${DATA_FILE}")
                message(FATAL_ERROR "TEST_DATA_FILE specified for ${TEST_NAME} does not exist: ${DATA_FILE}")
            endif()
            list(APPEND SETUP_SCRIPT_CONTENT "message(STATUS \"Copying data file: ${DATA_FILE} to ${DATA_DESTINATION_DIR}\")\n")
            list(APPEND SETUP_SCRIPT_CONTENT "file(COPY \"${DATA_FILE}\" DESTINATION \"${DATA_DESTINATION_DIR}\")\n")
        endforeach()

        # Write the setup script to a temporary file
        set(SETUP_SCRIPT_PATH "${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}_setup_data.cmake")
        file(WRITE "${SETUP_SCRIPT_PATH}" "${SETUP_SCRIPT_CONTENT}")

        # The command prelude will execute this setup script
        set(TEST_COMMAND_PRELUDE "${CMAKE_COMMAND}" "-P" "${SETUP_SCRIPT_PATH}")
    endif()

    # --- Construct ADD_TEST_COMMAND for gtest_discover_tests ---
    # The ADD_TEST_COMMAND argument needs to be the actual command that runs the test executable.
    # gtest_discover_tests will wrap this in a script that also handles environment variables.
    set(ACTUAL_TEST_EXECUTABLE "${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}") # The full path to the executable

    # If we have a prelude (for data copying), we need to ensure the executable is part of that command chain.
    # gtest_discover_tests will automatically add the executable to the end of ADD_TEST_COMMAND,
    # but only if ADD_TEST_COMMAND is the executable itself.
    # If ADD_TEST_COMMAND is a script, that script needs to execute the test executable.
    # So, the setup script for data copying must *also* execute the test executable.

    set(COMMAND_TO_EXECUTE_TEST_IN_SETUP_SCRIPT "\"${ACTUAL_TEST_EXECUTABLE}\"")
    if(TEST_COMMAND_PRELUDE)
        # Modify the setup script to include executing the test executable
        set(SETUP_SCRIPT_PATH "${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}_setup_data.cmake")
        file(APPEND "${SETUP_SCRIPT_PATH}" "message(STATUS \"Executing test ${TEST_NAME}\")\n")
        file(APPEND "${SETUP_SCRIPT_PATH}" "execute_process(COMMAND ${COMMAND_TO_EXECUTE_TEST_IN_SETUP_SCRIPT} RESULT_VARIABLE _ret_code WORKING_DIRECTORY \"${TEST_ACTUAL_WORKING_DIRECTORY}\")\n")
        file(APPEND "${SETUP_SCRIPT_PATH}" "if(NOT \"\${_ret_code}\" STREQUAL \"0\")\n")
        file(APPEND "${SETUP_SCRIPT_PATH}" "  message(FATAL_ERROR \"Test ${TEST_NAME} failed with code \${_ret_code}\")\n")
        file(APPEND "${SETUP_SCRIPT_PATH}" "endif()\n")
        set(FINAL_ADD_TEST_COMMAND ${TEST_COMMAND_PRELUDE})
    else()
        set(FINAL_ADD_TEST_COMMAND "${TEST_NAME}") # Pass target name, gtest_discover_tests will find its path
    endif()

    # --- Combine LABELS and CATEGORIES ---
    set(ALL_LABELS "")
    if(LAGTD_LABELS)
        list(APPEND ALL_LABELS ${LAGTD_LABELS})
    endif()
    if(LAGTD_CATEGORIES)
        list(APPEND ALL_LABELS ${LAGTD_CATEGORIES})
    endif()

    # --- Call gtest_discover_tests ---
    gtest_discover_tests(${TEST_NAME}
        ADD_TEST_COMMAND ${FINAL_ADD_TEST_COMMAND}
        # Note: WORKING_DIRECTORY here applies to the *discovered tests*, not the executable's build directory.
        # It's where the test executable is run from by CTest.
        # If ADD_TEST_COMMAND is a script, its WORKING_DIRECTORY must be set inside the script (done above).
        WORKING_DIRECTORY "${TEST_ACTUAL_WORKING_DIRECTORY}"
        LABELS "${ALL_LABELS}"
        PROPERTIES ${LAGTD_PROPERTIES}
        ENVIRONMENT ${LAGTD_ENVIRONMENT}
    )
endmacro()

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
file(GLOB TEST_SOURCES "*.cpp")

# Loop through each test source file
foreach(TEST_SOURCE IN LISTS TEST_SOURCES)
    get_filename_component(TEST_NAME "${TEST_SOURCE}" NAME_WE) # e.g., LogAnalyzer_test
    string(REPLACE "_test" "" TEST_LABEL "${TEST_NAME}")       # e.g., LogAnalyzer

    # Create an executable for each test file
    add_executable(${TEST_NAME} ${TEST_SOURCE})

    # Include test-specific options file if it exists
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

    # Register tests with CTest using the enhanced loganalyzer_gtest_discover_tests macro
    loganalyzer_gtest_discover_tests(${TEST_NAME}
        LABELS "${TEST_LABEL}"
    )
endforeach()
