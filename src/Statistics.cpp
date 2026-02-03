#include "Statistics.h"
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <iostream> // For debug prints

Statistics::Statistics(std::span<const LogEntry> entries)
    : m_sortedEntries(entries.begin(), entries.end()) {
    std::sort(m_sortedEntries.begin(), m_sortedEntries.end(), [](const auto& a, const auto& b) {
        return a.timestamp < b.timestamp;
    });
}

std::map<LogLevel, int> Statistics::calculateLogLevelDistribution() const {
    std::map<LogLevel, int> distribution;
    for (const auto& entry : m_sortedEntries) {
        distribution[entry.level]++;
    }
    return distribution;
}

std::map<std::string, int> Statistics::calculateUniqueMessageCounts() const {
    std::map<std::string, int> counts;
    for (const auto& entry : m_sortedEntries) {
        counts[entry.message]++;
    }
    return counts;
}

std::vector<std::pair<std::string, int>> Statistics::getRankedMessages(size_t n, SortOrder order) const {
    auto counts = calculateUniqueMessageCounts();
    std::vector<std::pair<std::string, int>> sortedCounts(counts.begin(), counts.end());

    std::function<bool(const std::pair<std::string, int>&, const std::pair<std::string, int>&)> comparator;
    if (order == SortOrder::Descending) {
        comparator = [](const auto& a, const auto& b) { return a.second > b.second; };
    } else {
        comparator = [](const auto& a, const auto& b) { return a.second < b.second; };
    }

    std::partial_sort(sortedCounts.begin(),
                      sortedCounts.begin() + std::min<size_t>(n, sortedCounts.size()),
                      sortedCounts.end(),
                      comparator);

    if (sortedCounts.size() > n) {
        sortedCounts.resize(n);
    }
    return sortedCounts;
}

std::vector<TimeWindowStats> Statistics::getLogFrequencyDistributionOverTime(std::chrono::seconds windowSize) const {
    if (m_sortedEntries.empty()) {
        return {};
    }
    if (windowSize <= std::chrono::seconds(0)) {
        throw std::invalid_argument("windowSize must be a positive duration.");
    }

    std::vector<TimeWindowStats> result;
    const auto minTime = m_sortedEntries.front().timestamp;
    const auto maxTime = m_sortedEntries.back().timestamp;

    auto it_entry_current = m_sortedEntries.begin();

    for (auto currentWindowStart = minTime; currentWindowStart <= maxTime; currentWindowStart += windowSize) {
        TimeWindowStats stats;
        stats.windowStart = currentWindowStart;
        stats.windowEnd = currentWindowStart + windowSize;
        stats.totalCount = 0;

        // Iterate through entries belonging to the current window [stats.windowStart, stats.windowEnd)
        while (it_entry_current != m_sortedEntries.end() && it_entry_current->timestamp < stats.windowEnd) {
            stats.counts[it_entry_current->level]++;
            stats.totalCount++;
            ++it_entry_current;
        }
        
        result.push_back(stats);
    }

    return result;
}

std::vector<TimeGap> Statistics::findTimeGaps(std::chrono::milliseconds minGapDuration) const {
    if (m_sortedEntries.size() < 2) {
        return {};
    }

    std::vector<TimeGap> gaps;
    for (size_t i = 0; i < m_sortedEntries.size() - 1; ++i) {
        const auto& entry1 = m_sortedEntries[i];
        const auto& entry2 = m_sortedEntries[i+1];
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(entry2.timestamp - entry1.timestamp);
        if (diff >= minGapDuration) {
            gaps.push_back({entry1.timestamp, entry2.timestamp, diff});
        }
    }

    return gaps;
}

double Statistics::calculateAverageEntryRate() const {
    if (m_sortedEntries.size() < 2) {
        return 0.0;
    }

    const auto totalDuration = std::chrono::duration_cast<std::chrono::seconds>(
        m_sortedEntries.back().timestamp - m_sortedEntries.front().timestamp
    ).count();

    if (totalDuration == 0) {
        return static_cast<double>(m_sortedEntries.size());
    }

    return static_cast<double>(m_sortedEntries.size()) / totalDuration;
}

std::map<std::string, int> Statistics::calculateDistributionByGroup(GroupKeyExtractor extractor) const {
    std::map<std::string, int> distribution;
    for (const auto& entry : m_sortedEntries) {
        if (auto key = extractor(entry)) {
            distribution[*key]++;
        }
    }
    return distribution;
}

void Statistics::ensureTimeGapsAreCalculated() const {
    std::call_once(m_timeGapsCalculatedFlag, [this]() {
        if (m_sortedEntries.size() < 2) {
            return;
        }

        m_timeGaps.reserve(m_sortedEntries.size() - 1);
        for (size_t i = 0; i < m_sortedEntries.size() - 1; ++i) {
            auto diff = m_sortedEntries[i+1].timestamp - m_sortedEntries[i].timestamp;
            m_timeGaps.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(diff));
        }
        std::sort(m_timeGaps.begin(), m_timeGaps.end());
    });
}

