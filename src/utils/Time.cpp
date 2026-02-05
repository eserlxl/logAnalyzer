#include "utils/Core.h"
#include <ctime>
#include <sstream>
#include <iomanip>
#include <regex>
#include <mutex>
#include <iostream>
#include <stdlib.h> // for setenv, getenv, unsetenv
#include <string.h> // for strdup, free
#include <errno.h>  // for errno

namespace Utils {

static std::mutex localtimeMutex;

// Helper to validate if a date is valid (not normalized by mktime)
bool isTmValid(const std::tm& tm_orig, const std::tm& tm_new) {
    // Check if mktime/timegm normalized any fields
    return tm_new.tm_year == tm_orig.tm_year &&
           tm_new.tm_mon == tm_orig.tm_mon &&
           tm_new.tm_mday == tm_orig.tm_mday &&
           tm_new.tm_hour == tm_orig.tm_hour &&
           tm_new.tm_min == tm_orig.tm_min &&
           tm_new.tm_sec == tm_orig.tm_sec;
}

// Portable timegm implementation
time_t portable_timegm(struct tm *tm) {
    // Save original TZ
    char* original_tz = getenv("TZ");
    char* original_tz_copy = nullptr;
    if (original_tz) {
        original_tz_copy = strdup(original_tz);
        if (!original_tz_copy) {
            // Handle memory allocation failure
            return -1; 
        }
    }

    // Set TZ to UTC
    setenv("TZ", "UTC", 1);
    tzset();

    // Call mktime
    time_t ret = mktime(tm);

    // Restore original TZ
    if (original_tz_copy) {
        setenv("TZ", original_tz_copy, 1);
        free(original_tz_copy);
    } else {
        unsetenv("TZ");
    }
    tzset();

    return ret;
}


std::string formatTimestamp(std::chrono::system_clock::time_point tp, std::string_view format) {
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;
    {
        std::lock_guard<std::mutex> lock(localtimeMutex);
        tm = *std::localtime(&tt);
    }
    std::ostringstream ss;
    ss << std::put_time(&tm, format.data());
    return ss.str();
}

std::expected<std::chrono::seconds, ErrorCode::Error> parseDuration(const std::string& durationStr, bool allowExtendedUnits) {
    static const std::regex durationRegex("^(\\d+)([smhd]|ms|us|w|M|y)$");
    std::smatch matches;

    if (std::regex_match(durationStr, matches, durationRegex)) {
        long long value;
        try {
            value = std::stoll(matches[1].str());
        } catch (const std::out_of_range& oor) {
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Duration value out of range for 'stoll'."));
        } catch (const std::invalid_argument& ia) {
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid duration value."));
        }
        std::string unit = matches[2].str();

        if (unit == "s") return std::chrono::seconds(value);
        if (unit == "m") return std::chrono::minutes(value);
        if (unit == "h") return std::chrono::hours(value);
        if (unit == "d") return std::chrono::days(value);

        if (allowExtendedUnits) {
            if (unit == "ms") return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds(value));
            if (unit == "us") return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::microseconds(value));
            if (unit == "w") return std::chrono::weeks(value);
            if (unit == "M") return std::chrono::days(value * 30);
            if (unit == "y") return std::chrono::days(value * 365);
        }
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Unknown or disabled duration unit."));
    }
    return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid duration format. Expected formats like '10s', '5m', '2h', '1d' or extended units if enabled."));
}

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseRelativeTime(const std::string& timeStr) {
    auto now = std::chrono::system_clock::now();
    std::smatch matches;

    static const std::regex agoRegex("^(\\d+)([smhd]) ago$");
    if (std::regex_match(timeStr, matches, agoRegex)) {
        long long val;
        try {
            val = std::stoll(matches[1].str());
        } catch (const std::out_of_range& oor) {
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Relative time 'ago' value out of range."));
        } catch (const std::invalid_argument& ia) {
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid relative time 'ago' value."));
        }
        char unit = matches[2].str()[0];
        if (unit == 's') return now - std::chrono::seconds(val);
        if (unit == 'm') return now - std::chrono::minutes(val);
        if (unit == 'h') return now - std::chrono::hours(val);
        if (unit == 'd') return now - std::chrono::days(val);
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Unknown time unit in 'ago' expression."));
    }

    static const std::regex inRegex("^in (\\d+)([smhd])$");
    if (std::regex_match(timeStr, matches, inRegex)) {
        long long val;
        try {
            val = std::stoll(matches[1].str());
        } catch (const std::out_of_range& oor) {
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Relative time 'in' value out of range."));
        } catch (const std::invalid_argument& ia) {
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid relative time 'in' value."));
        }
        char unit = matches[2].str()[0];
        if (unit == 's') return now + std::chrono::seconds(val);
        if (unit == 'm') return now + std::chrono::minutes(val);
        if (unit == 'h') return now + std::chrono::hours(val);
        if (unit == 'd') return now + std::chrono::days(val);
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Unknown time unit in 'in' expression."));
    }

    if (timeStr == "yesterday") return now - std::chrono::days(1);
    if (timeStr == "tomorrow") return now + std::chrono::days(1);
    if (timeStr == "next week") return now + std::chrono::weeks(1);

    return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid relative time format. Expected formats like '10s ago', 'in 5m', 'yesterday', 'tomorrow', 'next week'."));
}

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseAbsoluteTime(const std::string& timeStr) {
    std::tm tm = {};
    std::istringstream ss(timeStr);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail() || !ss.eof()) { // Check eof to ensure whole string matched
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid absolute time format. Expected 'YYYY-MM-DD HH:MM:SS'."));
    }
    std::tm tm_orig = tm; // Save original tm to check for normalization
    tm.tm_isdst = -1; // Let mktime determine DST
    std::time_t tt = std::mktime(&tm);
    if (tt == -1 || !isTmValid(tm_orig, tm)) { // Check if mktime failed or normalized the date
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid date/time value (e.g., Feb 30th) or failed to convert to time_t."));
    }
    return std::chrono::system_clock::from_time_t(tt);
}

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseISO8601(const std::string& timeStr) {
    std::tm tm = {};
    std::smatch matches;

    // UTC: YYYY-MM-DDTHH:MM:SSZ
    static const std::regex utcRegex("^(\\d{4})-(\\d{2})-(\\d{2})T(\\d{2}):(\\d{2}):(\\d{2})Z$");
    if (std::regex_match(timeStr, matches, utcRegex)) {
        tm.tm_year = std::stoi(matches[1].str()) - 1900;
        tm.tm_mon = std::stoi(matches[2].str()) - 1;
        tm.tm_mday = std::stoi(matches[3].str());
        tm.tm_hour = std::stoi(matches[4].str());
        tm.tm_min = std::stoi(matches[5].str());
        tm.tm_sec = std::stoi(matches[6].str());
        tm.tm_isdst = 0; // UTC does not have DST

        std::tm tm_orig = tm; // Save original tm
        time_t tt = portable_timegm(&tm); // Use portable_timegm
        if (tt == -1 || !isTmValid(tm_orig, tm)) { // Check if timegm failed or normalized the date
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid ISO8601 UTC date/time (e.g., Feb 30th) or failed to convert to time_t."));
        }
        return std::chrono::system_clock::from_time_t(tt);
    }

    // Local: YYYY-MM-DDTHH:MM:SS (implicit local)
    static const std::regex localRegex("^(\\d{4})-(\\d{2})-(\\d{2})T(\\d{2}):(\\d{2}):(\\d{2})$");
    if (std::regex_match(timeStr, matches, localRegex)) {
        tm.tm_year = std::stoi(matches[1].str()) - 1900;
        tm.tm_mon = std::stoi(matches[2].str()) - 1;
        tm.tm_mday = std::stoi(matches[3].str());
        tm.tm_hour = std::stoi(matches[4].str());
        tm.tm_min = std::stoi(matches[5].str());
        tm.tm_sec = std::stoi(matches[6].str());
        tm.tm_isdst = -1; // Let mktime determine DST

        std::tm tm_orig = tm; // Save original tm
        time_t tt = std::mktime(&tm);
        if (tt == -1 || !isTmValid(tm_orig, tm)) { // Check if mktime failed or normalized the date
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid ISO8601 local date/time (e.g., Feb 30th) or failed to convert to time_t."));
        }
        return std::chrono::system_clock::from_time_t(tt);
    }

    // Offset: YYYY-MM-DDTHH:MM:SS[+-]HH:MM
    static const std::regex offsetRegex("^(\\d{4})-(\\d{2})-(\\d{2})T(\\d{2}):(\\d{2}):(\\d{2})([+-])(\\d{2}):(\\d{2})$");
    if (std::regex_match(timeStr, matches, offsetRegex)) {
        tm.tm_year = std::stoi(matches[1].str()) - 1900;
        tm.tm_mon = std::stoi(matches[2].str()) - 1;
        tm.tm_mday = std::stoi(matches[3].str());
        tm.tm_hour = std::stoi(matches[4].str());
        tm.tm_min = std::stoi(matches[5].str());
        tm.tm_sec = std::stoi(matches[6].str());
        tm.tm_isdst = 0; // Treat the parsed time as UTC before applying offset correction
        
        char sign = matches[7].str()[0];
        int off_h = std::stoi(matches[8].str());
        int off_m = std::stoi(matches[9].str());

        if (off_h < 0 || off_h > 23 || off_m < 0 || off_m > 59) {
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid timezone offset. Offset hours must be 0-23, minutes 0-59."));
        }

        std::tm tm_orig = tm; // Save original tm
        time_t tt_utc_before_offset_adjust = portable_timegm(&tm); // Get UTC time from the date/time part
        if (tt_utc_before_offset_adjust == -1 || !isTmValid(tm_orig, tm)) { // Check if timegm failed or normalized the date
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid ISO8601 offset date/time (e.g., Feb 30th) or failed to convert to time_t."));
        }
        
        auto tp = std::chrono::system_clock::from_time_t(tt_utc_before_offset_adjust);
        std::chrono::seconds offset_seconds = std::chrono::hours(off_h) + std::chrono::minutes(off_m);

        // Adjust to actual UTC: If input was +HH:MM, subtract offset from parsed time to get UTC.
        // If input was -HH:MM, add offset to parsed time to get UTC.
        return (sign == '+') ? tp - offset_seconds : tp + offset_seconds;
    }

    return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid ISO 8601 format. Expected formats like YYYY-MM-DDTHH:MM:SSZ, YYYY-MM-DDTHH:MM:SS, or YYYY-MM-DDTHH:MM:SS[+-]HH:MM."));
}

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
        // Check if string contains only digits (or starts with - for negative timestamps)
        if (!timeStr.empty() && (timeStr.find_first_not_of("0123456789", (timeStr[0] == '-' ? 1 : 0)) == std::string::npos)) {
            long long timestamp = std::stoll(timeStr);
            // We accept negative timestamps (before epoch) but mktime usually doesn't handle them well
            // For from_time_t, it should be fine.
            return std::chrono::system_clock::from_time_t(static_cast<std::time_t>(timestamp));
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
    for (const auto& format : formats) {
        if (format.empty()) continue;
        std::tm tm = {};
        std::istringstream ss(timeStr);
        // Attempt to parse. std::get_time will read as much as it can from timeStr
        ss >> std::get_time(&tm, format.c_str());
        
        // After parsing, check if the stream failed AND if the entire input string was consumed.
        // Also check if the format string itself was fully matched against the input.
        // A simple way to check if the format was fully matched to the consumed input
        // is to format tm back and compare.
        char buffer[256]; // Sufficiently large buffer for formatted time
        if (!ss.fail() && ss.eof() && std::strftime(buffer, sizeof(buffer), format.c_str(), &tm) > 0) {
            // Check if formatting back yields the original string. This is a strict check for full match.
            // This implicitly handles cases where get_time partially matches but the format expected more.
            if (timeStr == buffer) {
                std::tm tm_orig = tm; // Save original tm
                tm.tm_isdst = -1; // Let mktime determine DST
                std::time_t tt = std::mktime(&tm);
                if (tt != -1 && isTmValid(tm_orig, tm)) { // Check if mktime failed or normalized
                    return std::chrono::system_clock::from_time_t(tt);
                }
            }
        }
    }
    return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Failed to parse time string with any provided format."));
}

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> validateTimestampCliOption(const std::string &tsStr) {
    if (tsStr.empty()) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Timestamp string cannot be empty."));
    }
    auto timePointResult = Utils::parseTime(tsStr);
    if (!timePointResult) { 
        // Return the specific error from parseTime directly if parsing failed
        return std::unexpected(timePointResult.error());
    }
    return *timePointResult; // Return the time_point if successful
}

