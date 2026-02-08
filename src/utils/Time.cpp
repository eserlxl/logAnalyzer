// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "utils/Time.h" // Include the new header
#include <chrono> // Added for std::chrono
#include <sstream>
#include <iomanip>
#include <regex>
#include <mutex>
#include <iostream>
#include <limits>

namespace Utils {

static std::mutex localtimeMutex;

bool isTmValid(const std::tm& tm_orig, std::tm& tm_new) {
    // Before comparing, adjust tm_new's daylight saving flag to match the original.
    // This is because std::get_time doesn't set tm_isdst, but mktime does.
    tm_new.tm_isdst = tm_orig.tm_isdst;
    
    // Check if mktime/timegm normalized any other fields
    return tm_new.tm_year == tm_orig.tm_year &&
           tm_new.tm_mon == tm_orig.tm_mon &&
           tm_new.tm_mday == tm_orig.tm_mday &&
           tm_new.tm_hour == tm_orig.tm_hour &&
           tm_new.tm_min == tm_orig.tm_min &&
           tm_new.tm_sec == tm_orig.tm_sec;
}

// Portable timegm implementation
time_t portable_timegm(struct tm *tm) {
#if defined(_WIN32)
    return _mkgmtime(tm);
#else
    return timegm(tm);
#endif
}

// Implementation for Iteration 5 Feature Design
// 1. Thread-Safe Time Handling
std::string format_utc(const std::chrono::system_clock::time_point& tp, const std::string& format_str) {
    // Use std::lock_guard for thread-safe access to gmtime, as std::gmtime
    // returns a pointer to static data that might be shared.
    std::lock_guard<std::mutex> lock(localtimeMutex);

    // Convert time_point to time_t
    std::time_t time = std::chrono::system_clock::to_time_t(tp);

    // Use std::gmtime for UTC conversion
    std::tm tm_utc = *std::gmtime(&time);

    std::stringstream ss;
    ss << std::put_time(&tm_utc, format_str.c_str());
    return ss.str();
}

std::chrono::system_clock::time_point parse_utc(const std::string& time_str, const std::string& format_str) {
    std::tm tm_utc = {};
    std::stringstream ss(time_str);

    std::lock_guard<std::mutex> lock(localtimeMutex);

    ss >> std::get_time(&tm_utc, format_str.c_str());

    if (ss.fail()) {
        throw std::runtime_error("Failed to parse UTC time string: " + time_str + " with format: " + format_str);
    }

    time_t time = portable_timegm(&tm_utc);
    if (time == (time_t)-1) {
         throw std::runtime_error("Failed to convert parsed tm to time_t for UTC string: " + time_str);
    }

    return std::chrono::system_clock::from_time_t(time);
}

// Iteration 5: Explicit Time Zone Support
std::string format_zone(const std::chrono::system_clock::time_point& tp, const tz::Zone* zone, const std::string& format_str) {
    if (!zone) {
        // Or handle error appropriately, e.g., throw or return an error string
        return "Invalid time zone";
    }

    // Create a zoned_time object, which associates the time_point with the time_zone
    auto zt = std::chrono::zoned_time(zone, tp);
    
    // Get the time_point in the system clock's frame
    auto sys_tp = zt.get_sys_time();

    // Convert to time_t
    auto t = std::chrono::system_clock::to_time_t(sys_tp);

    std::tm tm_local;
    {
        std::lock_guard<std::mutex> lock(localtimeMutex);
        tm_local = *std::localtime(&t);
    }

    std::stringstream ss;
    ss << std::put_time(&tm_local, format_str.c_str());
    return ss.str();
}

std::chrono::system_clock::time_point parse_zone(const std::string& time_str, const tz::Zone* zone, const std::string& format_str) {
    if (!zone) {
        throw std::runtime_error("Invalid time zone provided for parsing.");
    }

    std::chrono::system_clock::time_point tp;
    std::stringstream ss(time_str);
    ss >> std::chrono::parse(format_str, tp);

    if (ss.fail()) {
        throw std::runtime_error("Failed to parse time string with format.");
    }

    // The parsed time_point is naive; we need to associate it with the zone.
    // We create a local_time, then convert it to a zoned_time.
    auto local = tp.time_since_epoch();
    std::chrono::local_time<std::chrono::system_clock::duration> local_tp(local);
    
    // This gives us a zoned_time, which correctly represents the time in the given zone.
    auto zt = std::chrono::zoned_time(zone, local_tp);

    return zt.get_sys_time();
}

std::chrono::system_clock::time_point convert_zone(const std::chrono::system_clock::time_point& tp, const tz::Zone* from_zone, const tz::Zone* to_zone) {
    if (!from_zone || !to_zone) {
        throw std::runtime_error("Invalid time zone provided for conversion.");
    }

    // Create a zoned_time representing the input timepoint in the 'from' zone
    auto zt_from = std::chrono::zoned_time(from_zone, tp);

    // Get the local_time from the 'from' zone
    auto local_tp = zt_from.get_local_time();

    // Create a new zoned_time by interpreting the local_time in the 'to' zone
    auto zt_to = std::chrono::zoned_time(to_zone, local_tp);

    // Return the system_clock time_point of the new zoned_time
    return zt_to.get_sys_time();
}

// Iteration 5: Date and Time Arithmetic
std::chrono::system_clock::time_point add_duration(const std::chrono::system_clock::time_point& tp, std::chrono::seconds duration) {
    return tp + duration;
}
std::chrono::system_clock::time_point add_duration(const std::chrono::system_clock::time_point& tp, std::chrono::minutes duration) {
    return tp + duration;
}
std::chrono::system_clock::time_point add_duration(const std::chrono::system_clock::time_point& tp, std::chrono::hours duration) {
    return tp + duration;
}
std::chrono::system_clock::time_point add_duration(const std::chrono::system_clock::time_point& tp, std::chrono::days duration) {
    return tp + duration;
}

std::chrono::system_clock::time_point subtract_duration(const std::chrono::system_clock::time_point& tp, std::chrono::seconds duration) {
    return tp - duration;
}
std::chrono::system_clock::time_point subtract_duration(const std::chrono::system_clock::time_point& tp, std::chrono::minutes duration) {
    return tp - duration;
}
std::chrono::system_clock::time_point subtract_duration(const std::chrono::system_clock::time_point& tp, std::chrono::hours duration) {
    return tp - duration;
}
std::chrono::system_clock::time_point subtract_duration(const std::chrono::system_clock::time_point& tp, std::chrono::days duration) {
    return tp - duration;
}

// Iteration 5: High-Resolution Timing
HighResTimer::HighResTimer() : running(false) {}

void HighResTimer::start() {
    startTime = std::chrono::high_resolution_clock::now();
    running = true;
}

void HighResTimer::stop() {
    stopTime = std::chrono::high_resolution_clock::now();
    running = false;
}

double HighResTimer::elapsedSeconds() const {
    auto end = running ? std::chrono::high_resolution_clock::now() : stopTime;
    return std::chrono::duration<double>(end - startTime).count();
}

double HighResTimer::elapsedMilliseconds() const {
    auto end = running ? std::chrono::high_resolution_clock::now() : stopTime;
    return std::chrono::duration<double, std::milli>(end - startTime).count();
}

double HighResTimer::elapsedMicroseconds() const {
    auto end = running ? std::chrono::high_resolution_clock::now() : stopTime;
    return std::chrono::duration<double, std::micro>(end - startTime).count();
}

double HighResTimer::elapsedNanoseconds() const {
    auto end = running ? std::chrono::high_resolution_clock::now() : stopTime;
    return std::chrono::duration<double, std::nano>(end - startTime).count();
}

// Iteration 5: Enhanced Parsing and Formatting
std::string format_iso8601(const std::chrono::system_clock::time_point& tp) {
    return format_utc(tp, "%Y-%m-%dT%H:%M:%SZ");
}

// Placeholder implementations for missing functions to allow linking
std::string formatTimestamp(std::chrono::system_clock::time_point tp, std::string_view format) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;
    std::stringstream ss;

