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

#include "LogTypes.h"

enum class StatisticType {
    COUNT_BY_LEVEL,         // Count log entries per level
    COUNT_TOTAL,            // Total number of log entries
    OCCURRENCE_COUNT,       // Count occurrences of a specific pattern/value in a field
    TOP_N_OCCURRENCES,      // Find top N most frequent values in a field
    TIME_RANGE,             // Analyze time distribution (e.g., first/last entry, duration)
    CUSTOM_AGGREGATION      // Placeholder for future, more complex custom aggregates
};

// Configuration for a single statistic to be generated
struct StatisticConfig {
    StatisticType type;
    std::optional<LogEntryField> field; // Field relevant for the statistic (e.g., for OCCURRENCE_COUNT)
    std::optional<std::string> pattern; // Pattern to search for (e.g., for OCCURRENCE_COUNT)
    std::optional<int> topN;            // For TOP_N_OCCURRENCES
    std::optional<std::vector<LogEntryField>> groupByFields; // Group statistics by these fields

    // Constructor for general statistics
    StatisticConfig(StatisticType t) : type(t) {}
    // Constructor for field-specific statistics
    StatisticConfig(StatisticType t, LogEntryField f) : type(t), field(f) {}
    // Constructor for pattern-specific statistics
    StatisticConfig(StatisticType t, LogEntryField f, std::string p) : type(t), field(f), pattern(std::move(p)) {}
    // Constructor for top N occurrences
    StatisticConfig(StatisticType t, LogEntryField f, int n) : type(t), field(f), topN(n) {}
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
