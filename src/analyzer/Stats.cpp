// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer/Core.h"
#include "stats/Statistics.h"
#include <iostream>
#include <memory>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

const int DEFAULT_TOP_N_STATISTIC_VALUE = 10;

using json = nlohmann::json;

void LogAnalyzer::addStatisticCollector(std::shared_ptr<IStatisticCollector> collector) {
    if (collector) {
        collectors_.push_back(collector);
    }
}

void LogAnalyzer::processEntryForStatistics(const LogEntry& entry) {
    for (const auto& collector : collectors_) {
        collector->collect(entry);
    }
}

std::map<std::string, json> LogAnalyzer::getAllStatisticReports() const {
    std::map<std::string, json> reports;
    for (const auto& collector : collectors_) {
        reports[collector->getName()] = collector->generateReport();
    }
    return reports;
}

// Factory method for creating statistic collectors
std::shared_ptr<IStatisticCollector> LogAnalyzer::createStatisticCollector(const StatisticConfig& config) {
    std::string targetField;
    std::string customFieldKey;
    int topN = DEFAULT_TOP_N_STATISTIC_VALUE; // Default value
    auto itTopN = config.params.find("top_n");
    if (itTopN != config.params.end()) {
        try {
            topN = std::stoi(itTopN->second);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Invalid 'top_n' parameter for statistic. Defaulting to " << DEFAULT_TOP_N_STATISTIC_VALUE << ". Error: " << e.what() << '\n';
            // topN remains DEFAULT_TOP_N_STATISTIC_VALUE from initialization
        }
    }

    switch (config.type) {
        case StatisticType::UNIQUE_MESSAGES:
            return std::make_shared<UniqueMessagesCollector>();
        case StatisticType::TOP_MESSAGES:
            return std::make_shared<TopMessagesCollector>(topN); // Reusing topN for backward compatibility
        case StatisticType::ENTRY_RATE:
            return std::make_shared<EntryRateCollector>();
        case StatisticType::LOG_LEVEL_COUNT:
            return std::make_shared<LogLevelCountCollector>();
        case StatisticType::FIELD_VALUE_COUNT: {
            // Extract common parameters first if not already done
            auto itTargetField = config.params.find("target_field");
            if (itTargetField != config.params.end()) {
                targetField = itTargetField->second;
            }
            auto itCustomFieldKey = config.params.find("custom_field_key");
            if (itCustomFieldKey != config.params.end()) {
                customFieldKey = itCustomFieldKey->second;
            }

            if (targetField.empty()) {
                throw std::runtime_error("FieldValueCountCollector requires 'target_field' parameter.");
            }
            if (targetField == "customFields" && customFieldKey.empty()) {
                throw std::runtime_error("FieldValueCountCollector with target_field 'customFields' requires 'custom_field_key' parameter.");
            }
            return std::make_shared<FieldValueCountCollector>(targetField, customFieldKey);
        }
        case StatisticType::TOP_N_FIELD_VALUES: {
            // Extract common parameters first if not already done
            auto itTargetField = config.params.find("target_field");
            if (itTargetField != config.params.end()) {
                targetField = itTargetField->second;
            }
            auto itCustomFieldKey = config.params.find("custom_field_key");
            if (itCustomFieldKey != config.params.end()) {
                customFieldKey = itCustomFieldKey->second;
            }

            if (targetField.empty()) {
                throw std::runtime_error("TopNFieldValuesCollector requires 'target_field' parameter.");
            }
            if (targetField == "customFields" && customFieldKey.empty()) {
                throw std::runtime_error("TopNFieldValuesCollector with target_field 'customFields' requires 'custom_field_key' parameter.");
            }
            return std::make_shared<TopNFieldValuesCollector>(topN, targetField, customFieldKey);
        }
        case StatisticType::UNKNOWN:
        default:
            std::cerr << "Warning: Attempted to create unknown statistic type." << '\n';
            return nullptr;
    }
}