std::chrono::nanoseconds Statistics::getTimeGapPercentile(double percentile) const {
    if (percentile < 0.0 || percentile > 1.0) {
        throw std::invalid_argument("Percentile must be between 0.0 and 1.0");
    }

    ensureTimeGapsAreCalculated();

    if (m_timeGaps.empty()) {
        return std::chrono::nanoseconds(0);
    }

    // Use the NIST method for index calculation (R-7)
    // index = percentile * (N+1)
    // Since we want to use 0-based indexing, it becomes percentile * (N-1) for the continuous case
    const double index = percentile * (m_timeGaps.size() - 1);
    const size_t lowerIndex = static_cast<size_t>(index);
    const size_t upperIndex = std::min(lowerIndex + 1, m_timeGaps.size() - 1);
    const double fraction = index - lowerIndex;

    if (lowerIndex == upperIndex) {
        return m_timeGaps[lowerIndex];
    }
    
    const auto lowerValue = m_timeGaps[lowerIndex];
    const auto upperValue = m_timeGaps[upperIndex];

    const auto interpolatedValue = lowerValue + (upperValue - lowerValue) * fraction;
    return std::chrono::nanoseconds(static_cast<long long>(interpolatedValue.count()));
}

// Helper function to find the index of the first entry with a given timestamp.
// Assumes m_sortedEntries is sorted by timestamp.
// Returns static_cast<size_t>(-1) if timestamp is not found.
size_t Statistics::findIndexByTimestamp(std::chrono::system_clock::time_point ts) const {
    auto it = std::lower_bound(m_sortedEntries.begin(), m_sortedEntries.end(), ts, 
        [](const LogEntry& entry, const std::chrono::system_clock::time_point& t) {
        return entry.timestamp < t;
    });
    
    if (it != m_sortedEntries.end() && it->timestamp == ts) {
        return std::distance(m_sortedEntries.begin(), it);
    }
    return static_cast<size_t>(-1); // Sentinel for not found.
}

std::vector<Statistics::LogBurst> Statistics::findLogBursts(std::chrono::seconds windowSize, double thresholdMultiplier) const {
    if (m_sortedEntries.empty() || windowSize.count() <= 0) {
        return {};
    }

    const double overallAverageRate = calculateAverageEntryRate();
    if (overallAverageRate == 0) {
        // If overall average rate is 0, there are no events or only one event, thus no bursts.
        return {};
    }
    
    const double rateThreshold = overallAverageRate * thresholdMultiplier;
    std::vector<LogBurst> bursts;
    
    size_t left = 0;
    for (size_t right = 0; right < m_sortedEntries.size(); ++right) {
        const auto& currentEntry = m_sortedEntries[right];

        // Shrink window from the left
        while (currentEntry.timestamp - m_sortedEntries[left].timestamp > windowSize) {
            left++;
        }

        const size_t currentWindowEvents = right - left + 1;
        const auto windowDuration = currentEntry.timestamp - m_sortedEntries[left].timestamp;
        const auto windowDurationSec = std::chrono::duration_cast<std::chrono::duration<double>>(windowDuration).count();

        // Rate calculation: events per second.
        // If windowDurationSec is 0 (all events at the same timestamp), consider the rate to be
        // the number of events, interpreting it as `events / 1 second` for comparison.
        // This avoids division by zero and provides a quantifiable 'burstiness' for instantaneous events.
        double currentRate = (windowDurationSec > 0) ? (currentWindowEvents / windowDurationSec) : static_cast<double>(currentWindowEvents);

        if (currentRate > rateThreshold) {
            if (!bursts.empty() && bursts.back().endTime >= m_sortedEntries[left].timestamp) {
                // Merge with the previous burst.
                size_t startIdxOfPrevBurst = findIndexByTimestamp(bursts.back().startTime);
                // As bursts.back().startTime is always derived from m_sortedEntries[left].timestamp,
                // findIndexByTimestamp should always return a valid index.
                // If it somehow fails, it would indicate a logical inconsistency, but we proceed
                // assuming a valid index is found.
                
                // Update the end time to the current entry's timestamp.
                bursts.back().endTime = currentEntry.timestamp;

                // Recalculate eventCount to span from the original start to the new end.
                bursts.back().eventCount = right - startIdxOfPrevBurst + 1;
                
                // Recalculate the duration and rate for the entire merged burst.
                const auto mergedDuration = std::chrono::duration_cast<std::chrono::duration<double>>(
                    bursts.back().endTime - bursts.back().startTime).count();
                
                if (mergedDuration > 0) {
                    bursts.back().peakRate = static_cast<double>(bursts.back().eventCount) / mergedDuration;
                } else {
                    // Handle zero duration case for merged burst: use event count as rate.
                    bursts.back().peakRate = static_cast<double>(bursts.back().eventCount);
                }
            } else {
                // This is a new burst.
                bursts.push_back({
                    m_sortedEntries[left].timestamp,   // startTime of the new burst
                    currentEntry.timestamp,            // endTime of the new burst
                    currentWindowEvents,               // eventCount for this new burst
                    currentRate                        // peakRate for this new burst
                });
            }
        }
    }
    return bursts;
}
