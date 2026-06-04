// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include "core/log/types.h"
#include "utils/core.h"
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
    TIME_BUCKET_HISTOGRAM,  // Per-bucket entry count grouped by configurable time window
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
            st = StatisticType::UNKNOWN;
        }
    } else {
        st = StatisticType::UNKNOWN;
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
    sc.type = StatisticType::UNKNOWN;
    sc.params.clear();

    if (!j.is_object()) {
        return;
    }

    if (auto typeIt = j.find("type"); typeIt != j.end()) {
        typeIt->get_to(sc.type);
    }

    if (auto paramsIt = j.find("params"); paramsIt != j.end()) {
        if (!paramsIt->is_object()) {
            sc.type = StatisticType::UNKNOWN;
            return;
        }

        for (auto it = paramsIt->begin(); it != paramsIt->end(); ++it) {
            if (!it.value().is_string()) {
                // Keep parser no-throw: invalidate the config and let validation report it.
                sc.type = StatisticType::UNKNOWN;
                sc.params.clear();
                return;
            }
            sc.params[it.key()] = it.value().get<std::string>();
        }
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
    // New: Method to reset the collector's internal state
    virtual void reset() = 0;
};

// Concrete implementation for Unique Messages
class UniqueMessagesCollector : public IStatisticCollector {
public:
    void collect(const LogEntry& entry) override;
    json generateReport() const override;
    std::string getName() const override { return "unique_messages"; }
    void reset() override { _counts.clear(); }
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
    void reset() override { _counts.clear(); }
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
    void reset() override { _timestamps.clear(); }
private:
    mutable std::vector<std::chrono::system_clock::time_point> _timestamps;
};

// Concrete implementation for counting log levels
class LogLevelCountCollector : public IStatisticCollector {
public:
    void collect(const LogEntry& entry) override;
    json generateReport() const override;
    std::string getName() const override { return "log_level_count"; }
    void reset() override { _counts.clear(); }
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
    void reset() override { _counts.clear(); }

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
    void reset() override { _counts.clear(); }

private:
    int _topN;
    std::string _targetFieldName; // "level", "message", "sourceFile", or "customFields"
    std::string _customFieldKey;  // Only used if _targetFieldName is "customFields"
    std::map<std::string, int> _counts; // Stores string representation of field values and their counts

    // Helper to extract the value as string based on targetFieldName
    std::string getFieldValueAsString(const LogEntry& entry) const;
};

// Per-bucket entry count grouped by configurable time window
class TimeBucketHistogramCollector : public IStatisticCollector {
public:
    explicit TimeBucketHistogramCollector(int bucketSeconds = 60);
    void collect(const LogEntry& entry) override;
    json generateReport() const override;
    std::string getName() const override { return "time_bucket_histogram"; }
    void reset() override { _counts.clear(); }
private:
    int _bucketSeconds;
    std::map<long long, int> _counts; // key: bucket start epoch seconds
};

namespace Statistics {
    std::unique_ptr<IStatisticCollector> createCollector(const StatisticConfig& config);
}
