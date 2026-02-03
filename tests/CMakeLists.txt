add_executable(runTests LogAnalyzer_test.cpp)
target_link_libraries(runTests PRIVATE GTest::GTest GTest::Main LogAnalyzerLib)
add_test(NAME runTests COMMAND runTests)

add_executable(runFilterTests Filter_test.cpp)
target_link_libraries(runFilterTests PRIVATE GTest::GTest GTest::Main LogAnalyzerLib)
add_test(NAME runFilterTests COMMAND runFilterTests)
