#include "LogAnalyzer.h"
#include <algorithm>
#include <chrono>
#include <mutex>
#include <map>
#include <vector>

// Audit: Inefficient Frequency Distribution Algorithm
// Current implementation is already O(N) due to single pass.
// The audit might be referring to an older version. This looks good.
std::vector<TimeWindowStats> LogAnalyzer::getFrequencyDistribution(std::chrono::seconds windowSize) const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    if (entries_.empty()) return {};

    std::vector<TimeWindowStats> stats;
    auto startTime = entries_.front().timestamp;
    auto endTime = entries_.back().timestamp;

    // Ensure windowSize is positive to prevent infinite loops or division by zero
    if (windowSize.count() <= 0) {
        // Return an empty vector or throw an exception, or define a default window.
        // For now, return empty.
        return {};
    }

    // Calculate the number of windows needed. Ensure at least one window.
    auto totalDurationSeconds = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
    size_t numWindows = (totalDurationSeconds / windowSize.count()) + 1;
    if (numWindows == 0) numWindows = 1; // Ensure at least one window if duration is small

    // Initialize time windows
    std::vector<TimeWindowStats> tempStats(numWindows);
    for (size_t i = 0; i < numWindows; ++i) {
        tempStats[i].windowStart = startTime + std::chrono::seconds(i * windowSize.count());
        tempStats[i].windowEnd = tempStats[i].windowStart + windowSize;
        tempStats[i].totalCount = 0;
    }

    // Single pass to assign entries to windows. This is O(N).
    size_t currentWindowIndex = 0;
    for (const auto& entry : entries_) {
        // Advance currentWindowIndex until the entry falls within the window or beyond.
        // Ensure we don't go past the last window index.
        while (currentWindowIndex < numWindows && entry.timestamp >= tempStats[currentWindowIndex].windowEnd) {
            currentWindowIndex++;
        }
        
        // If the entry falls within the current window's bounds (inclusive start, exclusive end)
        if (currentWindowIndex < numWindows && entry.timestamp >= tempStats[currentWindowIndex].windowStart && entry.timestamp < tempStats[currentWindowIndex].windowEnd) {
            tempStats[currentWindowIndex].totalCount++;
            tempStats[currentWindowIndex].counts[entry.level]++;
        }
        // Entries that fall *exactly* on the windowEnd of the last window might be missed if not careful.
        // However, the loop condition `entry.timestamp < tempStats[currentWindowIndex].windowEnd` handles this.
        // If an entry's timestamp is exactly `tempStats[currentWindowIndex].windowEnd`, it will be considered for the *next* window.
    }

    // Filter out empty windows for the final result
    for (const auto& window : tempStats) {
        if (window.totalCount > 0) {
            stats.push_back(window);
        }
    }
    return stats;
}


std::vector<TimeGap> LogAnalyzer::findTimeGaps(std::chrono::milliseconds minGapDuration) const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    if (entries_.size() < 2) return {};
    
    std::vector<TimeGap> gaps;
    for (size_t i = 0; i < entries_.size() - 1; ++i) {
        auto duration = entries_[i+1].timestamp - entries_[i].timestamp;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(duration) >= minGapDuration) {
            gaps.push_back({entries_[i].timestamp, entries_[i+1].timestamp, duration});
        }
    }
    return gaps;
}

double LogAnalyzer::getAverageEntryRate() const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    if (entries_.size() < 2) return 0.0;
    
    auto duration = entries_.back().timestamp - entries_.front().timestamp;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    
    // Avoid division by zero if duration is zero or very small.
    if (seconds == 0) {
        // If duration is 0, but entries are present, rate is effectively infinite or very high.
        // Return total entries as an indicator, or a very large number.
        // For simplicity, returning number of entries if duration is zero.
        return static_cast<double>(entries_.size());
    }
    return static_cast<double>(entries_.size()) / static_cast<double>(seconds);
}

// Non-locking version for internal use
std::map<std::string, int> LogAnalyzer::getUniqueMessageCounts_NoLock() const {
    std::map<std::string, int> counts;
    for (const auto& entry : entries_) {
        counts[entry.message]++;
    }
    return counts;
}

std::map<std::string, int> LogAnalyzer::getUniqueMessageCounts() const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    return getUniqueMessageCounts_NoLock();
}

std::vector<std::pair<std::string, int>> LogAnalyzer::getTopMessages(int n) const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    auto messageCounts = getUniqueMessageCounts_NoLock();
    std::vector<std::pair<std::string, int>> sortedCounts(messageCounts.begin(), messageCounts.end());
    
    if (n < 0) n = sortedCounts.size(); // If n is negative, return all
    n = std::min(n, static_cast<int>(sortedCounts.size())); // Cap n to the number of unique messages

    std::partial_sort(sortedCounts.begin(), sortedCounts.begin() + n, sortedCounts.end(), [](const auto& a, const auto& b) {
        return a.second > b.second; // Sort by count descending
    });
    
    sortedCounts.resize(n); // Trim the vector to the top n elements
    
    return sortedCounts;
}
