// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer/core.h"
#include "stats/Core.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

const int DEFAULT_TOP_N_STATISTIC_VALUE = 10;

using json = nlohmann::json;

namespace {

bool tryParseStrictPositiveInt(std::string_view value, int& parsedValue) {
    if (value.empty()) {
        return false;
    }
    int parsed = 0;
    const char* begin = value.data();
    const char* end = begin + value.size();
    auto [ptr, ec] = std::from_chars(begin, end, parsed);
    if (ec != std::errc{} || ptr != end || parsed <= 0) {
        return false;
    }
    parsedValue = parsed;
    return true;
}

std::optional<std::string> normalizeTargetFieldName(std::string_view rawField) {
    if (rawField.empty()) {
        return std::nullopt;
    }
    std::string field(rawField);
    const auto first = field.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return std::nullopt;
    }
    const auto last = field.find_last_not_of(" \t");
    field = field.substr(first, last - first + 1);
    std::transform(field.begin(), field.end(), field.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (field == "level") return "level";
    if (field == "message") return "message";
    if (field == "source" || field == "source_file" || field == "sourcefile") return "sourceFile";
    if (field == "timestamp" || field == "time") return "timestamp";
    if (field == "line" || field == "line_number" || field == "linenumber") return "lineNumber";
    if (field == "thread" || field == "thread_id" || field == "threadid" || field == "tid") return "threadId";
    if (field == "module") return "module";
    if (field == "host") return "host";
    if (field == "custom" || field == "custom_fields" || field == "customfields") return "customFields";
    return std::nullopt;
}

void trimInPlace(std::string& value) {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string::npos) {
        value.clear();
        return;
    }
    const auto last = value.find_last_not_of(" \t");
    value = value.substr(first, last - first + 1);
}

} // namespace

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

void LogAnalyzer::removeStatisticCollector(const std::shared_ptr<IStatisticCollector>& collector) {
    if (!collector) {
        return;
    }
    collectors_.erase(
        std::remove(collectors_.begin(), collectors_.end(), collector),
        collectors_.end());
}

void LogAnalyzer::clearStatisticCollectors() {
    collectors_.clear();
}

void LogAnalyzer::resetStatisticCollectors() {
    for (const auto& collector : collectors_) {
        if (collector) {
            collector->reset();
        }
    }
}

void LogAnalyzer::processEntriesForStatistics(std::span<const LogEntry> entries) {
    for (const auto& entry : entries) {
        processEntryForStatistics(entry);
    }
}

void LogAnalyzer::processEntriesForStatistics(std::generator<const LogEntry&> entries) {
    for (const auto& entry : entries) {
        processEntryForStatistics(entry);
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
        int parsedTopN = 0;
        if (tryParseStrictPositiveInt(itTopN->second, parsedTopN)) {
            topN = parsedTopN;
        } else {
            std::cerr << "Warning: Invalid 'top_n' parameter for statistic. Defaulting to " << DEFAULT_TOP_N_STATISTIC_VALUE << ".\n";
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
                auto normalized = normalizeTargetFieldName(itTargetField->second);
                if (normalized) {
                    targetField = *normalized;
                }
            }
            auto itCustomFieldKey = config.params.find("custom_field_key");
            if (itCustomFieldKey != config.params.end()) {
                customFieldKey = itCustomFieldKey->second;
                trimInPlace(customFieldKey);
            }

            if (targetField.empty()) {
                std::cerr << "Warning: FieldValueCountCollector requires 'target_field' parameter. Collector disabled." << '\n';
                return nullptr;
            }
            if (targetField == "customFields" && customFieldKey.empty()) {
                std::cerr << "Warning: FieldValueCountCollector with target_field 'customFields' requires 'custom_field_key' parameter. Collector disabled." << '\n';
                return nullptr;
            }
            return std::make_shared<FieldValueCountCollector>(targetField, customFieldKey);
        }
        case StatisticType::TOP_N_FIELD_VALUES: {
            // Extract common parameters first if not already done
            auto itTargetField = config.params.find("target_field");
            if (itTargetField != config.params.end()) {
                auto normalized = normalizeTargetFieldName(itTargetField->second);
                if (normalized) {
                    targetField = *normalized;
                }
            }
            auto itCustomFieldKey = config.params.find("custom_field_key");
            if (itCustomFieldKey != config.params.end()) {
                customFieldKey = itCustomFieldKey->second;
                trimInPlace(customFieldKey);
            }

            if (targetField.empty()) {
                std::cerr << "Warning: TopNFieldValuesCollector requires 'target_field' parameter. Collector disabled." << '\n';
                return nullptr;
            }
            if (targetField == "customFields" && customFieldKey.empty()) {
                std::cerr << "Warning: TopNFieldValuesCollector with target_field 'customFields' requires 'custom_field_key' parameter. Collector disabled." << '\n';
                return nullptr;
            }
            return std::make_shared<TopNFieldValuesCollector>(topN, targetField, customFieldKey);
        }
        case StatisticType::UNKNOWN:
        default:
            std::cerr << "Warning: Attempted to create unknown statistic type." << '\n';
            return nullptr;
    }
}
