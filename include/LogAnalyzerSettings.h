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
    // The regex pattern used to parse individual log lines.
    std::string lineParsePattern;
    // Mappings from regex capture groups to LogEntry fields.
    // Constructors initialize these with default mappings (timestamp, level, message).
    // For custom patterns, users are expected to provide explicit mappings or modify these defaults for correct parsing.
    // If fieldMappings is empty, LogAnalyzer may not be able to parse log entries correctly.
    std::vector<FieldMapping> fieldMappings;
    // Custom mappings for log level strings (e.g., "WARN" -> LogLevel::WARNING).
    std::map<std::string, LogLevel, ci_less> customLogLevelMappings;

    // Optional regex pattern to identify the start of a new log entry, enabling multi-line parsing.
    // If set, the parser will buffer lines until a new start pattern is encountered or EOF.
    std::optional<std::string> logEntryStartPattern;

    // Defines whether log parsing should be case-sensitive. Applies to regex patterns.
    bool caseSensitiveParsing = false; 

private: // Helper for consistency
    // Helper to initialize default field mappings.
    void initializeDefaultFieldMappings() {
        fieldMappings.emplace_back(LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S");
        fieldMappings.emplace_back(LogEntryField::LEVEL, 2);
        fieldMappings.emplace_back(LogEntryField::MESSAGE, 3);
    }

public:
    // Default constructor: Initializes with default pattern and corresponding default field mappings.
    LogAnalyzerSettings() 
        : lineParsePattern(DEFAULT_LOG_REGEX_PATTERN_INTERNAL) 
    {
        initializeDefaultFieldMappings(); 
    }
    
    // Constructor for custom patterns.
    // If 'pattern' matches DEFAULT_LOG_REGEX_PATTERN_INTERNAL, it will also initialize default field mappings.
    // Otherwise, fieldMappings will remain empty, expecting the user to provide custom mappings.
        explicit LogAnalyzerSettings(std::string pattern)
            : lineParsePattern(std::move(pattern))
        {
            // Always initialize default field mappings to provide a baseline.
            initializeDefaultFieldMappings();
        }};
#endif // LOG_ANALYZER_CONFIG_H
