#include "../include/Utils.h"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <map>
#include <regex>
#include <vector> // Required for std::vector in split
#include <filesystem> // Required for std::filesystem utilities
#include <ctime>      // Required for std::mktime, std::time_t, std::tm, and timegm (non-standard but common)
#include <CLI/CLI.hpp> // Required for CLI::ValidationError
// No need to explicitly include <string> as it's included by Utils.h
// No need to explicitly include <locale> for ::tolower/::toupper in this context,
// but it's good to be aware for wider character sets.

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

LogLevel stringToLogLevelIgnoreCase(const std::string &levelStr) {
    std::string upperLevelStr = levelStr;
    std::transform(upperLevelStr.begin(), upperLevelStr.end(), upperLevelStr.begin(),
                   ::toupper);
    return stringToLogLevel(upperLevelStr);
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

void replaceAllIgnoreCase(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return;
    }
    std::string lowerStr = toLower(str);
    std::string lowerFrom = toLower(from);
    size_t start_pos = 0;
    while ((start_pos = lowerStr.find(lowerFrom, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        lowerStr.replace(start_pos, from.length(), toLower(to)); // Keep lowerStr in sync
        start_pos += to.length();
    }
}

std::string trim(const std::string& str, const std::string& whitespace) {
    const size_t strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; // no content

    const size_t strEnd = str.find_last_not_of(whitespace);
    const size_t strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string toLower(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
                   ::tolower);
    return lowerStr;
}

std::string toUpper(const std::string& str) {
    std::string upperStr = str;
    std::transform(upperStr.begin(), upperStr.end(), upperStr.begin(),
                   ::toupper);
    return upperStr;
}

// File System Utilities
bool fileExists(const std::string& filePath) {
    return std::filesystem::is_regular_file(filePath);
}

std::string getFileName(const std::string& filePath) {
    return std::filesystem::path(filePath).filename().string();
}

std::string getFileExtension(const std::string& filePath) {
    return std::filesystem::path(filePath).extension().string().erase(0, 1); // erase(0,1) to remove leading dot
}

std::string getDirectory(const std::string& filePath) {
    return std::filesystem::path(filePath).parent_path().string();
}

std::expected<std::chrono::seconds, std::string> parseDuration(const std::string& durationStr, bool allowExtendedUnits) {
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
                return std::unexpected("Unknown duration unit with extended units enabled.");
            }
        } else {
            return std::unexpected("Unknown duration unit. Extended units are not enabled.");
        }
        return total_seconds;
    }
    return std::unexpected("Invalid duration format. Expected formats like '10s', '5m', '2h', '1d' or extended units if enabled.");
}

std::expected<std::chrono::system_clock::time_point, std::string> parseRelativeTime(const std::string& timeStr) {
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
            default: return std::unexpected("Unknown time unit in 'ago' expression.");
        }
        return now - duration_seconds;
    }

    // Handle "in X units"
    std::regex relativeTimeInRegex("^in (\\d+)([smhd])$");
    if (std::regex_match(timeStr, matches, relativeTimeInRegex)) {
        long long value = std::stoll(matches[1].str());
        char unit = matches[2].str()[0];
        switch (unit) {
            case 's': duration_seconds = std::chrono::seconds(value); break;
            case 'm': duration_seconds = std::chrono::minutes(value); break;
            case 'h': duration_seconds = std::chrono::hours(value); break;
            case 'd': duration_seconds = std::chrono::days(value); break;
            default: return std::unexpected("Unknown time unit in 'in' expression.");
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

    return std::unexpected("Invalid relative time format. Expected formats like '10s ago', 'in 5m', 'yesterday', 'tomorrow', 'next week'.");
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

// Helper to parse ISO 8601 with optional Z or offset
std::expected<std::chrono::system_clock::time_point, std::string> parseISO8601(const std::string& timeStr) {
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
    
    return std::unexpected("Invalid ISO 8601 format.");
}


std::expected<std::chrono::system_clock::time_point, std::string> parseTime(const std::string& timeStr) {
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

    return std::unexpected("Failed to parse time string. Unknown format.");
}

std::string escapeJsonString(const std::string& input) {
    std::ostringstream oss;
    for (char c : input) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 32) { // Control characters
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    oss << c;
                }
                break;
        }
    }
    return oss.str();
}

std::string globToRegex(const std::string& globPattern) {
    std::string regexPattern = "^"; // Anchor to the start of the string
    for (char c : globPattern) {
        switch (c) {
            case '*':
                regexPattern += ".*";
                break;
            case '?':
                regexPattern += ".";
                break;
            case '.':
            case '+':
            case '^':
            case '$':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '|':
            case '\\':
                regexPattern += '\\'; // Escape regex special characters
                regexPattern += c;
                break;
            default:
                regexPattern += c;
                break;
        }
    }
    regexPattern += "$"; // Anchor to the end of the string
    return regexPattern;
}

