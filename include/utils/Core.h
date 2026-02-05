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

// --- String Utilities (from UtilsString.cpp) ---
void replaceAll(std::string &str, const std::string &from, const std::string &to);
void replaceAllIgnoreCase(std::string& str, const std::string& from, const std::string& to);
std::string trim(const std::string& str, std::string_view whitespace = " \t\n\r\f\v");
std::vector<std::string> split(const std::string& str, char delimiter);
std::string toLower(const std::string& str);
std::string toUpper(const std::string& str);
std::string escapeJsonString(const std::string& input);
std::string globToRegex(const std::string& globPattern);
inline bool caseInsensitiveEquals(std::string_view str1, std::string_view str2) {
    return std::equal(str1.begin(), str1.end(),
                      str2.begin(), str2.end(),
                      [](char a, char b) {
                          return std::tolower(a) == std::tolower(b);
                      });
}

// Helper to search for a substring case-insensitively
inline bool caseInsensitiveSearch(std::string_view text, std::string_view keyword) {
    auto it = std::search(text.begin(), text.end(),
                          keyword.begin(), keyword.end(),
                          [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); });
    return (it != text.end());
}


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
