// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "stats/core.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <vector>
#include <optional>

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

// UniqueMessagesCollector implementation
void UniqueMessagesCollector::collect(const LogEntry& entry) {
    _counts[entry.message]++;
}

json UniqueMessagesCollector::generateReport() const {
    json report;
    report["name"] = getName();
    report["total_unique_messages"] = _counts.size();
    report["counts"] = _counts;
    return report;
}

// TopMessagesCollector implementation
TopMessagesCollector::TopMessagesCollector(int topN) : _topN(topN) {}

void TopMessagesCollector::collect(const LogEntry& entry) {
    _counts[entry.message]++;
}

json TopMessagesCollector::generateReport() const {
    std::vector<std::pair<std::string, int>> sortedCounts(_counts.begin(), _counts.end());
    std::sort(sortedCounts.begin(), sortedCounts.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    if (sortedCounts.size() > static_cast<size_t>(_topN)) {
        sortedCounts.resize(_topN);
    }
    
    json report;
    report["name"] = getName();
    report["top_n"] = _topN;
    report["messages"] = json::array();
    for(const auto& p : sortedCounts) {
        report["messages"].push_back({
            {"message", p.first},
            {"count", p.second}
        });
    }
    return report;
}

// EntryRateCollector implementation
void EntryRateCollector::collect(const LogEntry& entry) {
    if (entry.timestamp) {
        _timestamps.push_back(*entry.timestamp);
    }
}

json EntryRateCollector::generateReport() const {
    json report;
    report["name"] = getName();
    if (_timestamps.size() < 2) {
        report["average_rate_per_sec"] = 0;
        report["total_entries"] = _timestamps.size();
        report["duration_sec"] = "0";
        return report;
    }

    std::sort(_timestamps.begin(), _timestamps.end());
    auto duration = _timestamps.back() - _timestamps.front();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

    double rate = (secs > 0) ? static_cast<double>(_timestamps.size()) / secs : static_cast<double>(_timestamps.size());
    
    report["average_rate_per_sec"] = rate;
    report["total_entries"] = _timestamps.size();
    return report;
}

// LogLevelCountCollector implementation
void LogLevelCountCollector::collect(const LogEntry& entry) {
    _counts[entry.level]++;
}

json LogLevelCountCollector::generateReport() const {
    json report;
    report["name"] = getName();
    int totalEntries = 0;
    json levelCountsJson = json::object();
    for (const auto& pair : _counts) {
        // Assuming logLevelToString is available globally or via included headers
        levelCountsJson[Utils::logLevelToString(pair.first)] = pair.second;
        totalEntries += pair.second;
    }
    report["total_entries"] = totalEntries;
    report["counts"] = levelCountsJson;
    return report;
}

// FieldValueCountCollector implementation
FieldValueCountCollector::FieldValueCountCollector(const std::string& targetFieldName)
    : _targetFieldName(normalizeTargetFieldName(targetFieldName).value_or(targetFieldName)) {}

FieldValueCountCollector::FieldValueCountCollector(const std::string& targetFieldName, const std::string& customFieldKey)
    : _targetFieldName(normalizeTargetFieldName(targetFieldName).value_or(targetFieldName)), _customFieldKey(customFieldKey) {}

std::string FieldValueCountCollector::getFieldValueAsString(const LogEntry& entry) const {
    if (_targetFieldName == "level") {
        // Assuming logLevelToString is available globally or via included headers
        return Utils::logLevelToString(entry.level);
    } else if (_targetFieldName == "message") {
        return entry.message;
    } else if (_targetFieldName == "sourceFile") {
        return entry.sourceFile;
    } else if (_targetFieldName == "timestamp" && entry.timestamp) {
        return Utils::formatTimestamp(*entry.timestamp);
    } else if (_targetFieldName == "lineNumber" && entry.sourceLineNumber) {
        return std::to_string(*entry.sourceLineNumber);
    } else if (_targetFieldName == "threadId" && entry.threadId) {
        return *entry.threadId;
    } else if (_targetFieldName == "module" && entry.module) {
        return *entry.module;
    } else if (_targetFieldName == "host" && entry.host) {
        return *entry.host;
    } else if (_targetFieldName == "customFields" && !_customFieldKey.empty()) {
        auto it = entry.customFields.find(_customFieldKey);
        if (it != entry.customFields.end()) {
            return it->second;
        }
    }
    return ""; // Default for unknown field or missing custom field
}

void FieldValueCountCollector::collect(const LogEntry& entry) {
    std::string value = getFieldValueAsString(entry);
    if (!value.empty()) { // Only count if a valid value was extracted
        _counts[value]++;
    }
}

json FieldValueCountCollector::generateReport() const {
    json report;
    report["name"] = getName();
    report["target_field"] = _targetFieldName;
    if (!_customFieldKey.empty()) {
        report["custom_field_key"] = _customFieldKey;
    }
    report["total_unique_values"] = _counts.size();
    report["counts"] = _counts;
    return report;
}

// TopNFieldValuesCollector implementation
TopNFieldValuesCollector::TopNFieldValuesCollector(int topN, const std::string& targetFieldName)
    : _topN(topN), _targetFieldName(normalizeTargetFieldName(targetFieldName).value_or(targetFieldName)) {}

TopNFieldValuesCollector::TopNFieldValuesCollector(int topN, const std::string& targetFieldName, const std::string& customFieldKey)
    : _topN(topN), _targetFieldName(normalizeTargetFieldName(targetFieldName).value_or(targetFieldName)), _customFieldKey(customFieldKey) {}

std::string TopNFieldValuesCollector::getFieldValueAsString(const LogEntry& entry) const {
    if (_targetFieldName == "level") {
        // Assuming logLevelToString is available globally or via included headers
        return Utils::logLevelToString(entry.level);
    } else if (_targetFieldName == "message") {
        return entry.message;
    } else if (_targetFieldName == "sourceFile") {
        return entry.sourceFile;
    } else if (_targetFieldName == "timestamp" && entry.timestamp) {
        return Utils::formatTimestamp(*entry.timestamp);
    } else if (_targetFieldName == "lineNumber" && entry.sourceLineNumber) {
        return std::to_string(*entry.sourceLineNumber);
    } else if (_targetFieldName == "threadId" && entry.threadId) {
        return *entry.threadId;
    } else if (_targetFieldName == "module" && entry.module) {
        return *entry.module;
    } else if (_targetFieldName == "host" && entry.host) {
        return *entry.host;
    } else if (_targetFieldName == "customFields" && !_customFieldKey.empty()) {
        auto it = entry.customFields.find(_customFieldKey);
        if (it != entry.customFields.end()) {
            return it->second;
        }
    }
    return ""; // Default for unknown field or missing custom field
}

void TopNFieldValuesCollector::collect(const LogEntry& entry) {
    std::string value = getFieldValueAsString(entry);
    if (!value.empty()) { // Only count if a valid value was extracted
        _counts[value]++;
    }
}

json TopNFieldValuesCollector::generateReport() const {
    std::vector<std::pair<std::string, int>> sortedCounts(_counts.begin(), _counts.end());
    std::sort(sortedCounts.begin(), sortedCounts.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    if (sortedCounts.size() > static_cast<size_t>(_topN)) {
        sortedCounts.resize(_topN);
    }

    json report;
    report["name"] = getName();
    report["top_n"] = _topN;
    report["target_field"] = _targetFieldName;
    if (!_customFieldKey.empty()) {
        report["custom_field_key"] = _customFieldKey;
    }
    report["values"] = json::array();
    for(const auto& p : sortedCounts) {
        report["values"].push_back({
            {"value", p.first},
            {"count", p.second}
        });
    }
    return report;
}

// --- New functionality for Iteration 11 ---

// Assuming 'Statistics' is a namespace based on header file content not defining a Statistics class.
namespace Statistics {

    // Factory function to create statistic collectors based on configuration.
    // This function is responsible for instantiating the correct collector
    // based on the provided StatisticConfig.
    std::unique_ptr<IStatisticCollector> createCollector(const StatisticConfig& config) {
        switch (config.type) {
            case StatisticType::UNIQUE_MESSAGES:
                return std::make_unique<UniqueMessagesCollector>();
            case StatisticType::TOP_MESSAGES: {
                int topN = 10; // Default value
                if (config.params.count("top_n")) {
                    int parsedTopN = 0;
                    if (tryParseStrictPositiveInt(config.params.at("top_n"), parsedTopN)) {
                        topN = parsedTopN;
                    }
                }
                return std::make_unique<TopMessagesCollector>(topN);
            }
            case StatisticType::ENTRY_RATE:
                return std::make_unique<EntryRateCollector>();
            case StatisticType::LOG_LEVEL_COUNT:
                return std::make_unique<LogLevelCountCollector>();
            case StatisticType::FIELD_VALUE_COUNT: {
                if (!config.params.count("target_field")) {
                    // Error: target_field is required. Return nullptr to indicate failure.
                    return nullptr; 
                }
                const auto targetFieldOpt = normalizeTargetFieldName(config.params.at("target_field"));
                if (!targetFieldOpt) {
                    return nullptr;
                }
                const std::string targetField = *targetFieldOpt;
                if (targetField == "customFields" && config.params.count("custom_field_key")) {
                    std::string customFieldKey = config.params.at("custom_field_key");
                    trimInPlace(customFieldKey);
                    if (customFieldKey.empty()) {
                        return nullptr;
                    }
                    return std::make_unique<FieldValueCountCollector>(targetField, customFieldKey);
                } else {
                    if (targetField == "customFields") {
                        return nullptr;
                    }
                    return std::make_unique<FieldValueCountCollector>(targetField);
                }
            }
            case StatisticType::TOP_N_FIELD_VALUES: {
                if (!config.params.count("target_field") || !config.params.count("top_n")) {
                    // Error: target_field and top_n are required. Return nullptr.
                    return nullptr;
                }
                int topN = 0;
                if (!tryParseStrictPositiveInt(config.params.at("top_n"), topN)) {
                    return nullptr;
                }
                
                const auto targetFieldOpt = normalizeTargetFieldName(config.params.at("target_field"));
                if (!targetFieldOpt) {
                    return nullptr;
                }
                const std::string targetField = *targetFieldOpt;
                if (targetField == "customFields" && config.params.count("custom_field_key")) {
                    std::string customFieldKey = config.params.at("custom_field_key");
                    trimInPlace(customFieldKey);
                    if (customFieldKey.empty()) {
                        return nullptr;
                    }
                    return std::make_unique<TopNFieldValuesCollector>(topN, targetField, customFieldKey);
                } else {
                    if (targetField == "customFields") {
                        return nullptr;
                    }
                    return std::make_unique<TopNFieldValuesCollector>(topN, targetField);
                }
            }
            case StatisticType::UNKNOWN:
            default:
                // Handle unknown statistic type. Return nullptr.
                return nullptr;
        }
    }

} // End of Statistics namespace
