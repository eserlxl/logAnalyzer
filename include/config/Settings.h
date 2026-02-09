// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef LOG_ANALYZER_SETTINGS_H
#define LOG_ANALYZER_SETTINGS_H

#include "core/Log/Types.h"
#include "filter/Core.h"
#include "export/Core.h"
#include "stats/Core.h"
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <string_view>
#include <expected>
#include "core/CiLess.h" // Include for LogAnalyzer::ci_less
#include <filesystem>


// Define DEFAULT_LOG_REGEX_PATTERN directly in LogAnalyzerConfig.h or a common header
// to avoid circular dependency with LogAnalyzer.h
constexpr std::string_view DEFAULT_LOG_REGEX_PATTERN_INTERNAL = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) ([A-Z]+): (.*)$)";

namespace config_keys {
    constexpr std::string_view TOP_N = "top_n";
    constexpr std::string_view TARGET_FIELD = "target_field";
    constexpr std::string_view CUSTOM_FIELD_KEY = "custom_field_key";
} // namespace config_keys

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

    // The maximum buffer size in bytes for a single multi-line log entry.
    std::optional<size_t> maxMultilineBufferSize;

    // Defines whether log parsing should be case-sensitive. Applies to regex patterns.
    std::optional<bool> caseSensitiveParsing; 

    // Filter rules
    std::vector<filter::FilterRule> filterRules;
    
    // Advanced filtering expression (optional replacement for filterRules)
    std::optional<filter::FilterExpression> rootFilterExpression;

    // Export settings
    ExportSettings exportSettings;

    // Action on parse error
    std::optional<ParserErrorAction> parserErrorAction;

    // Statistics configuration
    std::vector<StatisticConfig> statisticConfigs;

    // Schema versioning
    std::string version = "1.0";

    // JSON serialization/deserialization methods
    static std::expected<LogAnalyzerSettings, std::vector<std::string>> fromJson(const std::string& jsonContent);
    static std::expected<LogAnalyzerSettings, std::vector<std::string>> fromFile(
        const std::filesystem::path& filePath, 
        bool expandEnv = true);
    std::string toJson() const;
    std::vector<std::string> validate() const;

    /**
     * Merges settings from 'other' into this object.
     * Scalar values (strings, bools, optionals) in 'other' will overwrite current values if set.
     * Collections (vectors, maps) will be merged (upserted or appended) with the existing collections.
     */
    void merge(const LogAnalyzerSettings& other);

    /**
     * Returns a LogAnalyzerSettings object initialized with the project's standard defaults.
     */
    static LogAnalyzerSettings createDefault();

    // Fluent API helpers for tests
    LogAnalyzerSettings& setLineParsePattern(std::string p) { lineParsePattern = std::move(p); return *this; }
    LogAnalyzerSettings& setCaseSensitiveParsing(bool b) { caseSensitiveParsing = b; return *this; }
    LogAnalyzerSettings& setLogEntryStartPattern(std::optional<std::string> p) {
        if (p && p->empty()) {
            logEntryStartPattern = std::nullopt;
        } else {
            logEntryStartPattern = std::move(p);
        }
        return *this;
    }
    LogAnalyzerSettings& addFieldMapping(LogEntryField f, int gi, const std::string& fmt = "") { 
        if (gi <= 0) return *this; // Validation: groupIndex must be positive

        // Overwrite if exists
        auto it = std::find_if(fieldMappings.begin(), fieldMappings.end(),
                               [&](const FieldMapping& m) {
                                   return std::holds_alternative<LogEntryField>(m.field) &&
                                          std::get<LogEntryField>(m.field) == f;
                               });
        if (it != fieldMappings.end()) {
            it->groupIndex = std::make_optional(static_cast<size_t>(gi));
            it->formats.clear();
            if (!fmt.empty()) {
                it->formats.push_back(fmt);
            }
        } else {
            fieldMappings.emplace_back(f, std::make_optional(static_cast<size_t>(gi)), fmt.empty() ? std::vector<std::string>{} : std::vector<std::string>{fmt});
        }
        return *this;
    }
    LogAnalyzerSettings& addFieldMapping(const std::string& customFieldName, int gi, const std::string& fmt = "") {
        if (customFieldName.empty()) return *this; // Validation: customFieldName cannot be empty
        if (gi <= 0) return *this; // Validation: groupIndex must be positive

        // Overwrite if exists
        auto it = std::find_if(fieldMappings.begin(), fieldMappings.end(),
                               [&](const FieldMapping& m) {
                                   return std::holds_alternative<std::string>(m.field) &&
                                          std::get<std::string>(m.field) == customFieldName;
                               });
        if (it != fieldMappings.end()) {
            it->groupIndex = std::make_optional(static_cast<size_t>(gi));
            it->formats.clear();
            if (!fmt.empty()) {
                it->formats.push_back(fmt);
            }
        } else {
            fieldMappings.emplace_back(customFieldName, std::make_optional(static_cast<size_t>(gi)), fmt.empty() ? std::vector<std::string>{} : std::vector<std::string>{fmt});
        }
        return *this;
    }
    LogAnalyzerSettings& clearFieldMappings() { fieldMappings.clear(); return *this; }
    LogAnalyzerSettings& addCustomLogLevelMapping(std::string s, LogLevel l) { 
        customLogLevelMappings[s] = l; return *this; 
    }
    LogAnalyzerSettings& clearCustomLogLevelMappings() { customLogLevelMappings.clear(); return *this; }
    LogAnalyzerSettings& addFilterRule(filter::FilterRule r) { filterRules.push_back(std::move(r)); return *this; }
    LogAnalyzerSettings& clearFilterRules() { filterRules.clear(); return *this; }
    LogAnalyzerSettings& setExportSettings(ExportSettings es) { exportSettings = std::move(es); return *this; }
    LogAnalyzerSettings& setExportPath(std::string p) { exportSettings.outputPath = std::move(p); return *this; }
    LogAnalyzerSettings& setExportFormat(ExportFormat f) { exportSettings.format = f; return *this; }
    LogAnalyzerSettings& setExportFieldsToExport(std::vector<ExportFieldMapping> fields) { exportSettings.fieldsToExport = std::move(fields); return *this; }
    LogAnalyzerSettings& addStatisticConfig(StatisticConfig sc) { statisticConfigs.push_back(std::move(sc)); return *this; }
    LogAnalyzerSettings& clearStatisticConfigs() { statisticConfigs.clear(); return *this; }

private: // Helper for consistency
    // Helper to initialize default field mappings.
    void initializeDefaultFieldMappings() {
        fieldMappings.emplace_back(LogEntryField::TIMESTAMP, std::make_optional<size_t>(1), std::vector<std::string>{"%Y-%m-%d %H:%M:%S"});
        fieldMappings.emplace_back(LogEntryField::LEVEL, std::make_optional<size_t>(2));
        fieldMappings.emplace_back(LogEntryField::MESSAGE, std::make_optional<size_t>(3));
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
        if (lineParsePattern == DEFAULT_LOG_REGEX_PATTERN_INTERNAL) {
            initializeDefaultFieldMappings();
        }
    }
};
#endif // LOG_ANALYZER_SETTINGS_H
