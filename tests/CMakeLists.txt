cmake_minimum_required(VERSION 3.20)

find_package(GTest REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(runTests LogAnalyzer_test.cpp)
target_link_libraries(runTests PRIVATE GTest::GTest GTest::Main LogAnalyzerLib)

add_test(NAME runTests COMMAND runTests)
