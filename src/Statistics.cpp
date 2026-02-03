#include "Statistics.h"
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <iostream> // For debug prints

Statistics::Statistics(std::span<const LogEntry> entries)
    : m_entries(entries), m_sortedEntries(entries.begin(), entries.end()) {
    std::sort(m_sortedEntries.begin(), m_sortedEntries.end(), [](const auto& a, const auto& b) {
        return a.timestamp < b.timestamp;
    });
}

std::map<LogLevel, int> Statistics::calculateLogLevelDistribution() const {
    std::map<LogLevel, int> distribution;
    for (const auto& entry : m_entries) {
        distribution[entry.level]++;
    }
    return distribution;
}

std::map<std::string, int> Statistics::calculateUniqueMessageCounts() const {
    std::map<std::string, int> counts;
    for (const auto& entry : m_entries) {
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

    auto currentWindowStart = minTime;
    auto it_entry_current = m_sortedEntries.begin();

    while (currentWindowStart <= maxTime || (result.empty() && !m_sortedEntries.empty())) {
        TimeWindowStats stats;
        stats.windowStart = currentWindowStart;
        stats.windowEnd = currentWindowStart + windowSize;
        stats.totalCount = 0;

        // Skip entries that are before the current window's start.
        while (it_entry_current != m_sortedEntries.end() && it_entry_current->timestamp < stats.windowStart) {
            ++it_entry_current;
        }

        // Iterate through entries belonging to the current window [stats.windowStart, stats.windowEnd)
        auto window_iterator = it_entry_current;
        while (window_iterator != m_sortedEntries.end() && window_iterator->timestamp < stats.windowEnd) {
            stats.counts[window_iterator->level]++;
            stats.totalCount++;
            ++window_iterator;
        }
        
        result.push_back(stats);
        
        currentWindowStart += windowSize;
        it_entry_current = window_iterator;

        if (it_entry_current == m_sortedEntries.end() && currentWindowStart > maxTime && !result.empty()) {
            break;
        }
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
    for (const auto& entry : m_entries) {
        if (auto key = extractor(entry)) {
            distribution[*key]++;
        }
    }
    return distribution;
}

void Statistics::ensureTimeGapsAreCalculated() const {
    if (m_timeGapsCalculated) {
        return;
    }
    if (m_sortedEntries.size() < 2) {
        m_timeGapsCalculated = true;
        return;
    }

    m_timeGaps.reserve(m_sortedEntries.size() - 1);
    for (size_t i = 0; i < m_sortedEntries.size() - 1; ++i) {
        auto diff = m_sortedEntries[i+1].timestamp - m_sortedEntries[i].timestamp;
        m_timeGaps.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(diff));
    }
    std::sort(m_timeGaps.begin(), m_timeGaps.end());
    m_timeGapsCalculated = true;
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
        return {};
    }
    
    const double rateThreshold = overallAverageRate * thresholdMultiplier;
    std::vector<LogBurst> bursts;

    std::cerr << "--- findLogBursts Debug ---" << std::endl;
    std::cerr << "Overall Average Rate: " << overallAverageRate << " e/s" << std::endl;
    std::cerr << "Threshold Multiplier: " << thresholdMultiplier << std::endl;
    std::cerr << "Rate Threshold: " << rateThreshold << " e/s" << std::endl;
    std::cerr << "Window Size: " << windowSize.count() << "s" << std::endl;
    
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

        double currentRate = (windowDurationSec > 0) ? (currentWindowEvents / windowDurationSec) : static_cast<double>(currentWindowEvents);

        std::cerr << "Iteration right=" << right << ", left=" << left << std::endl;
        std::cerr << "  Window: [" << std::chrono::duration_cast<std::chrono::milliseconds>(m_sortedEntries[left].timestamp.time_since_epoch()).count() << "ms, "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(currentEntry.timestamp.time_since_epoch()).count() << "ms]" << std::endl;
        std::cerr << "  Current Window Events: " << currentWindowEvents << ", Duration: " << windowDurationSec << "s, Rate: " << currentRate << " e/s" << std::endl;


        if (currentRate > rateThreshold) {
            std::cerr << "  Rate " << currentRate << " > Threshold " << rateThreshold << ". Potential burst." << std::endl;
            if (!bursts.empty() && bursts.back().endTime >= m_sortedEntries[left].timestamp) {
                // Merge with the previous burst.
                std::cerr << "    Merging with previous burst." << std::endl;
                size_t startIdxOfPrevBurst = findIndexByTimestamp(bursts.back().startTime);

                if (startIdxOfPrevBurst != static_cast<size_t>(-1)) {
                    // A merge is happening. Update the end time to the current entry's timestamp.
                    bursts.back().endTime = currentEntry.timestamp;

                    // Recalculate eventCount to span from the original start to the new end.
                    bursts.back().eventCount = right - startIdxOfPrevBurst + 1;
                    
                    // Recalculate the duration and rate for the entire merged burst.
                    const auto mergedDuration = std::chrono::duration_cast<std::chrono::duration<double>>(
                        bursts.back().endTime - bursts.back().startTime).count();
                    
                    if (mergedDuration > 0) {
                        bursts.back().peakRate = static_cast<double>(bursts.back().eventCount) / mergedDuration;
                    } else {
                        bursts.back().peakRate = static_cast<double>(bursts.back().eventCount); // Handle zero duration case
                    }
                    std::cerr << "    Merged Burst updated: Start=" << std::chrono::duration_cast<std::chrono::milliseconds>(bursts.back().startTime.time_since_epoch()).count() << "ms, End="
                              << std::chrono::duration_cast<std::chrono::milliseconds>(bursts.back().endTime.time_since_epoch()).count() << "ms, Count=" << bursts.back().eventCount
                              << ", Rate=" << bursts.back().peakRate << std::endl;
                } else {
                    // Fallback: If `startTime` not found, we can't precisely update `eventCount`.
                    // This scenario should ideally not occur if `startTime` is always from `m_sortedEntries`.
                    // For this iteration, we prioritize accurate `eventCount` and assume `findIndexByTimestamp` works.
                    // If it fails, `eventCount` will remain the old value from before the merge, which is incorrect.
                    std::cerr << "    WARNING: startIdxOfPrevBurst not found for merging." << std::endl;
                }
            } else {
                // This is a new burst.
                std::cerr << "    Creating new burst." << std::endl;
                bursts.push_back({
                    m_sortedEntries[left].timestamp,   // startTime of the new burst
                    currentEntry.timestamp,            // endTime of the new burst
                    currentWindowEvents,               // eventCount for this new burst
                    currentRate                        // peakRate for this new burst
                });
                 std::cerr << "    New Burst created: Start=" << std::chrono::duration_cast<std::chrono::milliseconds>(bursts.back().startTime.time_since_epoch()).count() << "ms, End="
                              << std::chrono::duration_cast<std::chrono::milliseconds>(bursts.back().endTime.time_since_epoch()).count() << "ms, Count=" << bursts.back().eventCount
                              << ", Rate=" << bursts.back().peakRate << std::endl;
            }
        }
    }
    std::cerr << "--- findLogBursts Debug End ---" << std::endl;
    return bursts;
}
