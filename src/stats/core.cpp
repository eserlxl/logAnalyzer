// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "stats/core.h"
#include "stats/helpers.h"
#include <chrono>
#include <limits>
#include <vector>

using stats::detail::normalizeTargetFieldName;
using stats::detail::trimInPlace;
using stats::detail::tryParseStrictPositiveInt;
using stats::detail::extractFieldValue;

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
    : _targetFieldName(normalizeTargetFieldName(targetFieldName).value_or(targetFieldName)) {}

FieldValueCountCollector::FieldValueCountCollector(const std::string& targetFieldName, const std::string& customFieldKey)
    : _targetFieldName(normalizeTargetFieldName(targetFieldName).value_or(targetFieldName)), _customFieldKey(customFieldKey) {}

std::string FieldValueCountCollector::getFieldValueAsString(const LogEntry& entry) const {
    return stats::detail::extractFieldValue(entry, _targetFieldName, _customFieldKey);
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
    return stats::detail::extractFieldValue(entry, _targetFieldName, _customFieldKey);
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

// TimeBucketHistogramCollector
TimeBucketHistogramCollector::TimeBucketHistogramCollector(int bucketSeconds)
    : _bucketSeconds(bucketSeconds > 0 ? bucketSeconds : 60) {}

void TimeBucketHistogramCollector::collect(const LogEntry& entry) {
    if (!entry.timestamp) return;
    long long epoch = std::chrono::duration_cast<std::chrono::seconds>(
        entry.timestamp->time_since_epoch()).count();
    long long bucket = (epoch / _bucketSeconds) * _bucketSeconds;
    _counts[bucket]++;
}

json TimeBucketHistogramCollector::generateReport() const {
    json buckets = json::array();
    for (const auto& [start, count] : _counts) {
        buckets.push_back({{"start_time", start}, {"count", count}});
    }
    return {
        {"name", "time_bucket_histogram"},
        {"bucket_seconds", _bucketSeconds},
        {"buckets", buckets}
    };
}

// PercentileStatsCollector
PercentileStatsCollector::PercentileStatsCollector(std::string fieldName)
    : _fieldName(std::move(fieldName)) {}

void PercentileStatsCollector::collect(const LogEntry& entry) {
    auto it = entry.customFields.find(_fieldName);
    if (it == entry.customFields.end()) return;
    try {
        _values.push_back(std::stod(it->second));
    } catch (...) {}
}

json PercentileStatsCollector::generateReport() const {
    json report;
    report["name"] = "percentile_stats";
    report["field"] = _fieldName;
    report["count"] = _values.size();
    if (_values.empty()) {
        report["p50"] = nullptr;
        report["p95"] = nullptr;
        report["p99"] = nullptr;
        return report;
    }
    std::vector<double> sorted = _values;
    std::sort(sorted.begin(), sorted.end());
    auto percentile = [&](double p) -> double {
        double idx = p * static_cast<double>(sorted.size() - 1);
        size_t lo = static_cast<size_t>(idx);
        double frac = idx - static_cast<double>(lo);
        if (lo + 1 >= sorted.size()) return sorted.back();
        return sorted[lo] + frac * (sorted[lo + 1] - sorted[lo]);
    };
    report["p50"] = percentile(0.50);
    report["p95"] = percentile(0.95);
    report["p99"] = percentile(0.99);
    return report;
}

// GapDetectorCollector
GapDetectorCollector::GapDetectorCollector(long long thresholdMs)
    : _thresholdMs(thresholdMs > 0 ? thresholdMs : 1000) {}

void GapDetectorCollector::collect(const LogEntry& entry) {
    if (entry.timestamp) _timestamps.push_back(*entry.timestamp);
}

json GapDetectorCollector::generateReport() const {
    json report;
    report["name"] = "gap_detector";
    report["threshold_ms"] = _thresholdMs;
    json gaps = json::array();
    if (_timestamps.size() >= 2) {
        std::vector<std::chrono::system_clock::time_point> sorted = _timestamps;
        std::sort(sorted.begin(), sorted.end());
        for (size_t i = 1; i < sorted.size(); ++i) {
            auto gapMs = std::chrono::duration_cast<std::chrono::milliseconds>(sorted[i] - sorted[i-1]).count();
            if (gapMs > _thresholdMs) {
                gaps.push_back({
                    {"start", Utils::formatTimestamp(sorted[i-1])},
                    {"end",   Utils::formatTimestamp(sorted[i])},
                    {"duration_ms", gapMs}
                });
            }
        }
    }
    report["gap_count"] = static_cast<int>(gaps.size());
    report["gaps"] = gaps;
    return report;
}

// MovingAverageRateCollector
MovingAverageRateCollector::MovingAverageRateCollector(int bucketSeconds)
    : _bucketSeconds(bucketSeconds > 0 ? bucketSeconds : 60) {}

void MovingAverageRateCollector::collect(const LogEntry& entry) {
    if (!entry.timestamp) return;
    long long epoch = std::chrono::duration_cast<std::chrono::seconds>(
        entry.timestamp->time_since_epoch()).count();
    long long bucket = (epoch / _bucketSeconds) * _bucketSeconds;
    _counts[bucket]++;
    _total++;
}

json MovingAverageRateCollector::generateReport() const {
    json report;
    report["name"] = "moving_average_rate";
    report["bucket_seconds"] = _bucketSeconds;
    report["bucket_count"] = static_cast<int>(_counts.size());
    report["total_entries"] = _total;
    if (_counts.empty()) {
        report["mean_per_bucket"] = nullptr;
        report["min_per_bucket"] = nullptr;
        report["max_per_bucket"] = nullptr;
        return report;
    }
    int minCount = std::numeric_limits<int>::max();
    int maxCount = std::numeric_limits<int>::min();
    for (const auto& [bucket, count] : _counts) {
        if (count < minCount) minCount = count;
        if (count > maxCount) maxCount = count;
    }
    report["mean_per_bucket"] = static_cast<double>(_total) / static_cast<double>(_counts.size());
    report["min_per_bucket"] = minCount;
    report["max_per_bucket"] = maxCount;
    return report;
}

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
            case StatisticType::TIME_BUCKET_HISTOGRAM: {
                int bucketSeconds = 60;
                if (config.params.count("bucket")) {
                    int parsed = 0;
                    if (tryParseStrictPositiveInt(config.params.at("bucket"), parsed)) {
                        bucketSeconds = parsed;
                    }
                }
                return std::make_unique<TimeBucketHistogramCollector>(bucketSeconds);
            }
            case StatisticType::PERCENTILE_STATS: {
                auto it = config.params.find("field");
                if (it == config.params.end() || it->second.empty()) return nullptr;
                return std::make_unique<PercentileStatsCollector>(it->second);
            }
            case StatisticType::MOVING_AVERAGE_RATE: {
                int bucketSeconds = 60;
                if (config.params.count("bucket")) {
                    int parsed = 0;
                    if (tryParseStrictPositiveInt(config.params.at("bucket"), parsed)) {
                        bucketSeconds = parsed;
                    }
                }
                return std::make_unique<MovingAverageRateCollector>(bucketSeconds);
            }
            case StatisticType::FIND_GAPS: {
                long long thresholdMs = 1000;
                auto it = config.params.find("threshold_ms");
                if (it != config.params.end() && !it->second.empty()) {
                    long long parsed = 0;
                    const char* begin = it->second.data();
                    const char* end = begin + it->second.size();
                    auto [ptr, ec] = std::from_chars(begin, end, parsed);
                    if (ec == std::errc{} && ptr == end && parsed > 0) {
                        thresholdMs = parsed;
                    }
                }
                return std::make_unique<GapDetectorCollector>(thresholdMs);
            }
            case StatisticType::UNKNOWN:
            default:
                // Handle unknown statistic type. Return nullptr.
                return nullptr;
        }
    }

} // End of Statistics namespace
