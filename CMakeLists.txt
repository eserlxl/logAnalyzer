cmake_minimum_required(VERSION 3.14)
project(logAnalyzer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# nlohmann_json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# CLI11
FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.3.2
)
FetchContent_MakeAvailable(cli11)

# GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

if(TARGET gtest)
    add_library(GTest::GTest ALIAS gtest)
    add_library(GTest::Main ALIAS gtest_main)
endif()

# Library source files
set(LIB_SOURCES
    src/analyzer/LogReader.cpp
    src/analyzer/LogWriter.cpp
    src/analyzer/Stats.cpp
    src/analyzer/Core.cpp
    src/analyzer/IO.cpp
    src/analyzer/Export.cpp
    src/analyzer/Filter.cpp
    src/config/Core.cpp
    src/config/Utils.cpp
    src/config/CLIConfig.cpp
    src/core/LogParser.cpp
    src/core/JsonLogParser.cpp
    src/core/LogTypes.cpp
    src/export/Exporter.cpp
    src/filter/EnumStringConversions.cpp
    src/filter/Expression.cpp
    src/filter/ConcreteFilters.cpp
    src/filter/Parser.cpp
    src/stats/Statistics.cpp
    src/utils/IpAddress.cpp
    src/utils/Version.cpp
    src/utils/Time.cpp
    src/utils/String.cpp
    src/utils/Core.cpp
)

add_library(LogAnalyzerLib ${LIB_SOURCES})
target_include_directories(LogAnalyzerLib PUBLIC include)
target_link_libraries(LogAnalyzerLib PUBLIC nlohmann_json::nlohmann_json CLI11::CLI11)

# Main executable
add_executable(logAnalyzer src/main.cpp)
target_link_libraries(logAnalyzer PRIVATE LogAnalyzerLib)

# Installation
include(GNUInstallDirs)
install(TARGETS logAnalyzer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS LogAnalyzerLib LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Enable testing
enable_testing()
add_subdirectory(tests)
