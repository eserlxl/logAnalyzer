// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "utils/time.h"
#include "utils/string.h"
#include <regex>
#include <charconv>
#include <sstream>
#include <iomanip>
#include <mutex>

namespace Utils {

extern std::mutex localtimeMutex;

namespace {
std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseUnixTimestampStrict(std::string_view input) {
    if (input.empty()) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Empty Unix timestamp."));
    }

    long long seconds = 0;
    const auto [ptr, ec] = std::from_chars(input.data(), input.data() + input.size(), seconds);
    if (ec != std::errc{} || ptr != input.data() + input.size()) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid Unix timestamp: " + std::string(input)));
    }

    const auto minSecs = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::time_point::min().time_since_epoch()).count();
    const auto maxSecs = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::time_point::max().time_since_epoch()).count();
    if (seconds < minSecs || seconds > maxSecs) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Unix timestamp out of range: " + std::string(input)));
    }

    return std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
}
} // namespace

std::expected<std::chrono::microseconds, ErrorCode::Error> parseDuration(const std::string& durationStr, [[maybe_unused]] bool allowExtendedUnits) {
    if (durationStr.empty()) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Empty duration string."));
    }

    std::smatch matches;
    const std::regex duration_regex(R"(^(\d+)\s*(ms|us|s|second|seconds|m|minute|minutes|h|hour|hours|d|day|days|w|week|weeks|M|month|months|y|year|years)$)");

    if (!std::regex_match(durationStr, matches, duration_regex)) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid duration format: " + durationStr));
    }

    long long value = 0;
    const auto& numberStr = matches[1].str();
    const auto [ptr, ec] = 
        std::from_chars(numberStr.data(), numberStr.data() + numberStr.size(), value);
    if (ec != std::errc{} || ptr != numberStr.data() + numberStr.size()) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Duration value out of range: " + durationStr));
    }

    const auto toMicroseconds = [&](long long multiplier) -> std::expected<std::chrono::microseconds, ErrorCode::Error> {
        const auto maxMicros = std::chrono::microseconds::max().count();
        if (value < 0 || multiplier <= 0 || value > maxMicros / multiplier) {
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Duration value out of range: " + durationStr));
        }
        return std::chrono::microseconds(value * multiplier);
    };

    constexpr long long microsPerMicrosecond = 1LL;
    constexpr long long microsPerMillisecond = 1000LL;
    constexpr long long microsPerSecond = 1000LL * 1000LL;
    constexpr long long microsPerMinute = 60LL * microsPerSecond;
    constexpr long long microsPerHour = 60LL * microsPerMinute;
    constexpr long long microsPerDay = 24LL * microsPerHour;
    constexpr long long microsPerWeek = 7LL * microsPerDay;
    constexpr long long microsPerMonthApprox = 30LL * microsPerDay;
    constexpr long long microsPerYearApprox = 365LL * microsPerDay;

    std::string unit = matches[2].str();
    if (unit == "s" || unit == "second" || unit == "seconds") return toMicroseconds(microsPerSecond);
    if (unit == "m" || unit == "minute" || unit == "minutes") return toMicroseconds(microsPerMinute);
    if (unit == "h" || unit == "hour" || unit == "hours") return toMicroseconds(microsPerHour);
    if (unit == "d" || unit == "day" || unit == "days") return toMicroseconds(microsPerDay);
    
    // For smaller units and longer approx units
    if (!unit.empty()) {
        if (unit == "ms") return toMicroseconds(microsPerMillisecond);
        if (unit == "us") return toMicroseconds(microsPerMicrosecond);
        if (unit == "w" || unit == "week" || unit == "weeks") return toMicroseconds(microsPerWeek);
        if (unit == "M" || unit == "month" || unit == "months") return toMicroseconds(microsPerMonthApprox);
        if (unit == "y" || unit == "year" || unit == "years") return toMicroseconds(microsPerYearApprox);
    }

    return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Unsupported duration unit: " + unit));
}

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> parseRelativeTime(const std::string& timeStr) {
    auto now = std::chrono::system_clock::now();
    const std::string lowered = Utils::toLower(timeStr); 

    if (lowered == "yesterday") {
        return now - std::chrono::days(1);
    }
    if (lowered == "tomorrow") {
        return now + std::chrono::days(1);
    }
    
    std::smatch matches;
    const std::regex ago_regex(R"(^(\d+\s*[a-zA-Z]+)\s+ago$)", std::regex::icase);
    const std::regex in_regex(R"(^in\s+(\d+\s*[a-zA-Z]+)$)", std::regex::icase);

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
    ss >> std::ws;
    if (!ss.eof()) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid absolute time format: " + timeStr));
    }
    
    std::tm tm_orig = tm;
    tm.tm_isdst = -1;
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
    std::regex iso_regex(R"((\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})(Z|([+-]\d{2}:\d{2}))?)", std::regex::icase);
    std::smatch match;

    if (!std::regex_match(timeStr, match, iso_regex)) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid ISO 8601 format: " + timeStr));
    }
    
    std::string datetime_part = match[1];
    std::string timezone_part = Utils::toUpper(match[2].str());

    std::stringstream ss(datetime_part);
    std::tm tm = {};
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    if (ss.fail()) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Failed to parse date/time part of ISO string: " + datetime_part));
    }

    std::tm tm_orig = tm;

    if (timezone_part.empty()) { // Local time
        tm.tm_isdst = -1;
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
        
        int offset_hours = 0;
        int offset_minutes = 0;
        const char* hoursStart = timezone_part.data() + 1;
        const char* minsStart = timezone_part.data() + 4;
        const auto [hptr, hec] = std::from_chars(hoursStart, hoursStart + 2, offset_hours);
        const auto [mptr, mec] = std::from_chars(minsStart, minsStart + 2, offset_minutes);
        if (hec != std::errc{} || hptr != hoursStart + 2 ||
            mec != std::errc{} || mptr != minsStart + 2) {
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid offset value: " + timeStr));
        }
        
        if (offset_hours > 23 || offset_minutes > 59) {
            return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Offset hours must be < 24 and minutes < 60: " + timeStr));
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
    const std::string trimmed = Utils::trim(timeStr); 
    if (trimmed.empty()) {
        return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Empty time value."));
    }

    auto unixTs = parseUnixTimestampStrict(trimmed);
    if (unixTs) return *unixTs;

    auto res_iso = parseISO8601(trimmed);
    if (res_iso) return res_iso;

    auto res_abs = parseAbsoluteTime(trimmed);
    if (res_abs) return res_abs;

    auto res_rel = parseRelativeTime(trimmed);
    if (res_rel) return res_rel;

    return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Unsupported time format: " + trimmed));
}

std::expected<std::chrono::system_clock::time_point, ErrorCode::Error>
parseTimeWithFormats(const std::string& timeStr, const std::vector<std::string>& formats) {
    if (formats.empty()) return parseTime(timeStr);
    
    for (const auto& fmt : formats) {
        std::tm tm = {};
        std::stringstream ss(timeStr);
        ss.clear();
        ss >> std::get_time(&tm, fmt.c_str());

        if (!ss.fail()) {
            ss >> std::ws;
            if (ss.eof()) {
                std::tm tm_orig = tm;
                tm.tm_isdst = -1;
                std::time_t t;
                {
                    std::lock_guard<std::mutex> lock(localtimeMutex);
                    t = std::mktime(&tm);
                }
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
    const std::vector<std::string> formats = {"%Y-%m-%d", "%Y/%m/%d", "%m-%d-%Y", "%m/%d/%Y"};

    for (const auto& format : formats) {
        std::tm tm = {};
        std::stringstream ss(dateString);
        ss >> std::get_time(&tm, format.c_str());
        if (!ss.fail() && ss.eof()) {
            tm.tm_hour = 0;
            tm.tm_min = 0;
            tm.tm_sec = 0;
            std::tm tm_orig = tm;
            tm.tm_isdst = -1;
            
            std::time_t start_time;
            {
                std::lock_guard<std::mutex> lock(localtimeMutex);
                start_time = std::mktime(&tm);
            }
            if (start_time != -1 && isTmValid(tm_orig, tm)) {
                auto start_tp = std::chrono::system_clock::from_time_t(start_time);
                auto end_tp = start_tp + std::chrono::days(1);
                return std::make_pair(start_tp, end_tp);
            }
        }
    }

    return std::unexpected(ErrorCode::Error(Code::TimestampParsingFailed, "Invalid date format: " + dateString));
}

} // namespace Utils
