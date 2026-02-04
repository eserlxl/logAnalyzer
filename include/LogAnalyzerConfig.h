#ifndef LOG_ANALYZER_CONFIG_H
#define LOG_ANALYZER_CONFIG_H

#include "LogTypes.h" // Assuming LogTypes.h defines LogLevel, FieldMapping, and ci_less
#include "Filter.h"   // For FilterRule
#include "Exporter.h" // For ExportSettings
#include "Statistics.h" // For StatisticConfig

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <string_view>
#include <functional> // Required for std::function
#include <utility>    // Required for std::move

// Defined as in LogAnalyzer.h comment
static constexpr std::string_view DEFAULT_LOG_REGEX_PATTERN_SV = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) ([A-Z]+): (.*)$)";
static const std::string DEFAULT_LOG_REGEX_PATTERN = std::string(DEFAULT_LOG_REGEX_PATTERN_SV);


struct LogAnalyzerSettings {
    // Parsing Configuration
    std::string lineParsePattern = DEFAULT_LOG_REGEX_PATTERN;
    std::vector<FieldMapping> fieldMappings;
    std::map<std::string, LogLevel, ci_less> customLogLevelMappings;
    std::optional<std::string> logEntryStartPattern;
    bool caseSensitiveParsing = false; 

    // Filtering Configuration (New)
    std::vector<FilterRule> filterRules;

    // Export Configuration (New)
    ExportSettings exportSettings;

    // Statistics Configuration (New)
    std::vector<StatisticConfig> statisticConfigs;

    // Constructor to provide sane defaults for common use cases.
    LogAnalyzerSettings() {
        fieldMappings.emplace_back(LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S"); // Corrected format string
        fieldMappings.emplace_back(LogEntryField::LEVEL, 2);
        fieldMappings.emplace_back(LogEntryField::MESSAGE, 3);
    }
    
    // Constructor for custom patterns. It clears field mappings, expecting the user to provide them.
    explicit LogAnalyzerSettings(std::string pattern) : 
        lineParsePattern(std::move(pattern)), 
        fieldMappings(), // Clear default mappings for custom pattern
        customLogLevelMappings(),
        logEntryStartPattern(std::nullopt),
        caseSensitiveParsing(false),
        exportSettings() // Default initialize new members
    {}

    // New fluent API methods for configuration (return *this for chaining)

    // Parsing Configuration
    LogAnalyzerSettings& setLineParsePattern(std::string pattern) {
        lineParsePattern = std::move(pattern);
        return *this;
    }
    LogAnalyzerSettings& addFieldMapping(LogEntryField field, int captureGroupIndex, std::string format = "") {
        fieldMappings.emplace_back(field, captureGroupIndex, std::move(format));
        return *this;
    }
    LogAnalyzerSettings& clearFieldMappings() {
        fieldMappings.clear();
        return *this;
    }
    LogAnalyzerSettings& addCustomLogLevelMapping(std::string levelString, LogLevel level) {
        customLogLevelMappings[std::move(levelString)] = level;
        return *this;
    }
    LogAnalyzerSettings& clearCustomLogLevelMappings() {
        customLogLevelMappings.clear();
        return *this;
    }
    LogAnalyzerSettings& setLogEntryStartPattern(std::optional<std::string> pattern) {
        logEntryStartPattern = std::move(pattern);
        return *this;
    }
    LogAnalyzerSettings& setCaseSensitiveParsing(bool enable) {
        caseSensitiveParsing = enable;
        return *this;
    }

    // Filtering Configuration
    LogAnalyzerSettings& addFilterRule(const FilterRule& rule) {
        filterRules.push_back(rule);
        return *this;
    }
    LogAnalyzerSettings& clearFilterRules() {
        filterRules.clear();
        return *this;
    }

    // Export Configuration
    LogAnalyzerSettings& setExportSettings(const ExportSettings& settings) {
        exportSettings = settings;
        return *this;
    }

    // Statistics Configuration
    LogAnalyzerSettings& addStatisticConfig(const StatisticConfig& config) {
        statisticConfigs.push_back(config);
        return *this;
    }
    LogAnalyzerSettings& clearStatisticConfigs() {
        statisticConfigs.clear();
        return *this;
    }
};

#endif // LOG_ANALYZER_CONFIG_H
