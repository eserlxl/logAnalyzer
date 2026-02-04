#ifndef LOG_ANALYZER_CONFIG_H
#define LOG_ANALYZER_CONFIG_H

#include "LogTypes.h"
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <string_view>

// Define DEFAULT_LOG_REGEX_PATTERN directly in LogAnalyzerConfig.h or a common header
// to avoid circular dependency with LogAnalyzer.h
constexpr std::string_view DEFAULT_LOG_REGEX_PATTERN_INTERNAL = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) ([A-Z]+): (.*)$)";

struct LogAnalyzerSettings {
    // Default regex pattern for parsing log lines.
    // If empty, the LogAnalyzer will revert to its internal hardcoded default.
    std::string lineParsePattern;

    // Mappings from regex capture groups to LogEntry fields.
    // If empty and lineParsePattern is DEFAULT_LOG_REGEX_PATTERN,
    // default field mappings for timestamp, level, and message will be used.
    // For custom patterns, explicit field mappings are highly recommended.
    std::vector<FieldMapping> fieldMappings;

    // Custom mappings for log level strings (e.g., "WARN" -> LogLevel::WARNING).
    std::map<std::string, LogLevel, ci_less> customLogLevelMappings;

    // Optional regex pattern to identify the start of a new log entry, enabling multi-line parsing.
    // If set, the parser will buffer lines until a new start pattern is encountered or EOF.
    std::optional<std::string> logEntryStartPattern;

    // Defines whether log parsing should be case-sensitive. Applies to regex patterns.
    bool caseSensitiveParsing = false; 

    // Constructor to provide sane defaults for common use cases.
    LogAnalyzerSettings() {
        // Initialize with default pattern
        lineParsePattern = std::string(DEFAULT_LOG_REGEX_PATTERN_INTERNAL);
        // Default field mappings for the DEFAULT_LOG_REGEX_PATTERN
        fieldMappings.emplace_back(LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S");
        fieldMappings.emplace_back(LogEntryField::LEVEL, 2);
        fieldMappings.emplace_back(LogEntryField::MESSAGE, 3);
    }
    
    // Allow easy construction for custom patterns without explicit field mappings initially
    explicit LogAnalyzerSettings(std::string pattern) : lineParsePattern(std::move(pattern)) {}
};

#endif // LOG_ANALYZER_CONFIG_H
