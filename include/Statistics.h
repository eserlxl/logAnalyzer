#ifndef STATISTICS_H
#define STATISTICS_H

#include <vector>
#include <chrono>
#include <map>
#include <string>
#include <functional>
#include <optional>
#include <span>
#include <mutex>
#include <utility> // For std::move
#include <nlohmann/json.hpp> // Required for JSON serialization
#include "LogTypes.h"
#include "Utils.h" // Required for utility functions like logEntryFieldToString
#include "Utils.h" // Required for utility functions like logEntryFieldToString
#include "Utils.h" // Required for utility functions like logEntryFieldToString
#include "Utils.h" // Required for utility functions like logEntryFieldToString

// Forward declarations to break circular dependency with Utils.h
namespace Utils {
    std::string statisticTypeToString(StatisticType type);
    StatisticType stringToStatisticType(const std::string& typeStr);
    std::string logEntryFieldToString(LogEntryField field);
    LogEntryField stringToLogEntryField(const std::string& fieldStr);
}

enum class StatisticType {
    COUNT_BY_LEVEL,         // Count log entries per level
    COUNT_TOTAL,            // Total number of log entries
    OCCURRENCE_COUNT,       // Count occurrences of a specific pattern/value in a field
    TOP_N_OCCURRENCES,      // Find top N most frequent values in a field
    TIME_RANGE,             // Analyze time distribution (e.g., first/last entry, duration)
    CUSTOM_AGGREGATION,      // Placeholder for future, more complex custom aggregates

    // New aggregation types for numeric fields
    SUM,                 // New: Sum of numeric field values
    AVERAGE,             // New: Average of numeric field values
    MIN,                 // New: Minimum of numeric field values
    MAX,                  // New: Maximum of numeric field values
    UNKNOWN
};

// Configuration for a single statistic to be generated
struct StatisticConfig {
    StatisticType type;
    std::optional<LogEntryField> field; // Field relevant for the statistic (e.g., for OCCURRENCE_COUNT, SUM, AVG)
    std::optional<std::string> pattern; // Pattern to search for (e.g., for OCCURRENCE_COUNT)
    std::optional<int> topN;            // For TOP_N_OCCURRENCES

    // New: Field to group statistics by
    std::optional<LogEntryField> groupByField; 

    // Default constructor
    StatisticConfig() = default;

    // Constructor for general statistics (no field, pattern, topN, groupByField)
    StatisticConfig(StatisticType t) : type(t) {}

    // Constructor for field-specific statistics (e.g., COUNT_BY_LEVEL, SUM, AVG, MIN, MAX without group by)
    StatisticConfig(StatisticType t, LogEntryField f) : type(t), field(f) {}

    // Constructor for field-specific statistics with group by (e.g., SUM, AVG, MIN, MAX with group by)
    StatisticConfig(StatisticType t, LogEntryField f, LogEntryField groupBy) 
        : type(t), field(f), groupByField(groupBy) {}

    // Constructor for pattern-specific statistics (e.g., OCCURRENCE_COUNT)
    StatisticConfig(StatisticType t, LogEntryField f, std::string p) 
        : type(t), field(f), pattern(std::move(p)) {}

    // Constructor for top N occurrences
    StatisticConfig(StatisticType t, LogEntryField f, int n) 
        : type(t), field(f), topN(n) {}

    // Constructor for pattern-specific statistics with group by (e.g., OCCURRENCE_COUNT group by)
    StatisticConfig(StatisticType t, LogEntryField f, std::string p, LogEntryField groupBy) 
        : type(t), field(f), pattern(std::move(p)), groupByField(groupBy) {}

    // Constructor for top N occurrences with group by
    StatisticConfig(StatisticType t, LogEntryField f, int n, LogEntryField groupBy) 
        : type(t), field(f), topN(n), groupByField(groupBy) {}
};

// --- JSON Conversion for StatisticConfig ---
inline void to_json(nlohmann::json& j, const StatisticConfig& sc) {
    j = nlohmann::json{
        {"type", Utils::statisticTypeToString(sc.type)}
    };
    if (sc.field) {
        j["field"] = Utils::logEntryFieldToString(*sc.field);
    }
    if (sc.pattern) {
        j["pattern"] = *sc.pattern;
    }
    if (sc.topN) {
        j["topN"] = *sc.topN;
    }
    if (sc.groupByField) {
        j["groupByField"] = Utils::logEntryFieldToString(*sc.groupByField);
    }
}

