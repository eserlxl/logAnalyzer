#pragma once

#include "core/LogTypes.h"
#include "utils/UtilsCore.h"
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

enum class StatisticType {
    UNIQUE_MESSAGES,
    TOP_MESSAGES,
    ENTRY_RATE,
    LOG_LEVEL_COUNT,        // New: Count occurrences of each LogLevel
    FIELD_VALUE_COUNT,      // New: Count occurrences of unique values for a specified field
    TOP_N_FIELD_VALUES,     // New: Report top N most frequent values for a specified field
    UNKNOWN
};

inline void to_json(json& j, const StatisticType& st) {
    j = Utils::statisticTypeToString(st);
}

inline void from_json(const json& j, StatisticType& st) {
    if (j.is_string()) {
        auto opt = Utils::stringToStatisticType(j.get<std::string>());
        if (opt) {
            st = *opt;
        } else {
            throw std::runtime_error("StatisticConfig has an unrecognized type.");
        }
    } else {
        throw std::runtime_error("StatisticType must be a string.");
    }
}

struct StatisticConfig {
    StatisticType type;
    std::map<std::string, std::string> params; // e.g., {"top_n": "10"}, {"target_field": "level"}, {"custom_field_key": "transactionId"}
};

inline void to_json(json& j, const StatisticConfig& sc) {
    j = json{{"type", sc.type}, {"params", sc.params}};
}

inline void from_json(const json& j, StatisticConfig& sc) {
    if (j.contains("type")) {
        sc.type = j.at("type").get<StatisticType>();
    } else {
        throw std::runtime_error("StatisticConfig is missing 'type'.");
    }
    
    if (j.contains("params")) {
        j.at("params").get_to(sc.params);
    }
}


// Base interface for all statistic collectors
class IStatisticCollector {
public:
    virtual ~IStatisticCollector() = default;
    // Method to process a log entry and update internal statistics
    virtual void collect(const LogEntry& entry) = 0;
    // Method to generate the final report (e.g., as JSON or formatted string)
    virtual json generateReport() const = 0;
    virtual std::string getName() const = 0; // For identifying collectors
};

// Concrete implementation for Unique Messages
class UniqueMessagesCollector : public IStatisticCollector {
public:
    void collect(const LogEntry& entry) override;
    json generateReport() const override;
    std::string getName() const override { return "unique_messages"; }
private:
    std::map<std::string, int> _counts;
};

// Concrete implementation for Top N Messages
class TopMessagesCollector : public IStatisticCollector {
public:
    explicit TopMessagesCollector(int topN); // Constructor to set N
    void collect(const LogEntry& entry) override;
    json generateReport() const override;
    std::string getName() const override { return "top_messages"; }
private:
    int _topN;
    std::map<std::string, int> _counts;
};

// Concrete implementation for Entry Rate
class EntryRateCollector : public IStatisticCollector {
public:
    void collect(const LogEntry& entry) override;
    json generateReport() const override;
    std::string getName() const override { return "entry_rate"; }
private:
    mutable std::vector<std::chrono::system_clock::time_point> _timestamps;
};

// Concrete implementation for counting log levels
class LogLevelCountCollector : public IStatisticCollector {
public:
    void collect(const LogEntry& entry) override;
    json generateReport() const override;
    std::string getName() const override { return "log_level_count"; }
private:
    std::map<LogLevel, int> _counts;
};

// Generic implementation for counting unique values of a specified LogEntry field
class FieldValueCountCollector : public IStatisticCollector {
public:
    // Constructor to specify the target field
    explicit FieldValueCountCollector(const std::string& targetFieldName);
    explicit FieldValueCountCollector(const std::string& targetFieldName, const std::string& customFieldKey);

    void collect(const LogEntry& entry) override;
    json generateReport() const override;
    std::string getName() const override {
        if (_customFieldKey.empty()) {
            return "field_value_count_" + _targetFieldName;
        }
        return "field_value_count_" + _targetFieldName + "_" + _customFieldKey;
    }

private:
    std::string _targetFieldName; // "level", "message", "sourceFile", or "customFields"
    std::string _customFieldKey;  // Only used if _targetFieldName is "customFields"
    std::map<std::string, int> _counts; // Stores string representation of field values and their counts

    // Helper to extract the value as string based on targetFieldName
    std::string getFieldValueAsString(const LogEntry& entry) const;
};

// Generic implementation for reporting top N most frequent values of a specified LogEntry field
class TopNFieldValuesCollector : public IStatisticCollector {
public:
    // Constructor to specify topN and the target field
    explicit TopNFieldValuesCollector(int topN, const std::string& targetFieldName);
    explicit TopNFieldValuesCollector(int topN, const std::string& targetFieldName, const std::string& customFieldKey);

    void collect(const LogEntry& entry) override;
    json generateReport() const override;
    std::string getName() const override {
        if (_customFieldKey.empty()) {
            return "top_n_field_values_" + _targetFieldName;
        }
        return "top_n_field_values_" + _targetFieldName + "_" + _customFieldKey;
    }

private:
    int _topN;
    std::string _targetFieldName; // "level", "message", "sourceFile", or "customFields"
    std::string _customFieldKey;  // Only used if _targetFieldName is "customFields"
    std::map<std::string, int> _counts; // Stores string representation of field values and their counts

    // Helper to extract the value as string based on targetFieldName
    std::string getFieldValueAsString(const LogEntry& entry) const;
};

namespace Statistics {
    std::unique_ptr<IStatisticCollector> createCollector(const StatisticConfig& config);
}
