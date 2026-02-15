# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Eser KUBALI

file(GLOB_RECURSE SOURCES *.cpp)
add_library(LogAnalyzerLib ${SOURCES})
target_include_directories(LogAnalyzerLib PUBLIC ../include)
target_link_libraries(LogAnalyzerLib PUBLIC CLI11::CLI11)

