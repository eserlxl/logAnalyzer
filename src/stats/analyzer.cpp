// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer/core.h"
#include "stats/core.h"
#include <memory>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void LogAnalyzer::addStatisticCollector(std::shared_ptr<IStatisticCollector> collector) {
    if (collector) {
        collectors_.push_back(collector);
    }
}

void LogAnalyzer::processEntryForStatistics(const LogEntry& entry) {
    for (const auto& collector : collectors_) {
        collector->collect(entry);
    }
}

void LogAnalyzer::removeStatisticCollector(const std::shared_ptr<IStatisticCollector>& collector) {
    if (!collector) {
        return;
    }
    collectors_.erase(
        std::remove(collectors_.begin(), collectors_.end(), collector),
        collectors_.end());
}

void LogAnalyzer::clearStatisticCollectors() {
    collectors_.clear();
}

void LogAnalyzer::resetStatisticCollectors() {
    for (const auto& collector : collectors_) {
        if (collector) {
            collector->reset();
        }
    }
}

void LogAnalyzer::processEntriesForStatistics(std::span<const LogEntry> entries) {
    for (const auto& entry : entries) {
        processEntryForStatistics(entry);
    }
}

void LogAnalyzer::processEntriesForStatistics(std::generator<const LogEntry&> entries) {
    for (const auto& entry : entries) {
        processEntryForStatistics(entry);
    }
}

std::map<std::string, json> LogAnalyzer::getAllStatisticReports() const {
    std::map<std::string, json> reports;
    for (const auto& collector : collectors_) {
        reports[collector->getName()] = collector->generateReport();
    }
    return reports;
}

// Factory method for creating statistic collectors
std::shared_ptr<IStatisticCollector> LogAnalyzer::createStatisticCollector(const StatisticConfig& config) {
    return Statistics::createCollector(config);
}
