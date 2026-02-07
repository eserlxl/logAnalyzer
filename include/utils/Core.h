// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef UTILS_H
#define UTILS_H

#include "core/Log/Types.h"   // For LogEntryField, LogLevel
#include "core/CiLess.h"     // For ci_less comparator
#include "core/Error.h"      // For ErrorCode::Error
#include "utils/Time.h"      // For time utilities

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <filesystem>
#include <chrono>
#include <expected>
#include <utility>
#include <algorithm>
#include <cctype>
#include <optional>

// Forward declarations to break include cycles
// Full definitions are in Filter.h, Exporter.h, Statistics.h
enum class ExportFormat;
enum class StatisticType;
namespace filter { enum class SortBy : uint8_t; }
namespace filter { enum class SortOrder : uint8_t; }


namespace Utils {

// ANSI Color Codes
namespace AnsiColor {
    inline constexpr std::string_view RESET = "\033[0m";
    inline constexpr std::string_view RED = "\033[31m";
    inline constexpr std::string_view GREEN = "\033[32m";
    inline constexpr std::string_view YELLOW = "\033[33m";
    inline constexpr std::string_view BLUE = "\033[34m";
    inline constexpr std::string_view MAGENTA = "\033[35m";
    inline constexpr std::string_view CYAN = "\033[36m";
    inline constexpr std::string_view WHITE = "\033[37m";
    inline constexpr std::string_view BOLD = "\033[1m";
    inline constexpr std::string_view FAINT = "\033[2m";
    inline constexpr std::string_view ITALIC = "\033[3m";
    inline constexpr std::string_view UNDERLINE = "\033[4m";
} // namespace AnsiColor

// Constant for stdin file path representation
inline constexpr std::string_view STDIN_FILE_PATH = "-"; // Made inline constexpr

// --- General Utilities (from Utils.cpp) ---
// File System Utilities
bool fileExists(const std::string& filePath);
std::string getFileName(const std::string& filePath);
std::string getFileExtension(const std::string& filePath);
std::string getDirectory(const std::string& filePath);

// --- Time Utilities ---
// Provided by utils/Time.h

std::expected<size_t, ErrorCode::Error> parseHumanReadableSize(std::string_view sizeStr);

size_t generateLogEntryId(const std::string& sourceFile, size_t lineNumber, std::string_view line);


// --- Enum to string and string to enum conversions ---
std::string logEntryFieldToString(LogEntryField field);
LogEntryField stringToLogEntryField(const std::string& fieldStr);

// LogLevel functions are declared in LogTypes.h inside namespace Utils

std::string exportFormatToString(ExportFormat format);
std::optional<ExportFormat> stringToExportFormat(const std::string& formatStr);

std::string statisticTypeToString(StatisticType type);
std::optional<StatisticType> stringToStatisticType(const std::string& typeStr);

std::string patternTypeToString(PatternType type);
std::optional<PatternType> stringToPatternType(const std::string& typeStr);

std::string parseErrorToString(ParseError error);
std::optional<ParseError> stringToParseError(const std::string& errorStr);

std::string sortByToString(filter::SortBy sort);
std::optional<filter::SortBy> stringToSortBy(const std::string& sortStr);

std::string sortOrderToString(filter::SortOrder order);
std::optional<filter::SortOrder> stringToSortOrder(const std::string& orderStr);

} // namespace Utils

#endif // UTILS_H
