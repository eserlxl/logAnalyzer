#ifndef UTILS_H
#define UTILS_H

#include "LogTypes.h"
#include <string>
#include <string_view>
#include <chrono>
#include <expected> // For std::expected

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

LogLevel stringToLogLevel(const std::string &levelStr);
std::string logLevelToString(LogLevel level);
std::string formatTimestamp(std::chrono::system_clock::time_point tp,
                             std::string_view format = "%Y-%m-%d %H:%M:%S");

// Replaces all occurrences of 'from' with 'to' in 'str'
void replaceAll(std::string &str, const std::string &from, const std::string &to);

// Parses a duration string (e.g., "10s", "5m", "2h", "1d") into std::chrono::seconds.
std::expected<std::chrono::seconds, std::string> parseDuration(const std::string& durationStr);

// Calculates a time point relative to the current time (e.g., "1h ago").
std::expected<std::chrono::system_clock::time_point, std::string> parseRelativeTime(const std::string& timeStr);

// Parses an absolute time string (e.g., "2023-01-01 12:30:00") into std::chrono::system_clock::time_point.
std::expected<std::chrono::system_clock::time_point, std::string> parseAbsoluteTime(const std::string& timeStr);

} // namespace Utils

#endif // UTILS_H
