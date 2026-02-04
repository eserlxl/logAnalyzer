#include "../include/Utils.h" // Includes all necessary declarations for Utils namespace
#include <ctime>
#include <sstream>
#include <iomanip>
#include <regex>
// #include <CLI/CLI.hpp> // Not directly used by functions in this file

namespace Utils {

std::string formatTimestamp(std::chrono::system_clock::time_point tp, std::string_view format) {
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::localtime(&tt); // Or gmtime for UTC
    std::ostringstream ss;
    ss << std::put_time(&tm, format.data());
    return ss.str();
}

std::expected<std::chrono::seconds, ErrorCode::Error> parseDuration(const std::string& durationStr, bool allowExtendedUnits) {
    std::regex durationRegex("^(\\d+)([smhd]|ms|us|w|M|y)$");
    std::smatch matches;

    if (std::regex_match(durationStr, matches, durationRegex)) {
        long long value = std::stoll(matches[1].str());
        std::string unit = matches[2].str();

        std::chrono::seconds total_seconds(0);

        if (unit == "s") {
            total_seconds = std::chrono::seconds(value);
        } else if (unit == "m") {
            total_seconds = std::chrono::minutes(value);
        } else if (unit == "h") {
            total_seconds = std::chrono::hours(value);
        } else if (unit == "d") {
            total_seconds = std::chrono::days(value);
        } else if (allowExtendedUnits) {
            if (unit == "ms") {
                total_seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds(value));
            } else if (unit == "us") {
                total_seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::microseconds(value));
            } else if (unit == "w") {
                total_seconds = std::chrono::weeks(value);
            } else if (unit == "M") { // Approximate month as 30 days
                total_seconds = std::chrono::days(value * 30);
            } else if (unit == "y") { // Approximate year as 365 days
                total_seconds = std::chrono::days(value * 365);
            } else {
                return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Unknown duration unit with extended units enabled."));
            }
        } else {
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Unknown duration unit. Extended units are not enabled."));
        }
        return total_seconds;
    }
    return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid duration format. Expected formats like '10s', '5m', '2h', '1d' or extended units if enabled."));
}

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseRelativeTime(const std::string& timeStr) {
    auto now = std::chrono::system_clock::now();
    std::chrono::seconds duration_seconds;
    std::smatch matches;

    // Handle "X units ago"
    std::regex relativeTimeAgoRegex("^(\\d+)([smhd]) ago$");
    if (std::regex_match(timeStr, matches, relativeTimeAgoRegex)) {
        long long value = std::stoll(matches[1].str());
        char unit = matches[2].str()[0];
        switch (unit) {
            case 's': duration_seconds = std::chrono::seconds(value); break;
            case 'm': duration_seconds = std::chrono::minutes(value); break;
            case 'h': duration_seconds = std::chrono::hours(value); break;
            case 'd': duration_seconds = std::chrono::days(value); break;
            default: return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Unknown time unit in 'ago' expression."));
        }
        return now - duration_seconds;
    }

    // Handle "in X units"
    std::regex relativeTimeInRegex("^in (\\d+)([smhd])$ ");
    if (std::regex_match(timeStr, matches, relativeTimeInRegex)) {
        long long value = std::stoll(matches[1].str());
        char unit = matches[2].str()[0];
        switch (unit) {
            case 's': duration_seconds = std::chrono::seconds(value); break;
            case 'm': duration_seconds = std::chrono::minutes(value); break;
            case 'h': duration_seconds = std::chrono::hours(value); break;
            case 'd': duration_seconds = std::chrono::days(value); break;
            default: return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Unknown time unit in 'in' expression."));
        }
        return now + duration_seconds;
    }

    // Handle "yesterday"
    if (timeStr == "yesterday") {
        return now - std::chrono::days(1);
    }

    // Handle "tomorrow"
    if (timeStr == "tomorrow") {
        return now + std::chrono::days(1);
    }

    // Handle "next week" (approx. 7 days from now)
    if (timeStr == "next week") {
        return now + std::chrono::weeks(1);
    }

    return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid relative time format. Expected formats like '10s ago', 'in 5m', 'yesterday', 'tomorrow', 'next week'."));
}

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseAbsoluteTime(const std::string& timeStr) {
    std::tm tm = {};
    std::stringstream ss(timeStr);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid absolute time format. Expected 'YYYY-MM-DD HH:MM:SS'."));
    }
    auto timePoint = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    return timePoint;
}

