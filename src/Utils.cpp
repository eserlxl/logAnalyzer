#include "../include/Utils.h"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <map>
#include <regex>

namespace Utils {

LogLevel stringToLogLevel(const std::string &levelStr) {
    if (levelStr == "DEBUG") return LogLevel::DEBUG;
    if (levelStr == "INFO") return LogLevel::INFO;
    if (levelStr == "WARNING") return LogLevel::WARNING;
    if (levelStr == "ERROR") return LogLevel::ERROR;
    if (levelStr == "FATAL") return LogLevel::FATAL;
    if (levelStr == "TRACE") return LogLevel::TRACE;
    return LogLevel::UNKNOWN;
}

std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN";
}

std::string formatTimestamp(std::chrono::system_clock::time_point tp, std::string_view format) {
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::localtime(&tt); // Or gmtime for UTC
    std::ostringstream ss;
    ss << std::put_time(&tm, format.data());
    return ss.str();
}

void replaceAll(std::string &str, const std::string &from, const std::string &to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

std::expected<std::chrono::seconds, std::string> parseDuration(const std::string& durationStr) {
    std::regex durationRegex("^(\\d+)([smhd])$");
    std::smatch matches;

    if (std::regex_match(durationStr, matches, durationRegex)) {
        int value = std::stoi(matches[1].str());
        char unit = matches[2].str()[0];

        switch (unit) {
            case 's': return std::chrono::seconds(value);
            case 'm': return std::chrono::minutes(value);
            case 'h': return std::chrono::hours(value);
            case 'd': return std::chrono::hours(value * 24);
            default: return std::unexpected("Unknown duration unit.");
        }
    }
    return std::unexpected("Invalid duration format. Expected formats like '10s', '5m', '2h', '1d'.");
}

std::expected<std::chrono::system_clock::time_point, std::string> parseRelativeTime(const std::string& timeStr) {
    std::regex relativeTimeRegex("^(\\d+)([smhd]) ago$");
    std::smatch matches;

    if (std::regex_match(timeStr, matches, relativeTimeRegex)) {
        int value = std::stoi(matches[1].str());
        char unit = matches[2].str()[0];

        auto now = std::chrono::system_clock::now();
        std::chrono::seconds duration_seconds;

        switch (unit) {
            case 's': duration_seconds = std::chrono::seconds(value); break;
            case 'm': duration_seconds = std::chrono::minutes(value); break;
            case 'h': duration_seconds = std::chrono::hours(value); break;
            case 'd': duration_seconds = std::chrono::hours(value * 24); break;
            default: return std::unexpected("Unknown time unit in 'ago' expression.");
        }
        return now - duration_seconds;
    }

    return std::unexpected("Invalid relative time format. Expected formats like '10s ago', '5m ago', '2h ago', '1d ago'.");
}

std::expected<std::chrono::system_clock::time_point, std::string> parseAbsoluteTime(const std::string& timeStr) {
    std::tm tm = {};
    std::stringstream ss(timeStr);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        return std::unexpected("Invalid absolute time format. Expected 'YYYY-MM-DD HH:MM:SS'.");
    }
    auto timePoint = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    return timePoint;
}

} // namespace Utils
