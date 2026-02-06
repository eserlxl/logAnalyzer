// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "config/Core.h"
#include "config/Settings.h"
#include "utils/Core.h"
#include <algorithm>

void LogAnalyzerSettings::merge(const LogAnalyzerSettings& other) {
    // Overwrite simple scalar values if the source has them set to a non-default.
    // This logic can be fine-tuned based on what "default" means for each property.
    if (!other.lineParsePattern.empty() && other.lineParsePattern != LogAnalyzerSettings::createDefault().lineParsePattern) {
        lineParsePattern = other.lineParsePattern;
    }
    
    // For optionals, overwrite if 'other' has a value.
    if (other.logEntryStartPattern.has_value()) {
        logEntryStartPattern = other.logEntryStartPattern;
    }
    
    // Simple boolean overwrite
    caseSensitiveParsing = other.caseSensitiveParsing;

    // For collections, replace if the 'other' collection is not empty.
    if (!other.fieldMappings.empty()) {
        fieldMappings = other.fieldMappings;
    }
    if (!other.customLogLevelMappings.empty()) {
        customLogLevelMappings = other.customLogLevelMappings;
    }
    if (!other.filterRules.empty()) {
        filterRules = other.filterRules;
    }
    if (!other.statisticConfigs.empty()) {
        statisticConfigs = other.statisticConfigs;
    }
    
    // For complex objects, you might need a deeper merge logic, but for now, we replace.
    // This simple replacement assumes `other` provides a complete replacement.
    exportSettings = other.exportSettings; // Assumes ExportSettings has its own sane copy/move semantics

    if (other.rootFilterExpression.has_value()) {
        rootFilterExpression = other.rootFilterExpression;
    }
}


LogAnalyzerSettings LogAnalyzerSettings::createDefault() {
    LogAnalyzerSettings defaults;
    defaults.lineParsePattern = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) ([A-Z]+): (.*)$)";
    defaults.fieldMappings.clear();
    defaults.fieldMappings.emplace_back(LogEntryField::TIMESTAMP, std::make_optional<size_t>(1), std::vector<std::string>{"%Y-%m-%d %H:%M:%S"});
    defaults.fieldMappings.emplace_back(LogEntryField::LEVEL, std::make_optional<size_t>(2));
    defaults.fieldMappings.emplace_back(LogEntryField::MESSAGE, std::make_optional<size_t>(3));
    defaults.caseSensitiveParsing = false;
    
    // Default export settings
    defaults.exportSettings.fieldsToExport = {
        LogEntryField::TIMESTAMP,
        LogEntryField::LEVEL,
        LogEntryField::MESSAGE
    };
    
    return defaults;
}
