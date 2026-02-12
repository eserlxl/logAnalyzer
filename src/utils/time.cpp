// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "utils/time.h"
#include "utils/string.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <mutex>

namespace Utils {

std::mutex localtimeMutex;

bool isTmValid(const std::tm& tm_orig, std::tm& tm_new) {
    tm_new.tm_isdst = tm_orig.tm_isdst;
    return tm_new.tm_year == tm_orig.tm_year &&
           tm_new.tm_mon == tm_orig.tm_mon &&
           tm_new.tm_mday == tm_orig.tm_mday &&
           tm_new.tm_hour == tm_orig.tm_hour &&
           tm_new.tm_min == tm_orig.tm_min &&
           tm_new.tm_sec == tm_orig.tm_sec;
}

time_t portable_timegm(struct tm *tm) {
#if defined(_WIN32)
    return _mkgmtime(tm);
#else
    return timegm(tm);
#endif
}

std::string format_utc(const std::chrono::system_clock::time_point& tp, const std::string& format_str) {
    std::lock_guard<std::mutex> lock(localtimeMutex);
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_utc = *std::gmtime(&time);
    std::stringstream ss;
    ss << std::put_time(&tm_utc, format_str.c_str());
    return ss.str();
}

std::chrono::system_clock::time_point parse_utc(const std::string& time_str, const std::string& format_str) {
    std::tm tm_utc = {};
    std::stringstream ss(time_str);
    std::tm tm_orig = {};
    std::lock_guard<std::mutex> lock(localtimeMutex);
    ss >> std::get_time(&tm_utc, format_str.c_str());
    tm_orig = tm_utc;
    if (ss.fail()) throw std::runtime_error("Failed to parse UTC time string: " + time_str);
    ss >> std::ws;
    if (!ss.eof()) throw std::runtime_error("Trailing characters in UTC time string: " + time_str);
    time_t time = portable_timegm(&tm_utc);
    if (time == (time_t)-1 || !isTmValid(tm_orig, tm_utc)) throw std::runtime_error("Failed to convert parsed tm to time_t for UTC string: " + time_str);
    return std::chrono::system_clock::from_time_t(time);
}

std::string format_zone(const std::chrono::system_clock::time_point& tp, const tz::Zone* zone, const std::string& format_str) {
    if (!zone) return "Invalid time zone";
    auto zt = std::chrono::zoned_time(zone, tp);
    auto sys_tp = zt.get_sys_time();
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
    if (!zone) throw std::runtime_error("Invalid time zone provided for parsing.");
    std::chrono::system_clock::time_point tp;
    std::stringstream ss(time_str);
    ss >> std::chrono::parse(format_str, tp);
    if (ss.fail()) throw std::runtime_error("Failed to parse time string with format.");
    auto local = tp.time_since_epoch();
    std::chrono::local_time<std::chrono::system_clock::duration> local_tp(local);
    auto zt = std::chrono::zoned_time(zone, local_tp);
    return zt.get_sys_time();
}

std::chrono::system_clock::time_point convert_zone(const std::chrono::system_clock::time_point& tp, const tz::Zone* from_zone, const tz::Zone* to_zone) {
    if (!from_zone || !to_zone) throw std::runtime_error("Invalid time zone provided for conversion.");
    auto zt_from = std::chrono::zoned_time(from_zone, tp);
    auto local_tp = zt_from.get_local_time();
    auto zt_to = std::chrono::zoned_time(to_zone, local_tp);
    return zt_to.get_sys_time();
}

std::chrono::system_clock::time_point add_duration(const std::chrono::system_clock::time_point& tp, std::chrono::seconds duration) { return tp + duration; }
std::chrono::system_clock::time_point add_duration(const std::chrono::system_clock::time_point& tp, std::chrono::minutes duration) { return tp + duration; }
std::chrono::system_clock::time_point add_duration(const std::chrono::system_clock::time_point& tp, std::chrono::hours duration) { return tp + duration; }
std::chrono::system_clock::time_point add_duration(const std::chrono::system_clock::time_point& tp, std::chrono::days duration) { return tp + duration; }

std::chrono::system_clock::time_point subtract_duration(const std::chrono::system_clock::time_point& tp, std::chrono::seconds duration) { return tp - duration; }
std::chrono::system_clock::time_point subtract_duration(const std::chrono::system_clock::time_point& tp, std::chrono::minutes duration) { return tp - duration; }
std::chrono::system_clock::time_point subtract_duration(const std::chrono::system_clock::time_point& tp, std::chrono::hours duration) { return tp - duration; }
std::chrono::system_clock::time_point subtract_duration(const std::chrono::system_clock::time_point& tp, std::chrono::days duration) { return tp - duration; }

std::string format_iso8601(const std::chrono::system_clock::time_point& tp) { return format_utc(tp, "%Y-%m-%dT%H:%M:%SZ"); }

std::chrono::system_clock::time_point parse_iso8601(const std::string& time_str) {
    auto res = parseISO8601(time_str);
    if (!res) throw std::runtime_error("Failed to parse ISO 8601 time string: " + time_str + ". Error: " + res.error().message);
    return *res;
}

std::string formatTimestamp(std::chrono::system_clock::time_point tp, std::string_view format) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;
    std::stringstream ss;
    std::lock_guard<std::mutex> lock(localtimeMutex);
    tm = *localtime(&t);
    ss << std::put_time(&tm, std::string(format).c_str());
    return ss.str();
}

} // namespace Utils
