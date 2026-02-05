// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 eserlxl

#include "utils/Core.h" // Includes all necessary declarations for Utils namespace
#include "utils/String.h"
#include "export/Exporter.h"
#include "filter/Types.h" // Include FilterTypes.h for SortBy, SortOrder
#include "stats/Statistics.h"
#include <algorithm>
#include <map>
#include <filesystem>
#include <string>
#include <optional>

namespace Utils {

// LogLevel functions (now correctly declared in LogTypes.h within Utils namespace)
LogLevel stringToLogLevel(const std::string &levelStr) {
    std::string upperLevelStr = levelStr;
    std::transform(upperLevelStr.begin(), upperLevelStr.end(), upperLevelStr.begin(), ::toupper);

    if (upperLevelStr == "TRACE") return LogLevel::TRACE;
    if (upperLevelStr == "DEBUG") return LogLevel::DEBUG;
    if (upperLevelStr == "INFO") return LogLevel::INFO;
    if (upperLevelStr == "WARN" || upperLevelStr == "WARNING") return LogLevel::WARNING;
    if (upperLevelStr == "ERROR") return LogLevel::ERROR;
    if (upperLevelStr == "CRITICAL") return LogLevel::CRITICAL;
    if (upperLevelStr == "FATAL") return LogLevel::FATAL;
    return LogLevel::UNKNOWN;
}

LogLevel stringToLogLevel(const std::string &levelStr, const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> &customMappings) {
    // ci_less comparator handles case insensitivity directly for the map lookup.
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

    if (upperFieldStr == "ID") return LogEntryField::ID;
    if (upperFieldStr == "TIMESTAMP") return LogEntryField::TIMESTAMP;
    if (upperFieldStr == "LEVEL") return LogEntryField::LEVEL;
    if (upperFieldStr == "MESSAGE") return LogEntryField::MESSAGE;
    if (upperFieldStr == "SOURCE_FILE") return LogEntryField::SOURCE_FILE;
    if (upperFieldStr == "LINE_NUMBER") return LogEntryField::LINE_NUMBER;
    if (upperFieldStr == "THREAD_ID") return LogEntryField::THREAD_ID;
    if (upperFieldStr == "MODULE") return LogEntryField::MODULE;
    if (upperFieldStr == "HOST") return LogEntryField::HOST;
    if (upperFieldStr == "CUSTOM") return LogEntryField::CUSTOM; // Added handling for CUSTOM
    if (upperFieldStr == "STRUCTURED_FIELD") return LogEntryField::STRUCTURED_FIELD;
    return LogEntryField::UNKNOWN;
}
std::string exportFormatToString(ExportFormat format) {
    switch (format) {
        case ExportFormat::PLAINTEXT: return "PLAINTEXT";
        case ExportFormat::JSON: return "JSON";
        case ExportFormat::CSV: return "CSV";
        case ExportFormat::XML: return "XML";
        default: return "UNKNOWN";
    }
}

std::optional<ExportFormat> stringToExportFormat(const std::string& formatStr) {
    std::string upperFormatStr = formatStr;
    std::transform(upperFormatStr.begin(), upperFormatStr.end(), upperFormatStr.begin(), ::toupper);

    if (upperFormatStr == "PLAINTEXT" || upperFormatStr == "TEXT") return ExportFormat::PLAINTEXT;
    if (upperFormatStr == "JSON") return ExportFormat::JSON;
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
        default: return "UNKNOWN";
    }
}

