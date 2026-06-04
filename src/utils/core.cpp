// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "utils/core.h" // Includes all necessary declarations for Utils namespace
#include "utils/string.h"
#include "export/core.h"
#include "filter/types.h" // Include FilterTypes.h for SortBy, SortOrder
#include "stats/core.h"
#include <algorithm>
#include <map>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <limits>
#include <string>
#include <optional>
#include <regex>

namespace Utils {

namespace {

void toUpperInPlaceAsciiSafe(std::string& value) {
    std::ranges::transform(value, value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
}

} // namespace

// LogLevel functions (now correctly declared in LogTypes.h within Utils namespace)
LogLevel stringToLogLevel(const std::string &levelStr) {
    std::string upperLevelStr = levelStr;
    toUpperInPlaceAsciiSafe(upperLevelStr);

    if (upperLevelStr == "TRACE") return LogLevel::TRACE;
    if (upperLevelStr == "DEBUG") return LogLevel::DEBUG;
    if (upperLevelStr == "INFO") return LogLevel::INFO;
    if (upperLevelStr == "WARN" || upperLevelStr == "WARNING") return LogLevel::WARNING;
    if (upperLevelStr == "ERROR") return LogLevel::ERROR;
    if (upperLevelStr == "CRITICAL") return LogLevel::CRITICAL;
    if (upperLevelStr == "FATAL") return LogLevel::FATAL;
    return LogLevel::UNKNOWN;
}

LogLevel stringToLogLevel(const std::string &levelStr, const std::map<std::string, LogLevel, LogAnalyzerInternal::CaseInsensitiveLess> &customMappings) {
    // CaseInsensitiveLess comparator handles case insensitivity directly for the map lookup.
    auto it = customMappings.find(levelStr);
    if (it != customMappings.end()) {
        return it->second;
    }
    // Fallback to default conversion (which is case-insensitive) if not found in custom mappings.
    return stringToLogLevel(levelStr);
}

std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        case LogLevel::FATAL: return "FATAL";
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN";
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

// Enum to string and string to enum conversions for various types
// LogEntryField
std::string logEntryFieldToString(LogEntryField field) {
    switch (field) {
        case LogEntryField::ID: return "ID";
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

LogEntryField stringToLogEntryField(const std::string& fieldStr) {
    std::string upperFieldStr = toUpper(fieldStr);

    if (upperFieldStr == "ID" || upperFieldStr == "PID") return LogEntryField::ID;
    if (upperFieldStr == "TIMESTAMP" || upperFieldStr == "TIME") return LogEntryField::TIMESTAMP;
    if (upperFieldStr == "LEVEL" || upperFieldStr == "LVL" || upperFieldStr == "SEVERITY") return LogEntryField::LEVEL;
    if (upperFieldStr == "MESSAGE" || upperFieldStr == "MSG" || upperFieldStr == "TEXT") return LogEntryField::MESSAGE;
    if (upperFieldStr == "SOURCE_FILE" || upperFieldStr == "SOURCE" || upperFieldStr == "SOURCEFILE" ||
        upperFieldStr == "FILE" || upperFieldStr == "FILENAME") return LogEntryField::SOURCE_FILE;
    if (upperFieldStr == "LINE_NUMBER" || upperFieldStr == "LINE" || upperFieldStr == "LINENUMBER" ||
        upperFieldStr == "LINE_NO" || upperFieldStr == "LINENO") return LogEntryField::LINE_NUMBER;
    if (upperFieldStr == "THREAD_ID" || upperFieldStr == "THREAD" || upperFieldStr == "THREADID" ||
        upperFieldStr == "TID" || upperFieldStr == "THREAD-ID") return LogEntryField::THREAD_ID;
    if (upperFieldStr == "MODULE") return LogEntryField::MODULE;
    if (upperFieldStr == "HOST") return LogEntryField::HOST;
    if (upperFieldStr == "STRUCTURED_FIELD") return LogEntryField::STRUCTURED_FIELD;
    // Removed explicit handling for "CUSTOM". If fieldStr is "CUSTOM", it will now fall through
    // to return LogEntryField::UNKNOWN, correctly triggering the custom field logic in from_json.
    return LogEntryField::UNKNOWN;
}
std::string exportFormatToString(ExportFormat format) {
    switch (format) {
        case ExportFormat::PLAINTEXT: return "PLAINTEXT";
        case ExportFormat::JSON: return "JSON";
        case ExportFormat::NDJSON: return "NDJSON";
        case ExportFormat::CSV: return "CSV";
        case ExportFormat::XML: return "XML";
        default: return "UNKNOWN";
    }
}

std::optional<ExportFormat> stringToExportFormat(const std::string& formatStr) {
    std::string upperFormatStr = formatStr;
    toUpperInPlaceAsciiSafe(upperFormatStr);

    if (upperFormatStr == "PLAINTEXT" || upperFormatStr == "TEXT") return ExportFormat::PLAINTEXT;
    if (upperFormatStr == "JSON") return ExportFormat::JSON;
    if (upperFormatStr == "NDJSON") return ExportFormat::NDJSON;
    if (upperFormatStr == "CSV") return ExportFormat::CSV;
    if (upperFormatStr == "XML") return ExportFormat::XML;
    return std::nullopt;
}

// StatisticType
std::string statisticTypeToString(StatisticType type) {
    switch (type) {
        case StatisticType::UNIQUE_MESSAGES: return "UNIQUE_MESSAGES";
        case StatisticType::TOP_MESSAGES: return "TOP_MESSAGES";
        case StatisticType::ENTRY_RATE: return "ENTRY_RATE";
        case StatisticType::LOG_LEVEL_COUNT: return "COUNT_BY_LEVEL";
        case StatisticType::FIELD_VALUE_COUNT: return "FIELD_VALUE_COUNT";
        case StatisticType::TOP_N_FIELD_VALUES: return "TOP_N_FIELD_VALUES";
        case StatisticType::TIME_BUCKET_HISTOGRAM: return "TIME_BUCKET_HISTOGRAM";
        case StatisticType::PERCENTILE_STATS: return "PERCENTILE_STATS";
        default: return "UNKNOWN";
    }
}

std::optional<StatisticType> stringToStatisticType(const std::string& typeStr) {
    std::string upperTypeStr = typeStr;
    toUpperInPlaceAsciiSafe(upperTypeStr);

    if (upperTypeStr == "UNIQUE_MESSAGES") return StatisticType::UNIQUE_MESSAGES;
    if (upperTypeStr == "TOP_MESSAGES") return StatisticType::TOP_MESSAGES;
    if (upperTypeStr == "ENTRY_RATE") return StatisticType::ENTRY_RATE;
    if (upperTypeStr == "COUNT_BY_LEVEL" || upperTypeStr == "LOG_LEVEL_COUNT") return StatisticType::LOG_LEVEL_COUNT;
    if (upperTypeStr == "FIELD_VALUE_COUNT") return StatisticType::FIELD_VALUE_COUNT;
    if (upperTypeStr == "TOP_N_FIELD_VALUES") return StatisticType::TOP_N_FIELD_VALUES;
    if (upperTypeStr == "TIME_BUCKET_HISTOGRAM") return StatisticType::TIME_BUCKET_HISTOGRAM;
    if (upperTypeStr == "PERCENTILE_STATS") return StatisticType::PERCENTILE_STATS;
    return std::nullopt;
}

// PatternType
std::string patternTypeToString(PatternType type) {
    switch (type) {
        case PatternType::Literal: return "Literal";
        case PatternType::Regex: return "Regex";
        case PatternType::Wildcard: return "Wildcard";
        default: return "Unknown";
    }
}

std::optional<PatternType> stringToPatternType(const std::string& typeStr) {
    std::string upperTypeStr = typeStr;
    toUpperInPlaceAsciiSafe(upperTypeStr);

    if (upperTypeStr == "LITERAL") return PatternType::Literal;
    if (upperTypeStr == "REGEX") return PatternType::Regex;
    if (upperTypeStr == "WILDCARD") return PatternType::Wildcard;
    return std::nullopt;
}

// ParseError
std::string parseErrorToString(ParseError error) {
    switch (error) {
        case ParseError::SUCCESS: return "Success";
        case ParseError::PARTIAL_FAILURE: return "Partial Failure";
        case ParseError::UNKNOWN_ERROR: return "Unknown Error";
        case ParseError::INVALID_REGEX_PATTERN: return "Invalid Regex Pattern";
        default: return "Unknown";
    }
}

std::optional<ParseError> stringToParseError(const std::string& errorStr) {
    std::string upperErrorStr = errorStr;
    toUpperInPlaceAsciiSafe(upperErrorStr);

    if (upperErrorStr == "SUCCESS") return ParseError::SUCCESS;
    if (upperErrorStr == "PARTIAL FAILURE" || upperErrorStr == "PARTIAL_FAILURE") return ParseError::PARTIAL_FAILURE;
    if (upperErrorStr == "UNKNOWN ERROR" || upperErrorStr == "UNKNOWN_ERROR") return ParseError::UNKNOWN_ERROR;
    if (upperErrorStr == "INVALID REGEX PATTERN" || upperErrorStr == "INVALID_REGEX_PATTERN") return ParseError::INVALID_REGEX_PATTERN;
    return std::nullopt;
}

// SortBy
std::string sortByToString(filter::SortBy sort) {
    switch (sort) {
        case filter::SortBy::TIMESTAMP: return "TIMESTAMP";
        case filter::SortBy::LEVEL: return "LEVEL";
        case filter::SortBy::MESSAGE: return "MESSAGE";
        case filter::SortBy::SOURCE: return "SOURCE";
        case filter::SortBy::THREAD_ID: return "THREAD_ID";
        default: return "UNKNOWN";
    }
}

std::optional<filter::SortBy> stringToSortBy(const std::string& sortStr) {
    std::string upperSortStr = sortStr;
    toUpperInPlaceAsciiSafe(upperSortStr);

    if (upperSortStr == "TIMESTAMP" || upperSortStr == "TIME") return filter::SortBy::TIMESTAMP;
    if (upperSortStr == "LEVEL") return filter::SortBy::LEVEL;
    if (upperSortStr == "MESSAGE" || upperSortStr == "MSG") return filter::SortBy::MESSAGE;
    if (upperSortStr == "SOURCE") return filter::SortBy::SOURCE;
    if (upperSortStr == "THREAD_ID" || upperSortStr == "THREAD") return filter::SortBy::THREAD_ID;
    return std::nullopt;
}

// SortOrder
std::string sortOrderToString(filter::SortOrder order) {
    switch (order) {
        case filter::SortOrder::ASCENDING: return "ASCENDING";
        case filter::SortOrder::DESCENDING: return "DESCENDING";
        default: return "UNKNOWN";
    }
}

std::optional<filter::SortOrder> stringToSortOrder(const std::string& orderStr) {
    std::string upperOrderStr = orderStr;
    toUpperInPlaceAsciiSafe(upperOrderStr);

    if (upperOrderStr == "ASCENDING" || upperOrderStr == "ASC") return filter::SortOrder::ASCENDING;
    if (upperOrderStr == "DESCENDING" || upperOrderStr == "DESC") return filter::SortOrder::DESCENDING;
    return std::nullopt;
}

std::expected<size_t, ErrorCode::Error> parseHumanReadableSize(std::string_view sizeStr) {
    if (sizeStr.empty()) {
        return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Empty size string"));
    }

    std::string s(sizeStr);
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Empty size string"));
    }
    const auto last = s.find_last_not_of(" \t\r\n");
    s = s.substr(first, last - first + 1);

    static const std::regex sizeRegex(R"(^([0-9]+(?:\.[0-9]+)?)(?:\s*([A-Za-z]+))?$)");
    std::smatch match;
    if (!std::regex_match(s, match, sizeRegex)) {
        return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Invalid size number format: " + s));
    }

    const std::string numberPart = match[1].str();
    std::string unit = match[2].matched ? match[2].str() : "";

    double val = 0.0;
    size_t multiplier = 1;

    try {
        size_t idx = 0;
        val = std::stod(numberPart, &idx);
        if (idx != numberPart.size()) {
            return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Invalid size number format: " + s));
        }

        toUpperInPlaceAsciiSafe(unit);
        if (unit.empty() || unit == "B" || unit == "BYTES") multiplier = 1;
        else if (unit == "K" || unit == "KB") multiplier = 1024;
        else if (unit == "M" || unit == "MB") multiplier = 1024 * 1024;
        else if (unit == "G" || unit == "GB") multiplier = 1024 * 1024 * 1024;
        else if (unit == "T" || unit == "TB") multiplier = 1024ULL * 1024 * 1024 * 1024;
        else return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Invalid size unit: " + unit));
    } catch (...) {
        return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Invalid size number format: " + s));
    }

    if (!std::isfinite(val) || val < 0.0) {
        return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Invalid size number format: " + s));
    }

    const long double scaled = static_cast<long double>(val) * static_cast<long double>(multiplier);
    if (!std::isfinite(scaled) ||
        scaled > static_cast<long double>(std::numeric_limits<size_t>::max())) {
        return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Size value out of range: " + s));
    }

    return static_cast<size_t>(scaled);
}

size_t generateLogEntryId(const std::string& sourceFile, size_t lineNumber, std::string_view line) {
    // Basic but effective ID generation based on file, line, and content
    // Combining these with a simple hash
    size_t h1 = std::hash<std::string>{}(sourceFile);
    size_t h2 = std::hash<size_t>{}(lineNumber);
    size_t h3 = std::hash<std::string_view>{}(line);
    
    // Combine hashes (simple XOR with shifts)
    return h1 ^ (h2 << 1) ^ (h3 << 2);
}

} // namespace Utils
