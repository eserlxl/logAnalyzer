#include "../include/Utils.h"
#include "LogTypes.h"
#include "Filter.h"
#include "Exporter.h"
#include "Statistics.h"
#include <algorithm>
#include <map>
#include <filesystem>
#include <string>

namespace Utils {

LogLevel stringToLogLevel(const std::string &levelStr) {
    if (levelStr == "DEBUG") return LogLevel::DEBUG;
    if (levelStr == "INFO") return LogLevel::INFO;
    if (levelStr == "WARNING") return LogLevel::WARNING;
    if (levelStr == "ERROR") return LogLevel::ERROR;
    if (levelStr == "FATAL") return LogLevel::FATAL;
    if (levelStr == "TRACE") return LogLevel::TRACE;
    if (levelStr == "CRITICAL") return LogLevel::CRITICAL; // Added CRITICAL
    return LogLevel::UNKNOWN;
}

LogLevel stringToLogLevel(const std::string &levelStr, const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> &customMappings) {
    // ci_less comparator handles case insensitivity directly for the map lookup.
    auto it = customMappings.find(levelStr);
    if (it != customMappings.end()) {
        return it->second;
    }
    // Fallback to default conversion (which is case-insensitive) if not found in custom mappings.
    return stringToLogLevelIgnoreCase(levelStr);
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
        case StatisticType::UNIQUE_MESSAGES: return "UNIQUE_MESSAGES";
        case StatisticType::TOP_MESSAGES: return "TOP_MESSAGES";
        case StatisticType::ENTRY_RATE: return "ENTRY_RATE";
        default: return "UNKNOWN";
    }
}

// Helper to convert string to StatisticType enum
StatisticType stringToStatisticType(const std::string& typeStr) {
    if (typeStr == "UNIQUE_MESSAGES") return StatisticType::UNIQUE_MESSAGES;
    if (typeStr == "TOP_MESSAGES") return StatisticType::TOP_MESSAGES;
    if (typeStr == "ENTRY_RATE") return StatisticType::ENTRY_RATE;
    return StatisticType::UNKNOWN;
}

} // namespace Utils
