// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "config/Core.h"
#include "config/Settings.h"
#include <algorithm>

void LogAnalyzerSettings::merge(const LogAnalyzerSettings& other) {
    // Overwrite simple scalar values if the source has them set to a non-default.
    if (!other.lineParsePattern.empty() && other.lineParsePattern != DEFAULT_LOG_REGEX_PATTERN_INTERNAL) {
        lineParsePattern = other.lineParsePattern;
    }
    
    // For optionals, overwrite if 'other' has a value.
    if (other.logEntryStartPattern.has_value()) {
        logEntryStartPattern = other.logEntryStartPattern;
    }
    
    if (other.caseSensitiveParsing.has_value()) {
        caseSensitiveParsing = other.caseSensitiveParsing;
    }

    if (other.maxMultilineBufferSize.has_value()) {
        maxMultilineBufferSize = other.maxMultilineBufferSize;
    }

    if (other.parserErrorAction.has_value()) {
        parserErrorAction = other.parserErrorAction;
    }

    // For fieldMappings, we implement smart merging (upsert).
    // Audit Requirement: Collection merging is additive.
    // Strategy: Append fields from 'other'. If a field (LogEntryField or custom name) already exists in 'this', update it.
    for (const auto& mapping : other.fieldMappings) {
        auto it = std::ranges::find_if(fieldMappings, [&](const FieldMapping& m) {
            // Check if same type and value
            if (m.field.index() != mapping.field.index()) return false;
            if (std::holds_alternative<LogEntryField>(m.field)) {
                return std::get<LogEntryField>(m.field) == std::get<LogEntryField>(mapping.field);
            }                 return std::get<std::string>(m.field) == std::get<std::string>(mapping.field);
           
        });

        if (it != fieldMappings.end()) {
            *it = mapping; // Overwrite existing
        } else {
            fieldMappings.push_back(mapping); // Add new
        }
    }

    if (!other.customLogLevelMappings.empty()) {
        for (const auto& [key, value] : other.customLogLevelMappings) {
            customLogLevelMappings[key] = value;
        }
    }
    
    if (!other.filterRules.empty()) {
        filterRules.insert(filterRules.end(), other.filterRules.begin(), other.filterRules.end());
    }
    
    if (!other.statisticConfigs.empty()) {
        statisticConfigs.insert(statisticConfigs.end(), other.statisticConfigs.begin(), other.statisticConfigs.end());
    }
    
    // Use ExportSettings::merge
    exportSettings.merge(other.exportSettings);

    if (other.rootFilterExpression.has_value()) {
        rootFilterExpression = other.rootFilterExpression;
    }
}


LogAnalyzerSettings LogAnalyzerSettings::createDefault() {
    // The constructor already initializes with default values.
    // Return a default-constructed object.
    return {};
}

