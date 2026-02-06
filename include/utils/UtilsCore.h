// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "core/LogTypes.h"   // For LogEntryField, LogLevel

#ifndef UTILS_H
#define UTILS_H

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

#include "core/CiLess.h"     // For ci_less comparator
#include "core/Error.h"      // For ErrorCode::Error


// Forward declarations to break include cycles
// Full definitions are in Filter.h, Exporter.h, Statistics.h
enum class ExportFormat;
enum class StatisticType;
enum class SortBy : uint8_t;
enum class SortOrder : uint8_t;


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

// --- Time Utilities (from UtilsTime.cpp) ---
std::string formatTimestamp(std::chrono::system_clock::time_point tp, std::string_view format = "%Y-%m-%d %H:%M:%S");
std::expected<std::chrono::microseconds, ErrorCode::Error> parseDuration(const std::string& durationStr, bool allowExtendedUnits);
std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseRelativeTime(const std::string& timeStr);
std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseAbsoluteTime(const std::string& timeStr);
std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseISO8601(const std::string& timeStr);
std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseTime(const std::string& timeStr);
std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseTimeWithFormats(const std::string& timeStr, const std::vector<std::string>& formats);
std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> validateTimestampCliOption(const std::string &tsStr);
std::expected<std::pair<std::chrono::system_clock::time_point, std::chrono::system_clock::time_point>, ErrorCode::Error> parseDayRange(const std::string& dateString);

std::expected<size_t, ErrorCode::Error> parseHumanReadableSize(std::string_view sizeStr);


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

std::string sortByToString(SortBy sort);
std::optional<SortBy> stringToSortBy(const std::string& sortStr);

std::string sortOrderToString(SortOrder order);
std::optional<SortOrder> stringToSortOrder(const std::string& orderStr);

} // namespace Utils

#endif // UTILS_H
