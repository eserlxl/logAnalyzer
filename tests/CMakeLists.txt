add_executable(runTests LogAnalyzer_test.cpp)
target_link_libraries(runTests PRIVATE GTest::GTest GTest::Main LogAnalyzerLib)

add_test(NAME runTests COMMAND runTests)