std::expected<std::pair<std::chrono::system_clock::time_point, std::chrono::system_clock::time_point>, ErrorCode::Error>
parseDayRange(const std::string& dateString) {
    std::tm tm = {};
    std::vector<std::string> formats = {"%Y-%m-%d", "%Y/%m/%d", "%m-%d-%Y", "%m/%d/%Y"};

    bool parsedSuccessfully = false;
    std::tm tm_orig_for_validation = {}; // To store the tm as parsed by get_time before mktime
    
    for (const auto& format : formats) {
        std::istringstream ss(dateString);
        ss >> std::get_time(&tm, format.c_str());
        if (!ss.fail() && ss.eof()) { // Check eof to ensure whole string matched
            tm_orig_for_validation = tm; // Save for validation after mktime
            parsedSuccessfully = true;
            break;
        }
        tm = {}; // Reset tm for the next format attempt
    }

    if (!parsedSuccessfully) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid date format for day range. Expected 'YYYY-MM-DD', 'YYYY/MM/DD', 'MM-DD-YYYY', or 'MM/DD/YYYY'."));
    }

    tm.tm_isdst = -1; // Let mktime determine DST

    // Calculate start of day (00:00:00)
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    std::time_t start_tt = std::mktime(&tm);
    if (start_tt == -1 || !isTmValid(tm_orig_for_validation, tm)) { // Validate after mktime
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Failed to convert start of day for date range (mktime failed or invalid date)."));
    }
    auto startOfDay = std::chrono::system_clock::from_time_t(start_tt);

    // Calculate end of day by adding 24 hours and subtracting 1 second
    // This correctly handles DST transitions.
    auto endOfDay = startOfDay + std::chrono::days(1) - std::chrono::seconds(1);
    
    return std::make_pair(startOfDay, endOfDay);
}

} // namespace Utils
