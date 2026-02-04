#include "Statistics.h"
#include <algorithm>
#include <vector>

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
    _timestamps.push_back(entry.timestamp);
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
