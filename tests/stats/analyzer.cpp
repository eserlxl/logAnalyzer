// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

// Focused tests for the statistic-collector lifecycle API on LogAnalyzer
// (src/stats/analyzer.cpp): add/remove/clear/reset and the generator overload
// of processEntriesForStatistics. These pin externally-observable behavior via
// getAllStatisticReports(); the existing analyzer tests only exercise the span
// overload and the add+remove-of-a-known-collector happy path.

#include <gtest/gtest.h>
#include "analyzer/core.h"
#include "stats/core.h"

#include <chrono>
#include <generator>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {

// Deterministic, timezone-independent timestamp (LogLevelCountCollector counts
// by level, so the exact value is irrelevant — it only needs to be fixed).
const auto kTs = std::chrono::system_clock::from_time_t(1700000000);

std::vector<LogEntry> sampleEntries() {
    return {
        {1, "test.log", 1, kTs, LogLevel::INFO, "one"},
        {2, "test.log", 2, kTs, LogLevel::ERROR, "two"},
        {3, "test.log", 3, kTs, LogLevel::INFO, "three"},
    };
}

std::generator<const LogEntry&> entryGenerator(std::vector<LogEntry> entries) {
    for (const auto& entry : entries) {
        co_yield entry;
    }
}

class StatisticsLifecycleTest : public ::testing::Test {
protected:
    LogAnalyzer analyzer;
};

TEST_F(StatisticsLifecycleTest, AddNullCollectorIsIgnored) {
    analyzer.addStatisticCollector(nullptr);
    EXPECT_TRUE(analyzer.getAllStatisticReports().empty());
}

TEST_F(StatisticsLifecycleTest, ClearStatisticCollectorsEmptiesReports) {
    analyzer.addStatisticCollector(std::make_shared<LogLevelCountCollector>());
    auto entries = sampleEntries();
    analyzer.processEntriesForStatistics(std::span<const LogEntry>(entries));
    ASSERT_FALSE(analyzer.getAllStatisticReports().empty());

    analyzer.clearStatisticCollectors();
    EXPECT_TRUE(analyzer.getAllStatisticReports().empty());
}

TEST_F(StatisticsLifecycleTest, RemoveNonExistentCollectorIsNoop) {
    auto kept = std::make_shared<LogLevelCountCollector>();
    analyzer.addStatisticCollector(kept);

    // A different instance that was never added; removal is identity-based.
    auto neverAdded = std::make_shared<LogLevelCountCollector>();
    analyzer.removeStatisticCollector(neverAdded);

    EXPECT_TRUE(analyzer.getAllStatisticReports().contains("log_level_count"));
}

TEST_F(StatisticsLifecycleTest, ResetStatisticCollectorsClearsAccumulationButKeepsCollector) {
    analyzer.addStatisticCollector(std::make_shared<LogLevelCountCollector>());
    auto entries = sampleEntries();
    analyzer.processEntriesForStatistics(std::span<const LogEntry>(entries));
    const auto withData = analyzer.getAllStatisticReports();

    // A pristine collector that has never seen an entry.
    LogAnalyzer fresh;
    fresh.addStatisticCollector(std::make_shared<LogLevelCountCollector>());
    const auto pristine = fresh.getAllStatisticReports();

    analyzer.resetStatisticCollectors();
    const auto afterReset = analyzer.getAllStatisticReports();

    EXPECT_NE(withData, afterReset);                       // accumulated data was cleared
    EXPECT_EQ(pristine, afterReset);                       // restored to a pristine state
    EXPECT_TRUE(afterReset.contains("log_level_count"));   // the collector itself is retained
}

TEST_F(StatisticsLifecycleTest, GeneratorOverloadMatchesSpanOverload) {
    const auto entries = sampleEntries();

    LogAnalyzer viaSpan;
    viaSpan.addStatisticCollector(std::make_shared<LogLevelCountCollector>());
    viaSpan.processEntriesForStatistics(std::span<const LogEntry>(entries));

    LogAnalyzer viaGenerator;
    viaGenerator.addStatisticCollector(std::make_shared<LogLevelCountCollector>());
    viaGenerator.processEntriesForStatistics(entryGenerator(entries));

    EXPECT_FALSE(viaGenerator.getAllStatisticReports().empty());
    EXPECT_EQ(viaSpan.getAllStatisticReports(), viaGenerator.getAllStatisticReports());
}

} // namespace
