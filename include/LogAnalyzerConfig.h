#ifndef LOG_ANALYZER_CONFIG_H
#define LOG_ANALYZER_CONFIG_H

#include "LogTypes.h" // Assuming LogTypes.h defines LogLevel, FieldMapping, and ci_less
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <string_view>
#include <functional> // Required for std::function

// Forward declare LogAnalyzer to resolve its default pattern if needed
// Note: For a clean separation, it's better if LogAnalyzerConfig.h doesn't
// depend directly on LogAnalyzer.cpp or its specific constants beyond what's
// necessary for a default. The design document implies LogAnalyzer::DEFAULT_LOG_REGEX_PATTERN
// is a constant that can be directly used or duplicated here if it's truly a project-wide default.
// For this implementation, we will embed the pattern directly for now to ensure
// LogAnalyzerConfig.h is self-contained.
// If LogAnalyzer::DEFAULT_LOG_REGEX_PATTERN is truly a member constant, it would be better
// to include LogAnalyzer.h here, but that can lead to circular dependencies.
// A better approach is to define project-wide constants in a separate utility header.

// Defined as in LogAnalyzer.h comment
static constexpr std::string_view DEFAULT_LOG_REGEX_PATTERN_SV = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) ([A-Z]+): (.*)$)";
static const std::string DEFAULT_LOG_REGEX_PATTERN = std::string(DEFAULT_LOG_REGEX_PATTERN_SV);


struct LogAnalyzerSettings {
    // Default regex pattern for parsing log lines.
    // If empty, the LogAnalyzer will revert to its internal hardcoded default.
    // Using a string for flexibility, but could be string_view if lifetime is managed.
    std::string lineParsePattern = DEFAULT_LOG_REGEX_PATTERN;

    // Mappings from regex capture groups to LogEntry fields.
    // If empty and lineParsePattern is DEFAULT_LOG_REGEX_PATTERN,
    // default field mappings for timestamp, level, and message will be used.
    // For custom patterns, explicit field mappings are highly recommended.
    std::vector<FieldMapping> fieldMappings;

    // Custom mappings for log level strings (e.g., "WARN" -> LogLevel::WARNING).
    // Using ci_less for case-insensitive comparison of level names.
    std::map<std::string, LogLevel, ci_less> customLogLevelMappings;

    // Optional regex pattern to identify the start of a new log entry, enabling multi-line parsing.
    // If set, the parser will buffer lines until a new start pattern is encountered or EOF.
    std::optional<std::string> logEntryStartPattern;

    // Defines whether log parsing should be case-sensitive. Applies to regex patterns.
    bool caseSensitiveParsing = false; 

    // Constructor to provide sane defaults for common use cases.
    // Initializes with the default pattern and corresponding field mappings.
    LogAnalyzerSettings() {
        // Default field mappings for the DEFAULT_LOG_REGEX_PATTERN
        // Assuming FieldMapping constructor takes field, capture_group_index, and optional format
        fieldMappings.emplace_back(LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S");
        fieldMappings.emplace_back(LogEntryField::LEVEL, 2);
        fieldMappings.emplace_back(LogEntryField::MESSAGE, 3);
    }
    
    // Constructor for custom patterns. It clears field mappings, expecting the user to provide them.
    // If the user wants default mappings with a custom pattern, they must specify them separately.
    explicit LogAnalyzerSettings(std::string pattern) : 
        lineParsePattern(std::move(pattern)), 
        fieldMappings(), // Clear default mappings for custom pattern
        customLogLevelMappings(),
        logEntryStartPattern(std::nullopt),
        caseSensitiveParsing(false) 
    {}
};

#endif // LOG_ANALYZER_CONFIG_H