    // Use a mutex to make localtime thread-safe
    std::lock_guard<std::mutex> lock(localtimeMutex);
    tm = *localtime(&t);

    ss << std::put_time(&tm, std::string(format).c_str());
    return ss.str();
}

std::expected<std::chrono::microseconds, ErrorCode::Error> parseDuration(const std::string& durationStr, bool allowExtendedUnits) {
    if (durationStr.empty()) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Empty duration string."));
    }

    std::smatch matches;
    // Anchor the regex to the entire string and use alternations properly
    const std::regex duration_regex(R"(^(\d+)\s*(ms|us|s|second|seconds|m|minute|minutes|h|hour|hours|d|day|days|w|week|weeks|M|month|months|y|year|years)$)");

    if (!std::regex_match(durationStr, matches, duration_regex)) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid duration format: " + durationStr));
    }

    long long value;
    try {
        value = std::stoll(matches[1].str());
    } catch (const std::exception&) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Duration value out of range: " + durationStr));
    }

    const auto toMicroseconds = [&](long long multiplier) -> std::expected<std::chrono::microseconds, ErrorCode::Error> {
        const auto maxMicros = std::chrono::microseconds::max().count();
        if (value < 0 || multiplier <= 0 || value > maxMicros / multiplier) {
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Duration value out of range: " + durationStr));
        }
        return std::chrono::microseconds(value * multiplier);
    };

    constexpr long long kMicrosPerMicrosecond = 1LL;
    constexpr long long kMicrosPerMillisecond = 1000LL;
    constexpr long long kMicrosPerSecond = 1000LL * 1000LL;
    constexpr long long kMicrosPerMinute = 60LL * kMicrosPerSecond;
    constexpr long long kMicrosPerHour = 60LL * kMicrosPerMinute;
    constexpr long long kMicrosPerDay = 24LL * kMicrosPerHour;
    constexpr long long kMicrosPerWeek = 7LL * kMicrosPerDay;
    constexpr long long kMicrosPerMonthApprox = 30LL * kMicrosPerDay;
    constexpr long long kMicrosPerYearApprox = 365LL * kMicrosPerDay;

    std::string unit = matches[2].str();
        if (unit == "s" || unit == "second" || unit == "seconds") return toMicroseconds(kMicrosPerSecond);
        if (unit == "m" || unit == "minute" || unit == "minutes") return toMicroseconds(kMicrosPerMinute);
        if (unit == "h" || unit == "hour" || unit == "hours") return toMicroseconds(kMicrosPerHour);
        if (unit == "d" || unit == "day" || unit == "days") return toMicroseconds(kMicrosPerDay);
        
        if (allowExtendedUnits) {
            if (unit == "ms") return toMicroseconds(kMicrosPerMillisecond);
            if (unit == "us") return toMicroseconds(kMicrosPerMicrosecond);
            if (unit == "w" || unit == "week" || unit == "weeks") return toMicroseconds(kMicrosPerWeek);
            if (unit == "M" || unit == "month" || unit == "months") return toMicroseconds(kMicrosPerMonthApprox); // Approximation
            if (unit == "y" || unit == "year" || unit == "years") return toMicroseconds(kMicrosPerYearApprox); // Approximation
        }
    
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Unsupported duration unit: " + unit));
    }
    

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseRelativeTime(const std::string& timeStr) {
    auto now = std::chrono::system_clock::now();

    if (timeStr == "yesterday") {
        return now - std::chrono::days(1);
    }
    if (timeStr == "tomorrow") {
        return now + std::chrono::days(1);
    }
    
    std::smatch matches;
    const std::regex ago_regex(R"((\d+\s*[a-zA-Z]+) ago)");
    const std::regex in_regex(R"(in (\d+\s*[a-zA-Z]+))");

    if (std::regex_match(timeStr, matches, ago_regex)) {
        auto duration_str = matches[1].str();
        auto duration = parseDuration(duration_str, true);
        if (duration) {
            return now - *duration;
        } else {
            return std::unexpected(duration.error());
        }
    }

    if (std::regex_match(timeStr, matches, in_regex)) {
        auto duration_str = matches[1].str();
        auto duration = parseDuration(duration_str, true);
        if (duration) {
            return now + *duration;
        } else {
            return std::unexpected(duration.error());
        }
    }

    return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid relative time format: " + timeStr));
}

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseAbsoluteTime(const std::string& timeStr) {
    std::tm tm = {};
    std::stringstream ss(timeStr);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

    if (ss.fail()) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid absolute time format: " + timeStr));
    }
    
    std::tm tm_orig = tm;
    std::time_t time;
    {
        std::lock_guard<std::mutex> lock(localtimeMutex);
        time = std::mktime(&tm);
    }
    if (time == -1 || !isTmValid(tm_orig, tm)) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Failed to convert to time_t or invalid date: " + timeStr));
    }

    return std::chrono::system_clock::from_time_t(time);
}

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseISO8601(const std::string& timeStr) {
    std::string format;

    // Regex to handle ISO 8601 with optional 'Z' or offset
    std::regex iso_regex(R"((\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})(Z|([+-]\d{2}:\d{2}))?)");
    std::smatch match;

    if (!std::regex_match(timeStr, match, iso_regex)) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid ISO 8601 format: " + timeStr));
    }
    
    std::string datetime_part = match[1];
    std::string timezone_part = match[2];

    std::stringstream ss(datetime_part);
    std::tm tm = {};
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    if (ss.fail()) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Failed to parse date/time part of ISO string: " + datetime_part));
    }

    std::tm tm_orig = tm;

    if (timezone_part.empty()) { // Local time
        std::time_t time;
        {
            std::lock_guard<std::mutex> lock(localtimeMutex);
            time = std::mktime(&tm);
        }
        if (time == -1 || !isTmValid(tm_orig, tm)) {
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Failed to convert local time to time_t: " + datetime_part));
        }
        return std::chrono::system_clock::from_time_t(time);
    } else if (timezone_part == "Z") { // UTC
        time_t time = portable_timegm(&tm);
        if (time == -1 || !isTmValid(tm_orig, tm)) {
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Failed to convert UTC time to time_t: " + datetime_part));
        }
        return std::chrono::system_clock::from_time_t(time);
    } else { // UTC with offset
        time_t time = portable_timegm(&tm);
        if (time == -1 || !isTmValid(tm_orig, tm)) {
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Failed to convert base time to time_t for offset calculation: " + datetime_part));
        }
        
        int offset_hours = std::stoi(timezone_part.substr(1, 2));
        int offset_minutes = std::stoi(timezone_part.substr(4, 2));
        if (offset_hours >= 24) {
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid offset hours: " + timeStr));
        }
        if (offset_minutes >= 60) {
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid offset minutes: " + timeStr));
        }
        int total_offset_seconds = (offset_hours * 3600) + (offset_minutes * 60);
        if (timezone_part[0] == '-') {
            time += total_offset_seconds;
        } else {
            time -= total_offset_seconds;
        }
        return std::chrono::system_clock::from_time_t(time);
    }
}

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseTime(const std::string& timeStr) {
    // Try ISO 8601
    auto res_iso = parseISO8601(timeStr);
    if (res_iso) return res_iso;

    // Try Absolute Time
    auto res_abs = parseAbsoluteTime(timeStr);
    if (res_abs) return res_abs;

    // Try Relative Time
    auto res_rel = parseRelativeTime(timeStr);
    if (res_rel) return res_rel;

    return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Unsupported time format: " + timeStr));
}

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error>
parseTimeWithFormats(const std::string& timeStr, const std::vector<std::string>& formats) {
    if (formats.empty()) {
        return parseTime(timeStr);
    }
    
    for (const auto& fmt : formats) {
        std::tm tm = {};
        std::stringstream ss(timeStr);
        
        // Clear eofbit before parsing, as get_time may not proceed if it's set
        ss.clear();
        ss >> std::get_time(&tm, fmt.c_str());

        if (!ss.fail()) {
            ss >> std::ws;
            if (ss.eof()) {
                std::tm tm_orig = tm;
                std::time_t t;
                {
                    std::lock_guard<std::mutex> lock(localtimeMutex);
                    t = std::mktime(&tm);
                }
                
                // Use the updated isTmValid function
                if (t != (time_t)-1 && isTmValid(tm_orig, tm)) {
                    return std::chrono::system_clock::from_time_t(t);
                }
            }
        }
    }
    return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Failed to parse time with provided formats: " + timeStr));
}

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> validateTimestampCliOption(const std::string &tsStr) {
    return parseTime(tsStr);
}

std::expected<std::pair<std::chrono::system_clock::time_point, std::chrono::system_clock::time_point>, ErrorCode::Error>
parseDayRange(const std::string& dateString) {
    std::tm tm = {};
    const std::vector<std::string> formats = {"%Y-%m-%d", "%Y/%m/%d", "%m-%d-%Y", "%m/%d/%Y"};

    for (const auto& format : formats) {
        std::stringstream ss(dateString);
        ss >> std::get_time(&tm, format.c_str());
        if (!ss.fail() && ss.eof()) {
            tm.tm_hour = 0;
            tm.tm_min = 0;
            tm.tm_sec = 0;
            
            std::time_t start_time;
            {
                std::lock_guard<std::mutex> lock(localtimeMutex);
                start_time = std::mktime(&tm);
            }
            if (start_time != -1) {
                auto start_tp = std::chrono::system_clock::from_time_t(start_time);
                // Fix: Ensure the end time covers the entire day by setting it to the start of the next day.
                // The TimeRangeFilter uses [start, end) semantics, so this includes all times up to 23:59:59.999...
                auto end_tp = start_tp + std::chrono::days(1);
                return std::make_pair(start_tp, end_tp);
            }
        }
    }

    return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid date format: " + dateString));
}


} // namespace Utils
