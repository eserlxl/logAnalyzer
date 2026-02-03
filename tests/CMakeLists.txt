include(CTest)

# Find all test source files ending with _test.cpp
file(GLOB TEST_SOURCES "*.cpp")

# Loop through each test source file
foreach(TEST_SOURCE IN LISTS TEST_SOURCES)
    get_filename_component(TEST_NAME "${TEST_SOURCE}" NAME_WE) # e.g., LogAnalyzer_test
    string(REPLACE "_test" "" TEST_LABEL "${TEST_NAME}")       # e.g., LogAnalyzer

    # Create an executable for each test file
    add_executable(${TEST_NAME} ${TEST_SOURCE})

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

    # Register tests with CTest using gtest_discover_tests
    gtest_discover_tests(${TEST_NAME}
        LABELS "${TEST_LABEL}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endforeach()