std::string validateTimestampCliOption(const std::string &tsStr) {
    if (tsStr.empty()) return tsStr; // Optional, so empty is fine
    auto timePointResult = Utils::parseTime(tsStr);
    if (timePointResult.has_value()) {
        return tsStr; // Return the string if successful
    }
    throw CLI::ValidationError("Invalid time format: " + timePointResult.error() + ". Expected formats: 'YYYY-MM-DD HH:MM:SS', ISO 8601, Unix timestamp, or relative time like '1h ago'.");
}

std::expected<std::pair<std::chrono::system_clock::time_point, std::chrono::system_clock::time_point>, std::string>
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

    return std::unexpected("Invalid date format for day range. Expected 'YYYY-MM-DD', 'YYYY/MM/DD', 'MM-DD-YYYY', or 'MM/DD/YYYY'.");

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

    // mktime can adjust tm_mday if tm_hour, tm_min, tm_sec cause overflow for that month/year.
    // Ensure endOfDay is indeed on the same date as startOfDay or the next day if time rolls over.
    // For simplicity, we directly set to 23:59:59.
    // If we want actual "end of day" with sub-second precision, it's typically start of next day minus epsilon.
    // For this context, 23:59:59 is sufficient.

    return std::make_pair(startOfDay, endOfDay);
}

// Helper to convert LogEntryField enum to string
std::string logEntryFieldToString(LogEntryField field) {
    switch (field) {
        case LogEntryField::TIMESTAMP: return "TIMESTAMP";
        case LogEntryField::LEVEL: return "LEVEL";
        case LogEntryField::MESSAGE: return "MESSAGE";
        case LogEntryField::SOURCE_FILE: return "SOURCE_FILE";
        case LogEntryField::LINE_NUMBER: return "LINE_NUMBER";
        case LogEntryField::THREAD_ID: return "THREAD_ID";
        case LogEntryField::MODULE: return "MODULE";
        case LogEntryField::HOST: return "HOST";
        case LogEntryField::CUSTOM: return "CUSTOM";
        case LogEntryField::STRUCTURED_FIELD: return "STRUCTURED_FIELD";
        default: return "UNKNOWN";
    }
}

// Helper to convert string to LogEntryField enum.
LogEntryField stringToLogEntryField(const std::string& fieldStr) {
    if (fieldStr == "TIMESTAMP") return LogEntryField::TIMESTAMP;
    if (fieldStr == "LEVEL") return LogEntryField::LEVEL;
    if (fieldStr == "MESSAGE") return LogEntryField::MESSAGE;
    if (fieldStr == "SOURCE_FILE") return LogEntryField::SOURCE_FILE;
    if (fieldStr == "LINE_NUMBER") return LogEntryField::LINE_NUMBER;
    if (fieldStr == "THREAD_ID") return LogEntryField::THREAD_ID;
    if (fieldStr == "MODULE") return LogEntryField::MODULE;
    if (fieldStr == "HOST") return LogEntryField::HOST;
    if (fieldStr == "CUSTOM") return LogEntryField::CUSTOM;
    if (fieldStr == "STRUCTURED_FIELD") return LogEntryField::STRUCTURED_FIELD;
    return LogEntryField::UNKNOWN;
}

// Helper to convert FilterOperator enum to string
std::string filterOperatorToString(FilterOperator op) {
    switch (op) {
        case FilterOperator::EQUALS: return "EQUALS";
        case FilterOperator::NOT_EQUALS: return "NOT_EQUALS";
        case FilterOperator::CONTAINS: return "CONTAINS";
        case FilterOperator::NOT_CONTAINS: return "NOT_CONTAINS"; // Corrected from DOES_NOT_CONTAIN
        case FilterOperator::STARTS_WITH: return "STARTS_WITH";
        case FilterOperator::ENDS_WITH: return "ENDS_WITH";
        case FilterOperator::GREATER_THAN: return "GREATER_THAN";
        case FilterOperator::LESS_THAN: return "LESS_THAN";
        case FilterOperator::GREATER_THAN_OR_EQUAL: return "GREATER_THAN_OR_EQUAL";
        case FilterOperator::LESS_THAN_OR_EQUAL: return "LESS_THAN_OR_EQUAL";
        default: return "UNKNOWN";
    }
}

