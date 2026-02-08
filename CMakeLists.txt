# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Eser KUBALI

cmake_minimum_required(VERSION 3.14)
project(logAnalyzer VERSION 1.0.0 LANGUAGES CXX)

# Prevent in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Please create a separate build directory (e.g., mkdir build && cd build && cmake ..).")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(LOGANALYZER_FETCH_DEPS "Fetch third-party dependencies with FetchContent when not found locally" ON)

include(FetchContent)
include(CMakePackageConfigHelpers)

# nlohmann_json
find_package(nlohmann_json 3.11.3 QUIET CONFIG)
if(NOT TARGET nlohmann_json::nlohmann_json)
    if(LOGANALYZER_FETCH_DEPS)
        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG v3.11.3
        )
        FetchContent_MakeAvailable(nlohmann_json)
    else()
        message(FATAL_ERROR "nlohmann_json was not found. Install it locally or enable LOGANALYZER_FETCH_DEPS.")
    endif()
endif()

# CLI11
find_package(CLI11 2.6.0 QUIET CONFIG)
if(NOT TARGET CLI11::CLI11)
    if(LOGANALYZER_FETCH_DEPS)
        FetchContent_Declare(
            cli11
            GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
            GIT_TAG v2.6.0
        )
        FetchContent_MakeAvailable(cli11)
    else()
        message(FATAL_ERROR "CLI11 was not found. Install it locally or enable LOGANALYZER_FETCH_DEPS.")
    endif()
endif()

# GoogleTest
find_package(GTest 1.14.0 QUIET CONFIG)
if(NOT TARGET GTest::gtest AND NOT TARGET gtest)
    if(LOGANALYZER_FETCH_DEPS)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0
        )
        # For Windows: Prevent overriding the parent project's compiler/linker settings
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    else()
        message(FATAL_ERROR "GTest was not found. Install it locally or enable LOGANALYZER_FETCH_DEPS.")
    endif()
endif()
include(GoogleTest)

if(TARGET GTest::gtest AND NOT TARGET GTest::GTest)
    add_library(GTest::GTest ALIAS GTest::gtest)
endif()
if(TARGET GTest::gtest_main AND NOT TARGET GTest::Main)
    add_library(GTest::Main ALIAS GTest::gtest_main)
endif()
if(TARGET gtest AND NOT TARGET GTest::GTest)
    add_library(GTest::GTest ALIAS gtest)
endif()
if(TARGET gtest_main AND NOT TARGET GTest::Main)
    add_library(GTest::Main ALIAS gtest_main)
endif()

if(TARGET gtest)
    if(NOT MSVC)
        target_compile_options(gtest PRIVATE -Wno-character-conversion)
        if(TARGET gtest_main)
            target_compile_options(gtest_main PRIVATE -Wno-character-conversion)
        endif()
    endif()
endif()

# Library source files
set(LIB_SOURCES
    src/analyzer/Log/Reader.cpp
    src/analyzer/Log/Writer.cpp
    src/analyzer/Log/Loader.cpp
    src/analyzer/Stats.cpp
    src/analyzer/Core.cpp
    src/analyzer/IO.cpp
    src/analyzer/Export.cpp
    src/analyzer/Filter.cpp
    src/analyzer/Streaming.cpp
    src/config/Core.cpp
    src/config/CoreJson.cpp
    src/config/CoreValidation.cpp
    src/config/Utils.cpp
    src/config/CLI.cpp
    src/core/Log/Parser.cpp
    src/core/Log/ParserUtils.cpp
    src/core/Log/JsonParser.cpp
    src/core/Log/Types.cpp
    src/export/Core.cpp
    src/export/Json.cpp
    src/filter/EnumStringConversions.cpp
    src/filter/Expression.cpp
    src/filter/ConcreteFilters.cpp
    src/filter/ConditionEvaluation.cpp
    src/filter/Parser.cpp
    src/stats/Core.cpp
    src/utils/IpAddress.cpp
    src/utils/Version.cpp
    src/utils/Time.cpp
    src/utils/String.cpp
    src/utils/Core.cpp
)

add_library(LogAnalyzerLib ${LIB_SOURCES})
target_include_directories(LogAnalyzerLib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(LogAnalyzerLib
    PUBLIC
        $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
    PRIVATE
        $<BUILD_INTERFACE:CLI11::CLI11>
)

if(MSVC)
    target_compile_options(LogAnalyzerLib PRIVATE /W4 /WX)
else()
    target_compile_options(LogAnalyzerLib PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Main executable
add_executable(logAnalyzer src/main.cpp)
target_link_libraries(logAnalyzer PRIVATE LogAnalyzerLib)

if(MSVC)
    target_compile_options(logAnalyzer PRIVATE /W4 /WX)
else()
    target_compile_options(logAnalyzer PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Installation
include(GNUInstallDirs)
set(LOGANALYZER_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/logAnalyzer")

install(TARGETS logAnalyzer
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS LogAnalyzerLib
    EXPORT logAnalyzerTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/logAnalyzerConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/logAnalyzerConfig.cmake"
    INSTALL_DESTINATION "${LOGANALYZER_INSTALL_CMAKEDIR}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/logAnalyzerConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(EXPORT logAnalyzerTargets
    FILE logAnalyzerTargets.cmake
    NAMESPACE logAnalyzer::
    DESTINATION "${LOGANALYZER_INSTALL_CMAKEDIR}"
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/logAnalyzerConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/logAnalyzerConfigVersion.cmake"
    DESTINATION "${LOGANALYZER_INSTALL_CMAKEDIR}"
)

# Enable testing
enable_testing()
add_subdirectory(tests)
