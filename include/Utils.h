#include "LogTypes.h" // Ensure ci_less is declared before use

#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <string_view>
#include <vector> // Required for std::vector in split
#include <map> // Required for std::map
#include <filesystem> // Required for std::filesystem utilities
#include <chrono>
#expected>
#include <utility>
#include <algorithm>
#include <cctype>

// Forward declarations for types used in Utils functions.
// These are not #includes, as Utils.h should not directly include headers
// that might circularly include Utils.h.
enum class LogEntryField;
enum class FilterOperator;
enum class FilterLogicalOperator;
enum class ExportFormat;
enum class StatisticType;
enum class StatisticOutputFormat;
enum class LogLevel;

namespace Utils {

// ANSI Color Codes
namespace AnsiColor {
    inline const std::string RESET = "\033[0m";
    inline const std::string RED = "\033[31m";
    inline const std::string GREEN = "\033[32m";
    inline const std::string YELLOW = "\033[33m";
    inline const std::string BLUE = "\033[34m";
    inline const std::string MAGENTA = "\033[35m";
    inline const std::string CYAN = "\033[36m";
    inline const std::string WHITE = "\033[37m";
    inline const std::string BOLD = "\033[1m";
    inline const std::string FAINT = "\033[2m";
    inline const std::string ITALIC = "\033[3m";
    inline const std::string UNDERLINE = "\033[4m";
} // namespace AnsiColor

// Constant for stdin file path representation
static constexpr std::string_view STDIN_FILE_PATH = "-";

// Overload for stringToLogLevel that accepts custom mappings with ci_less comparator.
LogLevel stringToLogLevel(const std::string &levelStr, const std::map<std::string, LogLevel, ci_less> &customMappings);
LogLevel stringToLogLevel(const std::string &levelStr);
LogLevel stringToLogLevelIgnoreCase(const std::string &levelStr);
std::string logLevelToString(LogLevel level);
std::string formatTimestamp(std::chrono::system_clock::time_point tp,
                             std::string_view format = "%Y-%m-%d %H:%M:%S");

// Replaces all occurrences of 'from' with 'to' in 'str'
void replaceAll(std::string &str, const std::string &from, const std::string &to);

// Replaces all occurrences of 'from' with 'to' in 'str', ignoring case
void replaceAllIgnoreCase(std::string& str, const std::string& from, const std::string& to);

// Returns a new string with leading and trailing whitespace characters removed.
std::string trim(const std::string& str, const std::string& whitespace = " \t\n\r\f\v");

// Splits str into a std::vector<std::string> using delimiter as the separator.
std::vector<std::string> split(const std::string& str, char delimiter);

// Returns a new string with all characters converted to lower case.
std::string toLower(const std::string& str);

// Returns a new string with all characters converted to upper case.
std::string toUpper(const std::string& str);

// File System Utilities
// Returns true if filePath points to an existing regular file, false otherwise.
bool fileExists(const std::string& filePath);

// Returns the filename component of filePath (e.g., "file.txt" from "/path/to/file.txt").
std::string getFileName(const std::string& filePath);

// Returns the extension of filePath (e.g., "txt" from "/path/to/file.txt"). Returns an empty string if no extension.
std::string getFileExtension(const std::string& filePath);

// Returns the directory component of filePath (e.g., "/path/to/" from "/path/to/file.txt").
std::string getDirectory(const std::string& filePath);

// Parses a duration string (e.g., "10s", "5m", "2h", "1d") into std::chrono::seconds.
std::expected<std::chrono::seconds, std::string> parseDuration(const std::string& durationStr, bool allowExtendedUnits = false);

// Calculates a time point relative to the current time (e.g., "1h ago").
std::expected<std::chrono::system_clock::time_point, std::string> parseRelativeTime(const std::string& timeStr);

// Parses an absolute time string (e.g., "2023-01-01 12:30:00") into std::chrono::system_clock::time_point.
std::expected<std::chrono::system_clock::time_point, std::string> parseAbsoluteTime(const std::string& timeStr);

// Parses a time string, supporting multiple absolute formats (YYYY-MM-DD HH:MM:SS, ISO 8601, Unix timestamp)
// and also falling back to relative time parsing.
std::expected<std::chrono::system_clock::time_point, std::string> parseTime(const std::string& timeStr);

// Parses a time string using a list of provided formats.
std::expected<std::chrono::system_clock::time_point, std::string>
parseTimeWithFormats(const std::string& timeStr, const std::vector<std::string>& formats);

// Parses a date string (e.g., "YYYY-MM-DD", "YYYY/MM/DD") into a time range for that entire day.
// Returns a pair: first is 00:00:00 of the day, second is 23:59:59.999... of the day.
std::expected<std::pair<std::chrono::system_clock::time_point, std::chrono::system_clock::time_point>, std::string>
parseDayRange(const std::string& dateString);

// Validates a timestamp string for CLI options. Throws CLI::ValidationError on failure.
std::string validateTimestampCliOption(const std::string &tsStr);

// Escapes a string for JSON output, handling special characters like quotes, backslashes, and control characters.
std::string escapeJsonString(const std::string& input);

// Converts a glob pattern string into a regex pattern string.
// Handles '*' as '.*' and '?' as '.'
std::string globToRegex(const std::string& globPattern);

// Helper to compare strings case-insensitively
inline bool caseInsensitiveEquals(const std::string& str1, const std::string& str2) {
    if (str1.length() != str2.length()) {
        return false;
    }
    return std::equal(str1.begin(), str1.end(),
                      str2.begin(), str2.end(),
                      [](char a, char b) {
                          return std::tolower(static_cast<unsigned char>(a)) ==
                                 std::tolower(static_cast<unsigned char>(b));
                      });
}

// Helper to search for a substring case-insensitively
inline bool caseInsensitiveSearch(const std::string& text, const std::string& keyword) {
    if (keyword.empty()) {
        return true; // Empty keyword is considered to be found everywhere
    }
    if (text.length() < keyword.length()) {
        return false;
    }
    
    auto it = std::search(text.begin(), text.end(),
                          keyword.begin(), keyword.end(),
                          [](char a, char b) {
                              return std::tolower(static_cast<unsigned char>(a)) ==
                                     std::tolower(static_cast<unsigned char>(b));
                          });
    return it != text.end();
}

// Enum to string and string to enum conversions for various types
// Defined here to avoid redefinition issues and ensure single source of truth
std::string logEntryFieldToString(LogEntryField field);
LogEntryField stringToLogEntryField(const std::string& fieldStr);

std::string filterOperatorToString(FilterOperator op);
FilterOperator stringToFilterOperator(const std::string& opStr);

std::string filterLogicalOperatorToString(FilterLogicalOperator op);
FilterLogicalOperator stringToFilterLogicalOperator(const std::string& opStr);

std::string exportFormatToString(ExportFormat format);
ExportFormat stringToExportFormat(const std::string& formatStr);

std::string statisticTypeToString(StatisticType type);
StatisticType stringToStatisticType(const std::string& typeStr);

std::string statisticOutputFormatToString(StatisticOutputFormat format);
StatisticOutputFormat stringToStatisticOutputFormat(const std::string& formatStr);

} // namespace Utils

#endif // UTILS_H
