#ifndef UTILS_TIME_H
#define UTILS_TIME_H

#include <chrono>
#include <string>
#include <vector>
#include <expected>
#include <ctime> // For std::tm

#include "utils/Core.h" // For ErrorCode::Error

namespace Utils {

// Forward declaration of the mutex for external linkage if needed,
// but for now, it's internal to the .cpp.
// static std::mutex localtimeMutex; // Cannot be in header if static. Will make it extern if needed.

// Helper to validate if a date is valid (not normalized by mktime)
bool isTmValid(const std::tm& tm_orig, const std::tm& tm_new);

// Portable timegm implementation
time_t portable_timegm(struct tm *tm);

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
