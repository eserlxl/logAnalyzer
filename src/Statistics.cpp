#include "Statistics.h"
#include <algorithm>
#include <numeric>

std::map<LogLevel, int> Statistics::calculateLogLevelDistribution(const std::vector<LogEntry>& entries) {
    std::map<LogLevel, int> distribution;
    for (const auto& entry : entries) {
        distribution[entry.level]++;
    }
    return distribution;
}

std::map<std::string, int> Statistics::calculateUniqueMessageCounts(const std::vector<LogEntry>& entries) {
    std::map<std::string, int> counts;
    for (const auto& entry : entries) {
        counts[entry.message]++;
    }
    return counts;
}

std::vector<std::pair<std::string, int>> Statistics::getTopMessages(const std::vector<LogEntry>& entries, int n) {
    auto counts = calculateUniqueMessageCounts(entries);
    std::vector<std::pair<std::string, int>> sortedCounts(counts.begin(), counts.end());
    
    std::partial_sort(sortedCounts.begin(), 
                      sortedCounts.begin() + std::min<size_t>(n, sortedCounts.size()), 
                      sortedCounts.end(), 
                      [](const auto& a, const auto& b) {
                          return a.second > b.second;
                      });

    if (sortedCounts.size() > static_cast<size_t>(n)) {
        sortedCounts.resize(n);
    }
    return sortedCounts;
}

std::vector<TimeWindowStats> Statistics::getLogFrequencyDistributionOverTime(
    const std::vector<LogEntry>& entries, 
    std::chrono::seconds windowSize) {
    
    if (entries.empty()) return {};

    std::vector<TimeWindowStats> result;
    auto minTime = entries.front().timestamp;
    auto maxTime = entries.front().timestamp;

    for (const auto& entry : entries) {
        if (entry.timestamp < minTime) minTime = entry.timestamp;
        if (entry.timestamp > maxTime) maxTime = entry.timestamp;
    }

    for (auto start = minTime; start <= maxTime; start += windowSize) {
        TimeWindowStats stats;
        stats.windowStart = start;
        stats.windowEnd = start + windowSize;
        stats.totalCount = 0;
        
        for (const auto& entry : entries) {
            if (entry.timestamp >= stats.windowStart && entry.timestamp < stats.windowEnd) {
                stats.counts[entry.level]++;
                stats.totalCount++;
            }
        }
        result.push_back(stats);
    }

    return result;
}

std::vector<TimeGap> Statistics::findTimeGaps(
    const std::vector<LogEntry>& entries, 
    std::chrono::milliseconds minGapDuration) {
    
    if (entries.size() < 2) return {};

    std::vector<TimeGap> gaps;
    std::vector<LogEntry> sortedEntries = entries;
    std::sort(sortedEntries.begin(), sortedEntries.end(), [](const auto& a, const auto& b) {
        return a.timestamp < b.timestamp;
    });

    for (size_t i = 0; i < sortedEntries.size() - 1; ++i) {
        auto diff = sortedEntries[i+1].timestamp - sortedEntries[i].timestamp;
        if (diff >= minGapDuration) {
            gaps.push_back({sortedEntries[i].timestamp, sortedEntries[i+1].timestamp, diff});
        }
    }

    return gaps;
}

double Statistics::calculateAverageEntryRate(const std::vector<LogEntry>& entries) {
    if (entries.size() < 2) return 0.0;

    auto minTime = entries.front().timestamp;
    auto maxTime = entries.front().timestamp;

    for (const auto& entry : entries) {
        if (entry.timestamp < minTime) minTime = entry.timestamp;
        if (entry.timestamp > maxTime) maxTime = entry.timestamp;
    }

    auto totalDuration = std::chrono::duration_cast<std::chrono::seconds>(maxTime - minTime).count();
    if (totalDuration == 0) return static_cast<double>(entries.size());

    return static_cast<double>(entries.size()) / totalDuration;
}
