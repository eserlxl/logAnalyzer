#include "utils/Utils.h" // Includes all necessary declarations for Utils namespace
#include "export/Exporter.h"
#include "filter/FilterTypes.h" // Include FilterTypes.h for SortBy, SortOrder
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
    if (upperLevelStr == "WARNING") return LogLevel::WARNING;
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
    std::string upperFieldStr = fieldStr;
    std::transform(upperFieldStr.begin(), upperFieldStr.end(), upperFieldStr.begin(), ::toupper);

    if (upperFieldStr == "TIMESTAMP") return LogEntryField::TIMESTAMP;
    if (upperFieldStr == "LEVEL") return LogEntryField::LEVEL;
    if (upperFieldStr == "MESSAGE") return LogEntryField::MESSAGE;
    if (upperFieldStr == "SOURCE_FILE") return LogEntryField::SOURCE_FILE;
    if (upperFieldStr == "LINE_NUMBER") return LogEntryField::LINE_NUMBER;
    if (upperFieldStr == "THREAD_ID") return LogEntryField::THREAD_ID;
    if (upperFieldStr == "MODULE") return LogEntryField::MODULE;
    if (upperFieldStr == "HOST") return LogEntryField::HOST;
    if (upperFieldStr == "CUSTOM") return LogEntryField::CUSTOM;
    if (upperFieldStr == "STRUCTURED_FIELD") return LogEntryField::STRUCTURED_FIELD;
    return LogEntryField::UNKNOWN;
}

// FilterOperator
std::string filterOperatorToString(FilterOperator op) {
    switch (op) {
        case FilterOperator::EQUALS: return "EQUALS";
        case FilterOperator::NOT_EQUALS: return "NOT_EQUALS";
        case FilterOperator::CONTAINS: return "CONTAINS";
        case FilterOperator::NOT_CONTAINS: return "NOT_CONTAINS";
        case FilterOperator::STARTS_WITH: return "STARTS_WITH";
        case FilterOperator::ENDS_WITH: return "ENDS_WITH";
        case FilterOperator::REGEX_MATCH: return "REGEX_MATCH";
        case FilterOperator::LESS_THAN: return "LESS_THAN";
        case FilterOperator::GREATER_THAN: return "GREATER_THAN";
        case FilterOperator::LESS_THAN_OR_EQUAL: return "LESS_THAN_OR_EQUAL";
        case FilterOperator::GREATER_THAN_OR_EQUAL: return "GREATER_THAN_OR_EQUAL";
        default: return "UNKNOWN";
    }
}

std::optional<FilterOperator> stringToFilterOperator(const std::string& opStr) {
    std::string upperOpStr = opStr;
    std::transform(upperOpStr.begin(), upperOpStr.end(), upperOpStr.begin(), ::toupper);

    if (upperOpStr == "EQUALS") return FilterOperator::EQUALS;
    if (upperOpStr == "NOT_EQUALS") return FilterOperator::NOT_EQUALS;
    if (upperOpStr == "CONTAINS") return FilterOperator::CONTAINS;
    if (upperOpStr == "NOT_CONTAINS") return FilterOperator::NOT_CONTAINS;
    if (upperOpStr == "STARTS_WITH") return FilterOperator::STARTS_WITH;
    if (upperOpStr == "ENDS_WITH") return FilterOperator::ENDS_WITH;
    if (upperOpStr == "GREATER_THAN") return FilterOperator::GREATER_THAN;
    if (upperOpStr == "LESS_THAN") return FilterOperator::LESS_THAN;
    if (upperOpStr == "GREATER_THAN_OR_EQUAL") return FilterOperator::GREATER_THAN_OR_EQUAL;
    if (upperOpStr == "LESS_THAN_OR_EQUAL") return FilterOperator::LESS_THAN_OR_EQUAL;
    if (upperOpStr == "REGEX_MATCH") return FilterOperator::REGEX_MATCH;
    return std::nullopt;
}

// FilterLogicalOperator
std::string filterLogicalOperatorToString(FilterLogicalOperator op) {
    switch (op) {
        case FilterLogicalOperator::AND: return "AND";
        case FilterLogicalOperator::OR: return "OR";
        case FilterLogicalOperator::NOT: return "NOT";
        default: return "UNKNOWN";
    }
}

std::optional<FilterLogicalOperator> stringToFilterLogicalOperator(const std::string& opStr) {
    std::string upperOpStr = opStr;
    std::transform(upperOpStr.begin(), upperOpStr.end(), upperOpStr.begin(), ::toupper);

    if (upperOpStr == "AND") return FilterLogicalOperator::AND;
    if (upperOpStr == "OR") return FilterLogicalOperator::OR;
    if (upperOpStr == "NOT") return FilterLogicalOperator::NOT;
    return std::nullopt;
}

// FilterValueType
std::string filterValueTypeToString(FilterValueType type) {
    switch (type) {
        case FilterValueType::STRING: return "STRING";
        case FilterValueType::NUMERIC: return "NUMERIC";
        case FilterValueType::DATETIME: return "DATETIME";
        default: return "UNKNOWN";
    }
}

std::optional<FilterValueType> stringToFilterValueType(const std::string& typeStr) {
    std::string upperTypeStr = typeStr;
    std::transform(upperTypeStr.begin(), upperTypeStr.end(), upperTypeStr.begin(), ::toupper);

    if (upperTypeStr == "STRING") return FilterValueType::STRING;
    if (upperTypeStr == "NUMERIC") return FilterValueType::NUMERIC;
    if (upperTypeStr == "DATETIME") return FilterValueType::DATETIME;
    return std::nullopt;
}

// ExportFormat
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
        default: return "UNKNOWN";
    }
}

std::optional<SortBy> stringToSortBy(const std::string& sortStr) {
    std::string upperSortStr = sortStr;
    std::transform(upperSortStr.begin(), upperSortStr.end(), upperSortStr.begin(), ::toupper);

    if (upperSortStr == "TIMESTAMP" || upperSortStr == "TIME") return SortBy::TIMESTAMP;
    if (upperSortStr == "LEVEL") return SortBy::LEVEL;
    if (upperSortStr == "MESSAGE" || upperSortStr == "MSG") return SortBy::MESSAGE;
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

} // namespace Utils
