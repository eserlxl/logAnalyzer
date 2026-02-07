// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef UTILS_TIME_H
#define UTILS_TIME_H

#include <chrono>
#include <string>
#include <vector>
#include <expected>
#include <ctime> // For std::tm

#include "core/Error.h" // For ErrorCode::Error

namespace Utils {

// Forward declaration of the mutex for external linkage if needed,
// but for now, it's internal to the .cpp.
// static std::mutex localtimeMutex; // Cannot be in header if static. Will make it extern if needed.

// Helper to validate if a date is valid (not normalized by mktime)
bool isTmValid(const std::tm& tm_orig, const std::tm& tm_new);

// Portable timegm implementation
time_t portable_timegm(struct tm *tm);

// Iteration 5: Thread-Safe UTC Time Handling
/**
 * @brief Formats a time_point to a UTC string. Thread-safe.
 *
 * @param tp The time_point to format.
 * @param format_str The desired output format string (e.g., "%Y-%m-%d %H:%M:%S UTC").
 * @return The formatted UTC string.
 */
std::string format_utc(const std::chrono::system_clock::time_point& tp, const std::string& format_str = "%Y-%m-%d %H:%M:%S UTC");

/**
 * @brief Parses a UTC time string into a time_point. Thread-safe.
 *
 * @param time_str The UTC time string to parse.
 * @param format_str The format of the input time string (e.g., "%Y-%m-%d %H:%M:%S UTC").
 * @return The parsed time_point.
 * @throws std::runtime_error if parsing fails.
 */
std::chrono::system_clock::time_point parse_utc(const std::string& time_str, const std::string& format_str = "%Y-%m-%d %H:%M:%S UTC");

// Iteration 5: Explicit Time Zone Support
namespace tz {
    // Represents a time zone, leveraging C++20's std::chrono::time_zone.
    using Zone = std::chrono::time_zone;
}

/**
 * @brief Formats a time_point into a string in a specific time zone.
 *
 * @param tp The time_point to format.
 * @param zone The target time zone.
 * @param format_str The desired output format string.
 * @return The formatted string in the specified time zone.
 */
std::string format_zone(const std::chrono::system_clock::time_point& tp, const tz::Zone* zone, const std::string& format_str = "%Y-%m-%d %H:%M:%S %Z");

/**
 * @brief Parses a string (potentially with timezone info) into a time_point.
 *
 * @param time_str The time string to parse.
 * @param zone The time zone to assume for the time string.
 * @param format_str The format of the input time string.
 * @return The parsed time_point.
 */
std::chrono::system_clock::time_point parse_zone(const std::string& time_str, const tz::Zone* zone, const std::string& format_str = "%Y-%m-%d %H:%M:%S %Z");

/**
 * @brief Converts a time_point from one zone to another.
 *
 * @param tp The time_point to convert.
 * @param from_zone The source time zone.
 * @param to_zone The target time zone.
 * @return The converted time_point.
 */
std::chrono::system_clock::time_point convert_zone(const std::chrono::system_clock::time_point& tp, const tz::Zone* from_zone, const tz::Zone* to_zone);

// Iteration 5: Date and Time Arithmetic
std::chrono::system_clock::time_point add_duration(const std::chrono::system_clock::time_point& tp, std::chrono::seconds duration);
std::chrono::system_clock::time_point add_duration(const std::chrono::system_clock::time_point& tp, std::chrono::minutes duration);
std::chrono::system_clock::time_point add_duration(const std::chrono::system_clock::time_point& tp, std::chrono::hours duration);
std::chrono::system_clock::time_point add_duration(const std::chrono::system_clock::time_point& tp, std::chrono::days duration);

std::chrono::system_clock::time_point subtract_duration(const std::chrono::system_clock::time_point& tp, std::chrono::seconds duration);
std::chrono::system_clock::time_point subtract_duration(const std::chrono::system_clock::time_point& tp, std::chrono::minutes duration);
std::chrono::system_clock::time_point subtract_duration(const std::chrono::system_clock::time_point& tp, std::chrono::hours duration);
std::chrono::system_clock::time_point subtract_duration(const std::chrono::system_clock::time_point& tp, std::chrono::days duration);

// Iteration 5: High-Resolution Timing
class HighResTimer {
public:
    HighResTimer();
    void start();
    void stop();
    double elapsedSeconds() const;
    double elapsedMilliseconds() const;
    double elapsedMicroseconds() const;
    double elapsedNanoseconds() const;

private:
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point stopTime;
    bool running;
};

// Iteration 5: Enhanced Parsing and Formatting
std::string format_iso8601(const std::chrono::system_clock::time_point& tp);
std::chrono::system_clock::time_point parse_iso8601(const std::string& time_str);


std::string formatTimestamp(std::chrono::system_clock::time_point tp, std::string_view format);

std::expected<std::chrono::microseconds, ErrorCode::Error> parseDuration(const std::string& durationStr, bool allowExtendedUnits = false);

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseRelativeTime(const std::string& timeStr);

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseAbsoluteTime(const std::string& timeStr);

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseISO8601(const std::string& timeStr);

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseTime(const std::string& timeStr);

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error>
parseTimeWithFormats(const std::string& timeStr, const std::vector<std::string>& formats);

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> validateTimestampCliOption(const std::string &tsStr);

std::expected<std::pair<std::chrono::system_clock::time_point, std::chrono::system_clock::time_point>, ErrorCode::Error>
parseDayRange(const std::string& dateString);

} // namespace Utils

#endif // UTILS_TIME_H
