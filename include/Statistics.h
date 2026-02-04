#pragma once

#include "LogTypes.h"
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
    UNKNOWN
};

struct StatisticConfig {
    StatisticType type;
    std::map<std::string, std::string> params; // e.g., {"top_n": "10"}
};

NLOHMANN_JSON_SERIALIZE_ENUM(StatisticType, {
    {StatisticType::UNKNOWN, "UNKNOWN"},
    {StatisticType::UNIQUE_MESSAGES, "UNIQUE_MESSAGES"},
    {StatisticType::TOP_MESSAGES, "TOP_MESSAGES"},
    {StatisticType::ENTRY_RATE, "ENTRY_RATE"}
})

inline void to_json(json& j, const StatisticConfig& sc) {
    j = json{{"type", sc.type}, {"params", sc.params}};
}

inline void from_json(const json& j, StatisticConfig& sc) {
    j.at("type").get_to(sc.type);
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
    std::vector<std::chrono::system_clock::time_point> _timestamps;
};
