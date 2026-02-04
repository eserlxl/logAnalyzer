#include "Statistics.h"
#include <algorithm>
#include <vector>
#include <stdexcept> // For std::stoi exception handling

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
    if (entry.timestamp.has_value()) {
        _timestamps.push_back(entry.timestamp.value());
    }
}

json EntryRateCollector::generateReport() const {
    json report;
    report["name"] = getName();
    if (_timestamps.size() < 2) {
        report["average_rate_per_sec"] = 0;
        report["total_entries"] = _timestamps.size();
        report["duration_sec"] = 0;
        return report;
    }

    std::sort(_timestamps.begin(), _timestamps.end());
    auto duration = _timestamps.back() - _timestamps.front();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

    double rate = (secs > 0) ? static_cast<double>(_timestamps.size()) / secs : static_cast<double>(_timestamps.size());
    
    report["average_rate_per_sec"] = rate;
    report["total_entries"] = _timestamps.size();
    report["duration_sec"] = secs;

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
    : _targetFieldName(targetFieldName) {}

FieldValueCountCollector::FieldValueCountCollector(const std::string& targetFieldName, const std::string& customFieldKey)
    : _targetFieldName(targetFieldName), _customFieldKey(customFieldKey) {}

std::string FieldValueCountCollector::getFieldValueAsString(const LogEntry& entry) const {
    if (_targetFieldName == "level") {
        // Assuming logLevelToString is available globally or via included headers
        return Utils::logLevelToString(entry.level);
    } else if (_targetFieldName == "message") {
        return entry.message;
    } else if (_targetFieldName == "sourceFile") {
        return entry.sourceFile;
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
    : _topN(topN), _targetFieldName(targetFieldName) {}

TopNFieldValuesCollector::TopNFieldValuesCollector(int topN, const std::string& targetFieldName, const std::string& customFieldKey)
    : _topN(topN), _targetFieldName(targetFieldName), _customFieldKey(customFieldKey) {}

std::string TopNFieldValuesCollector::getFieldValueAsString(const LogEntry& entry) const {
    if (_targetFieldName == "level") {
        // Assuming logLevelToString is available globally or via included headers
        return Utils::logLevelToString(entry.level);
    } else if (_targetFieldName == "message") {
        return entry.message;
    } else if (_targetFieldName == "sourceFile") {
        return entry.sourceFile;
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
                    try {
                        topN = std::stoi(config.params.at("top_n"));
                    } catch (const std::exception& e) {
                        // Error handling for stoi. For now, proceed with default.
                        // In a real scenario, consider logging or returning an error indicator.
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
                std::string targetField = config.params.at("target_field");
                if (targetField == "customFields" && config.params.count("custom_field_key")) {
                    return std::make_unique<FieldValueCountCollector>(targetField, config.params.at("custom_field_key"));
                } else {
                    return std::make_unique<FieldValueCountCollector>(targetField);
                }
            }
            case StatisticType::TOP_N_FIELD_VALUES: {
                if (!config.params.count("target_field") || !config.params.count("top_n")) {
                    // Error: target_field and top_n are required. Return nullptr.
                    return nullptr;
                }
                int topN = 0;
                try {
                    topN = std::stoi(config.params.at("top_n"));
                } catch (const std::exception& e) {
                    // Error handling for stoi. Return nullptr.
                    return nullptr;
                }
                
                std::string targetField = config.params.at("target_field");
                if (targetField == "customFields" && config.params.count("custom_field_key")) {
                    return std::make_unique<TopNFieldValuesCollector>(topN, targetField, config.params.at("custom_field_key"));
                } else {
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
