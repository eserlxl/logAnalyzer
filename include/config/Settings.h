#ifndef LOG_ANALYZER_SETTINGS_H
#define LOG_ANALYZER_SETTINGS_H

#include "core/LogTypes.h"
#include "filter/Filter.h"
#include "export/Exporter.h"
#include "stats/Statistics.h"
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <string_view>
#include <expected>
#include "core/CiLess.h" // Include for LogAnalyzer::ci_less


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
    std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> customLogLevelMappings;

    // Optional regex pattern to identify the start of a new log entry, enabling multi-line parsing.
    // If set, the parser will buffer lines until a new start pattern is encountered or EOF.
    std::optional<std::string> logEntryStartPattern;

    // Defines whether log parsing should be case-sensitive. Applies to regex patterns.
    bool caseSensitiveParsing = false; 

    // Filter rules
    std::vector<FilterRule> filterRules;
    
    // Advanced filtering expression (optional replacement for filterRules)
    std::optional<FilterExpression> rootFilterExpression;

    // Export settings
    ExportSettings exportSettings;

    // Statistics configuration
    std::vector<StatisticConfig> statisticConfigs;

    // JSON serialization/deserialization methods
    static std::expected<LogAnalyzerSettings, std::vector<std::string>> fromJson(const std::string& jsonContent);
    static std::expected<LogAnalyzerSettings, std::vector<std::string>> fromFile(const std::string& filePath);
    std::string toJson() const;
    std::vector<std::string> validate() const;

    // Fluent API helpers for tests
    LogAnalyzerSettings& setLineParsePattern(std::string p) { lineParsePattern = std::move(p); return *this; }
    LogAnalyzerSettings& setCaseSensitiveParsing(bool b) { caseSensitiveParsing = b; return *this; }
    LogAnalyzerSettings& setLogEntryStartPattern(std::optional<std::string> p) { logEntryStartPattern = std::move(p); return *this; }
    LogAnalyzerSettings& addFieldMapping(LogEntryField f, int gi, const std::string& fmt = "") { 
        fieldMappings.emplace_back(f, gi, fmt); return *this; 
    }
    LogAnalyzerSettings& clearFieldMappings() { fieldMappings.clear(); return *this; }
    LogAnalyzerSettings& addCustomLogLevelMapping(std::string s, LogLevel l) { 
        customLogLevelMappings[s] = l; return *this; 
    }
    LogAnalyzerSettings& clearCustomLogLevelMappings() { customLogLevelMappings.clear(); return *this; }
    LogAnalyzerSettings& addFilterRule(FilterRule r) { filterRules.push_back(std::move(r)); return *this; }
    LogAnalyzerSettings& clearFilterRules() { filterRules.clear(); return *this; }
    LogAnalyzerSettings& setExportSettings(ExportSettings es) { exportSettings = std::move(es); return *this; }
    LogAnalyzerSettings& addStatisticConfig(StatisticConfig sc) { statisticConfigs.push_back(std::move(sc)); return *this; }
    LogAnalyzerSettings& clearStatisticConfigs() { statisticConfigs.clear(); return *this; }

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
    }
};
#endif // LOG_ANALYZER_SETTINGS_H
