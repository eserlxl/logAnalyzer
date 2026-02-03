#ifndef STATISTICS_H
#define STATISTICS_H

#include "LogTypes.h"
#include <map>
#include <string>
#include <vector>
#include <chrono>

class Statistics {
public:
    // Calculates the frequency of each log level
    std::map<LogLevel, int> calculateLogLevelDistribution(const std::vector<LogEntry>& entries);

    // Calculates the count of unique messages
    std::map<std::string, int> calculateUniqueMessageCounts(const std::vector<LogEntry>& entries);

    // Identifies the top N most frequent messages
    std::vector<std::pair<std::string, int>> getTopMessages(const std::vector<LogEntry>& entries, int n);

    // Calculates log frequency over time windows
    std::vector<TimeWindowStats> getLogFrequencyDistributionOverTime(
        const std::vector<LogEntry>& entries, 
        std::chrono::seconds windowSize);

    // Finds time gaps in log entries
    std::vector<TimeGap> findTimeGaps(
        const std::vector<LogEntry>& entries, 
        std::chrono::milliseconds minGapDuration);

    // Calculates the average entry rate
    double calculateAverageEntryRate(const std::vector<LogEntry>& entries);
};

#endif // STATISTICS_H