// Helper to parse ISO 8601 with optional Z or offset
namespace { // Anonymous namespace for internal helper
static std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseISO8601(const std::string& timeStr) {
    std::tm tm = {};
    std::stringstream ss(timeStr);
    std::string format;
    int offset_h = 0;
    int offset_m = 0;
    std::smatch matches; // Declare matches here

    // Try YYYY-MM-DDTHH:MM:SSZ (UTC)
    format = "%Y-%m-%dT%H:%M:%SZ";
    ss.clear(); ss.seekg(0); ss >> std::get_time(&tm, format.c_str());
    if (!ss.fail() && ss.eof()) { // Check eof to ensure whole string matched
        return std::chrono::system_clock::from_time_t(timegm(&tm)); // Use timegm for UTC
    }
    
    // Try YYYY-MM-DDTHH:MM:SS (local time implicitly)
    format = "%Y-%m-%dT%H:%M:%S";
    ss.clear(); ss.seekg(0); ss >> std::get_time(&tm, format.c_str());
    if (!ss.fail() && ss.eof()) {
        return std::chrono::system_clock::from_time_t(mktime(&tm));
    }

    // Try YYYY-MM-DDTHH:MM:SS+HH:MM or YYYY-MM-DDTHH:MM:SS-HH:MM
    std::regex iso8601_tz_regex("^(\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2})([+-])(\\d{2}):(\\d{2})$");
    if (std::regex_match(timeStr, matches, iso8601_tz_regex)) {
        std::string dateTimePart = matches[1].str();
        char sign = matches[2].str()[0];
        offset_h = std::stoi(matches[3].str());
        offset_m = std::stoi(matches[4].str());

        std::stringstream ss_dt(dateTimePart);
        ss_dt >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        if (!ss_dt.fail()) {
            std::time_t tt = mktime(&tm);
            if (tt != -1) {
                std::chrono::system_clock::time_point tp = std::chrono::system_clock::from_time_t(tt);
                std::chrono::seconds offset_sec = std::chrono::hours(offset_h) + std::chrono::minutes(offset_m);
                if (sign == '+') { 
                    tp -= offset_sec; // Convert local time with offset to UTC
                } else { // sign == '-' 
                    tp += offset_sec; // Convert local time with offset to UTC
                }
                return tp;
            }
        }
    }
    
    return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid ISO 8601 format."));
}
} // namespace

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseTime(const std::string& timeStr) {
    // 1. Try to parse as the original absolute time format "YYYY-MM-DD HH:MM:SS"
    auto absTimeResult = parseAbsoluteTime(timeStr);
    if (absTimeResult) {
        return absTimeResult;
    }

    // 2. Try to parse as ISO 8601
    auto iso8601Result = parseISO8601(timeStr);
    if (iso8601Result) {
        return iso8601Result;
    }

    // 3. Try to parse as Unix timestamp (seconds since epoch)
    try {
        // Check if string contains only digits
        if (timeStr.find_first_not_of("0123456789") == std::string::npos) {
            long long timestamp = std::stoll(timeStr);
            if (timestamp >= 0) { // Unix timestamps are typically non-negative
                return std::chrono::system_clock::from_time_t(static_cast<std::time_t>(timestamp));
            }
        }
    } catch (const std::out_of_range& oor) {
        // Timestamp too large or small for long long, ignore and try next
    } catch (const std::invalid_argument& ia) {
        // Not a number, ignore and try next
    }

    // 4. Fallback to relative time parsing
    auto relativeTimeResult = parseRelativeTime(timeStr);
    if (relativeTimeResult) {
        return relativeTimeResult;
    }

    return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Failed to parse time string. Unknown format."));
}

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error>
parseTimeWithFormats(const std::string& timeStr, const std::vector<std::string>& formats) {
    std::tm tm = {};
    for (const auto& format : formats) {
        if (format.empty()) continue;
        std::stringstream ss(timeStr);
        ss >> std::get_time(&tm, format.c_str());
        if (!ss.fail() && static_cast<size_t>(ss.tellg()) == timeStr.length()) {
            auto timePoint = std::chrono::system_clock::from_time_t(std::mktime(&tm));
            return timePoint;
        }
    }
    return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Failed to parse time string with any provided format."));
}

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> validateTimestampCliOption(const std::string &tsStr) {
    if (tsStr.empty()) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Timestamp string cannot be empty."));
    }
    auto timePointResult = Utils::parseTime(tsStr);
    if (timePointResult.has_value()) {
        return timePointResult.value(); // Return the time_point if successful
    }
    // Return an unexpected value with the error
    return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid time format: " + timePointResult.error().message + ". Expected formats: 'YYYY-MM-DD HH:MM:SS', ISO 8601, Unix timestamp, or relative time like '1h ago'."));
}

std::expected<std::pair<std::chrono::system_clock::time_point, std::chrono::system_clock::time_point>, ErrorCode::Error>
parseDayRange(const std::string& dateString) {
    std::tm tm = {};
    std::istringstream ss(dateString);

    // Try YYYY-MM-DD
    ss.clear(); ss.seekg(0);
    ss >> std::get_time(&tm, "%Y-%m-%d");
    if (!ss.fail() && ss.eof()) {
        goto success_parse_date;
    }

    // Try YYYY/MM/DD
    ss.clear(); ss.seekg(0);
    ss >> std::get_time(&tm, "%Y/%m/%d");
    if (!ss.fail() && ss.eof()) {
        goto success_parse_date;
    }

    // Try MM-DD-YYYY
    ss.clear(); ss.seekg(0);
    ss >> std::get_time(&tm, "%m-%d-%Y");
    if (!ss.fail() && ss.eof()) {
        goto success_parse_date;
    }

    // Try MM/DD/YYYY
    ss.clear(); ss.seekg(0);
    ss >> std::get_time(&tm, "%m/%d/%Y");
    if (!ss.fail() && ss.eof()) {
        goto success_parse_date;
    }

    return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid date format for day range. Expected 'YYYY-MM-DD', 'YYYY/MM/DD', 'MM-DD-YYYY', or 'MM/DD/YYYY'."));

success_parse_date:
    // Set time to beginning of the day (00:00:00)
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    auto startOfDay = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    // Set time to end of the day (23:59:59)
    tm.tm_hour = 23;
    tm.tm_min = 59;
    tm.tm_sec = 59;
    auto endOfDay = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    return std::make_pair(startOfDay, endOfDay);
}

} // namespace Utils