inline void from_json(const nlohmann::json& j, StatisticConfig& sc) {
    std::vector<std::string> errors;

    if (j.contains("type") && j.at("type").is_string()) {
        sc.type = Utils::stringToStatisticType(j.at("type").get<std::string>());
    } else {
        errors.push_back("StatisticConfig is missing or has invalid 'type'.");
    }

    if (j.contains("field") && j.at("field").is_string()) {
        std::string fieldStr = j.at("field").get<std::string>();
        // IMPORTANT: stringToLogEntryField only handles standard fields.
        // If custom fields can be used here, this needs adjustment.
        sc.field = Utils::stringToLogEntryField(fieldStr); 
        if (sc.field == LogEntryField::UNKNOWN && fieldStr != "UNKNOWN") {
             errors.push_back("StatisticConfig has an unrecognized 'field' string: " + fieldStr);
        }
    } // field is optional

    if (j.contains("pattern") && j.at("pattern").is_string()) {
        sc.pattern = j.at("pattern").get<std::string>();
    } // pattern is optional

    if (j.contains("topN") && j.at("topN").is_number_integer()) {
        sc.topN = j.at("topN").get<int>();
    } // topN is optional

    if (j.contains("groupByField") && j.at("groupByField").is_string()) {
        std::string groupByFieldStr = j.at("groupByField").get<std::string>();
        sc.groupByField = Utils::stringToLogEntryField(groupByFieldStr);
        if (sc.groupByField == LogEntryField::UNKNOWN && groupByFieldStr != "UNKNOWN") {
             errors.push_back("StatisticConfig has an unrecognized 'groupByField' string: " + groupByFieldStr);
        }
    } // groupByField is optional

    if (!errors.empty()) {
        throw std::runtime_error(errors[0]);
    }
}
// New: Enum for how statistics results should be presented
enum class StatisticOutputFormat {
    PLAINTEXT_TABLE, // Default
    JSON,
    CSV,
    UNKNOWN
};


class Statistics {
public:
    // Constructor taking a non-owning view of the log entries.
    // The class makes an internal copy, so the original data's lifetime is not an issue.
    explicit Statistics(std::span<const LogEntry> entries);

    // Enable move semantics.
    Statistics(const Statistics&) = delete;
    Statistics& operator=(const Statistics&) = delete;
    Statistics(Statistics&&) = default;
    Statistics& operator=(Statistics&&) = default;

    // --- Existing Methods (Refactored) ---

    // No longer take 'entries' as an argument.
    std::map<LogLevel, int> calculateLogLevelDistribution() const;
    std::map<std::string, int> calculateUniqueMessageCounts() const;

    // --- API Extensions & New Functionality ---
    
    // getTopMessages becomes getRankedMessages with more options
    enum class SortOrder { Ascending, Descending };
    std::vector<std::pair<std::string, int>> getRankedMessages(
        size_t n, 
        SortOrder order = SortOrder::Descending) const;

    // More efficient time-based calculations
    std::vector<TimeWindowStats> getLogFrequencyDistributionOverTime(std::chrono::seconds windowSize) const;
    std::vector<TimeGap> findTimeGaps(std::chrono::milliseconds minGapDuration) const;
    double calculateAverageEntryRate() const;
    
    // --- New Methods for Iteration 5 ---

    // Grouping by a key extractor function.
    // Allows grouping by sourceFile, structuredField keys, etc.
    using GroupKeyExtractor = std::function<std::optional<std::string>(const LogEntry&)>;
    std::map<std::string, int> calculateDistributionByGroup(GroupKeyExtractor extractor) const;

    // Time Gap Percentiles
    // Calculates the time gap duration at a given percentile (e.g., 0.99 for p99).
    std::chrono::nanoseconds getTimeGapPercentile(double percentile) const;

    // Burst Detection
    struct LogBurst {
        std::chrono::system_clock::time_point startTime;
        std::chrono::system_clock::time_point endTime;
        size_t eventCount;
        double peakRate; // events per second
    };
    std::vector<LogBurst> findLogBursts(std::chrono::seconds windowSize, double thresholdMultiplier) const;

private:
    // Internal copy of entries, sorted by timestamp. For efficient time-based lookups.
    std::vector<LogEntry> m_sortedEntries; 

    // Cache for time gaps, computed on demand.
    mutable std::vector<std::chrono::nanoseconds> m_timeGaps;
    mutable std::once_flag m_timeGapsCalculatedFlag;

    // Helper to lazily compute time gaps.
    void ensureTimeGapsAreCalculated() const;

    // Helper to find the index of a log entry by its timestamp.
    size_t findIndexByTimestamp(std::chrono::system_clock::time_point ts) const;
};

#endif // STATISTICS_H