// Helper to convert string to FilterOperator enum
FilterOperator stringToFilterOperator(const std::string& opStr) {
    if (opStr == "EQUALS") return FilterOperator::EQUALS;
    if (opStr == "NOT_EQUALS") return FilterOperator::NOT_EQUALS;
    if (opStr == "CONTAINS") return FilterOperator::CONTAINS;
    if (opStr == "NOT_CONTAINS") return FilterOperator::NOT_CONTAINS;
    if (opStr == "STARTS_WITH") return FilterOperator::STARTS_WITH;
    if (opStr == "ENDS_WITH") return FilterOperator::ENDS_WITH;
    if (opStr == "GREATER_THAN") return FilterOperator::GREATER_THAN;
    if (opStr == "LESS_THAN") return FilterOperator::LESS_THAN;
    if (opStr == "GREATER_THAN_OR_EQUAL") return FilterOperator::GREATER_THAN_OR_EQUAL;
    if (opStr == "LESS_THAN_OR_EQUAL") return FilterOperator::LESS_THAN_OR_EQUAL;
    return FilterOperator::UNKNOWN;
}

// Helper to convert FilterLogicalOperator to string
std::string filterLogicalOperatorToString(FilterLogicalOperator op) {
    switch (op) {
        case FilterLogicalOperator::AND: return "AND";
        case FilterLogicalOperator::OR: return "OR";
        case FilterLogicalOperator::NOT: return "NOT";
        default: return "UNKNOWN";
    }
}

// Helper to convert string to FilterLogicalOperator
FilterLogicalOperator stringToFilterLogicalOperator(const std::string& opStr) {
    if (opStr == "AND") return FilterLogicalOperator::AND;
    if (opStr == "OR") return FilterLogicalOperator::OR;
    if (opStr == "NOT") return FilterLogicalOperator::NOT;
    return FilterLogicalOperator::UNKNOWN;
}

// Helper to convert ExportFormat enum to string
std::string exportFormatToString(ExportFormat format) {
    switch (format) {
        case ExportFormat::PLAINTEXT: return "PLAINTEXT";
        case ExportFormat::JSON: return "JSON";
        case ExportFormat::CSV: return "CSV";
        case ExportFormat::XML: return "XML";
        default: return "UNKNOWN";
    }
}

// Helper to convert string to ExportFormat enum
ExportFormat stringToExportFormat(const std::string& formatStr) {
    if (formatStr == "PLAINTEXT") return ExportFormat::PLAINTEXT;
    if (formatStr == "JSON") return ExportFormat::JSON;
    if (formatStr == "CSV") return ExportFormat::CSV;
    if (formatStr == "XML") return ExportFormat::XML;
    return ExportFormat::UNKNOWN;
}

// Helper to convert StatisticType enum to string
std::string statisticTypeToString(StatisticType type) {
    switch (type) {
        case StatisticType::COUNT_BY_LEVEL: return "COUNT_BY_LEVEL";
        case StatisticType::TOP_N_OCCURRENCES: return "TOP_N_OCCURRENCES";
        case StatisticType::OCCURRENCE_COUNT: return "OCCURRENCE_COUNT";
        case StatisticType::SUM: return "SUM";
        case StatisticType::AVERAGE: return "AVERAGE";
        case StatisticType::MIN: return "MIN";
        case StatisticType::MAX: return "MAX";
        default: return "UNKNOWN";
    }
}

// Helper to convert string to StatisticType enum
StatisticType stringToStatisticType(const std::string& typeStr) {
    if (typeStr == "COUNT_BY_LEVEL") return StatisticType::COUNT_BY_LEVEL;
    if (typeStr == "TOP_N_OCCURRENCES") return StatisticType::TOP_N_OCCURRENCES;
    if (typeStr == "OCCURRENCE_COUNT") return StatisticType::OCCURRENCE_COUNT;
    if (typeStr == "SUM") return StatisticType::SUM;
    if (typeStr == "AVERAGE") return StatisticType::AVERAGE;
    if (typeStr == "MIN") return StatisticType::MIN;
    if (typeStr == "MAX") return StatisticType::MAX;
    return StatisticType::UNKNOWN;
}

// Helper to convert StatisticOutputFormat enum to string
std::string statisticOutputFormatToString(StatisticOutputFormat format) {
    switch (format) {
        case StatisticOutputFormat::PLAINTEXT_TABLE: return "PLAINTEXT_TABLE";
        case StatisticOutputFormat::JSON: return "JSON";
        case StatisticOutputFormat::CSV: return "CSV";
        default: return "UNKNOWN";
    }
}

// Helper to convert string to StatisticOutputFormat enum
StatisticOutputFormat stringToStatisticOutputFormat(const std::string& formatStr) {
    if (formatStr == "PLAINTEXT_TABLE") return StatisticOutputFormat::PLAINTEXT_TABLE;
    if (formatStr == "JSON") return StatisticOutputFormat::JSON;
    if (formatStr == "CSV") return StatisticOutputFormat::CSV;
    return StatisticOutputFormat::UNKNOWN;
}

} // namespace Utils