std::optional<StatisticType> stringToStatisticType(const std::string& typeStr) {
    std::string upperTypeStr = typeStr;
    std::transform(upperTypeStr.begin(), upperTypeStr.end(), upperTypeStr.begin(), ::toupper);

    if (upperTypeStr == "UNIQUE_MESSAGES") return StatisticType::UNIQUE_MESSAGES;
    if (upperTypeStr == "TOP_MESSAGES") return StatisticType::TOP_MESSAGES;
    if (upperTypeStr == "ENTRY_RATE") return StatisticType::ENTRY_RATE;
    if (upperTypeStr == "COUNT_BY_LEVEL" || upperTypeStr == "LOG_LEVEL_COUNT") return StatisticType::LOG_LEVEL_COUNT;
    if (upperTypeStr == "FIELD_VALUE_COUNT") return StatisticType::FIELD_VALUE_COUNT;
    if (upperTypeStr == "TOP_N_FIELD_VALUES") return StatisticType::TOP_N_FIELD_VALUES;
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
    std::transform(upperTypeStr.begin(), upperTypeStr.end(), upperTypeStr.begin(), ::toupper);

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
    std::transform(upperErrorStr.begin(), upperErrorStr.end(), upperErrorStr.begin(), ::toupper);

    if (upperErrorStr == "SUCCESS") return ParseError::SUCCESS;
    if (upperErrorStr == "PARTIAL FAILURE" || upperErrorStr == "PARTIAL_FAILURE") return ParseError::PARTIAL_FAILURE;
    if (upperErrorStr == "UNKNOWN ERROR" || upperErrorStr == "UNKNOWN_ERROR") return ParseError::UNKNOWN_ERROR;
    if (upperErrorStr == "INVALID REGEX PATTERN" || upperErrorStr == "INVALID_REGEX_PATTERN") return ParseError::INVALID_REGEX_PATTERN;
    return std::nullopt;
}

// SortBy
std::string sortByToString(SortBy sort) {
    switch (sort) {
        case SortBy::TIMESTAMP: return "TIMESTAMP";
        case SortBy::LEVEL: return "LEVEL";
        case SortBy::MESSAGE: return "MESSAGE";
        case SortBy::SOURCE: return "SOURCE";
        case SortBy::THREAD_ID: return "THREAD_ID";
        default: return "UNKNOWN";
    }
}

std::optional<SortBy> stringToSortBy(const std::string& sortStr) {
    std::string upperSortStr = sortStr;
    std::transform(upperSortStr.begin(), upperSortStr.end(), upperSortStr.begin(), ::toupper);

    if (upperSortStr == "TIMESTAMP" || upperSortStr == "TIME") return SortBy::TIMESTAMP;
    if (upperSortStr == "LEVEL") return SortBy::LEVEL;
    if (upperSortStr == "MESSAGE" || upperSortStr == "MSG") return SortBy::MESSAGE;
    if (upperSortStr == "SOURCE") return SortBy::SOURCE;
    if (upperSortStr == "THREAD_ID" || upperSortStr == "THREAD") return SortBy::THREAD_ID;
    return std::nullopt;
}

// SortOrder
std::string sortOrderToString(SortOrder order) {
    switch (order) {
        case SortOrder::ASCENDING: return "ASCENDING";
        case SortOrder::DESCENDING: return "DESCENDING";
        default: return "UNKNOWN";
    }
}

std::optional<SortOrder> stringToSortOrder(const std::string& orderStr) {
    std::string upperOrderStr = orderStr;
    std::transform(upperOrderStr.begin(), upperOrderStr.end(), upperOrderStr.begin(), ::toupper);

    if (upperOrderStr == "ASCENDING" || upperOrderStr == "ASC") return SortOrder::ASCENDING;
    if (upperOrderStr == "DESCENDING" || upperOrderStr == "DESC") return SortOrder::DESCENDING;
    return std::nullopt;
}

std::expected<size_t, ErrorCode::Error> parseHumanReadableSize(std::string_view sizeStr) {
    if (sizeStr.empty()) {
        return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Empty size string"));
    }

    std::string s(sizeStr);
    // Remove whitespace
    s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());

    size_t unitPos = std::string::npos;
    for (size_t i = 0; i < s.length(); ++i) {
        if (!isdigit(s[i]) && s[i] != '.') {
            unitPos = i;
            break;
        }
    }

    double val = 0.0;
    size_t multiplier = 1;

    try {
        if (unitPos == std::string::npos) {
            val = std::stod(s);
        } else {
            val = std::stod(s.substr(0, unitPos));
            std::string unit = s.substr(unitPos);
            std::transform(unit.begin(), unit.end(), unit.begin(), ::toupper);

            if (unit == "B" || unit == "BYTES") multiplier = 1;
            else if (unit == "K" || unit == "KB") multiplier = 1024;
            else if (unit == "M" || unit == "MB") multiplier = 1024 * 1024;
            else if (unit == "G" || unit == "GB") multiplier = 1024 * 1024 * 1024;
            else if (unit == "T" || unit == "TB") multiplier = 1024ULL * 1024 * 1024 * 1024;
            else return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Invalid size unit: " + unit));
        }
    } catch (...) {
        return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Invalid size number format: " + s));
    }

    return static_cast<size_t>(val * multiplier);
}

} // namespace Utils
