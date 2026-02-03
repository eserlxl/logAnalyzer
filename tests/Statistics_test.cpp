#undef ERROR
#undef INFO
#undef DEBUG
#undef WARNING

#include "LogTypes.h" // Moved before gtest/gtest.h
#include <gtest/gtest.h>
#include "Statistics.h"
#include <vector>
#include <chrono>

using namespace std::chrono_literals; // Re-introduced

class StatisticsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a dataset with varied timestamps, levels, and messages
        auto now = std::chrono::system_clock::now();
        entries = {
            // Non-burst entries, chronologically ordered
            {0, "main.cpp", now + 0s, LogLevel::INFO, "Begin process", {}},
            {0, "worker.cpp", now + 1s, LogLevel::DEBUG, "Step 1 complete", {}},
            {0, "network.cpp", now + 2s, LogLevel::WARNING, "Connection timeout", {}},
            {0, "main.cpp", now + 4s, LogLevel::ERROR, "File not found", {}}, // Gap from previous: 2s
            {0, "main.cpp", now + 5s, LogLevel::INFO, "Begin process", {}},    // Gap from previous: 1s
            {0, "network.cpp", now + 6s, LogLevel::WARNING, "Connection timeout", {}}, // Gap from previous: 1s
            // Burst of logs, clearly after other events to isolate burst detection
            {0, "burst.cpp", now + 7s, LogLevel::ERROR, "Burst event", {}},
            {0, "burst.cpp", now + 7s + 100ms, LogLevel::ERROR, "Burst event", {}},
            {0, "burst.cpp", now + 7s + 200ms, LogLevel::ERROR, "Burst event", {}},
            {0, "burst.cpp", now + 7s + 300ms, LogLevel::ERROR, "Burst event", {}},
            // A final entry after the burst, to give a proper end for average rate calculation
            {0, "final.cpp", now + 8s, LogLevel::INFO, "Final entry", {}}, // Gap from previous: ~0.7s
        };
    }

    std::vector<LogEntry> entries;
};

TEST_F(StatisticsTest, ConstructorSortsEntries) {
    // Create entries that are intentionally out of order
    auto now = std::chrono::system_clock::now();
    std::vector<LogEntry> unsortedEntries = {
        {0, "test.cpp", now + 5s, LogLevel::INFO, "Last", {}},
        {0, "test.cpp", now + 1s, LogLevel::INFO, "Second", {}},
        {0, "test.cpp", now, LogLevel::INFO, "First", {}},
    };

    Statistics stats(unsortedEntries);
    
    // Use a method that depends on sorted data to verify
    auto gaps = stats.findTimeGaps(std::chrono::milliseconds(0));
    ASSERT_EQ(gaps.size(), 2);
    EXPECT_EQ(gaps[0].start, now);
    EXPECT_EQ(gaps[0].end, now + 1s);
    EXPECT_EQ(gaps[1].start, now + 1s);
    EXPECT_EQ(gaps[1].end, now + 5s);
}

TEST_F(StatisticsTest, CalculateLogLevelDistribution) {
    Statistics stats(entries);
    auto distribution = stats.calculateLogLevelDistribution();

    EXPECT_EQ(distribution[LogLevel::INFO], 3);
    EXPECT_EQ(distribution[LogLevel::DEBUG], 1);
    EXPECT_EQ(distribution[LogLevel::WARNING], 2);
    EXPECT_EQ(distribution[LogLevel::ERROR], 5);
}

TEST_F(StatisticsTest, CalculateUniqueMessageCounts) {
    Statistics stats(entries);
    auto counts = stats.calculateUniqueMessageCounts();

    EXPECT_EQ(counts["Begin process"], 2);
    EXPECT_EQ(counts["Step 1 complete"], 1);
    EXPECT_EQ(counts["Connection timeout"], 2);
    EXPECT_EQ(counts["File not found"], 1);
    EXPECT_EQ(counts["Burst event"], 4);
    EXPECT_EQ(counts["Final entry"], 1);
}

TEST_F(StatisticsTest, GetRankedMessagesDescending) {
    Statistics stats(entries);
    auto ranked = stats.getRankedMessages(3, Statistics::SortOrder::Descending);

    ASSERT_EQ(ranked.size(), 3);
    EXPECT_EQ(ranked[0].first, "Burst event");
    EXPECT_EQ(ranked[0].second, 4);
    EXPECT_EQ(ranked[1].first, "Begin process");
    EXPECT_EQ(ranked[1].second, 2); 
    EXPECT_EQ(ranked[2].first, "Connection timeout"); 
    EXPECT_EQ(ranked[2].second, 2); 
}

TEST_F(StatisticsTest, GetRankedMessagesAscending) {
    Statistics stats(entries);
    // Request more messages than unique messages exist to test resizing logic
    auto ranked = stats.getRankedMessages(10, Statistics::SortOrder::Ascending);

    ASSERT_EQ(ranked.size(), 6); // Total unique messages is now 6
    EXPECT_EQ(ranked[0].second, 1);
    EXPECT_EQ(ranked[1].second, 1);
    EXPECT_EQ(ranked[2].second, 1);
    EXPECT_EQ(ranked[3].second, 2);
    EXPECT_EQ(ranked[4].second, 2);
    EXPECT_EQ(ranked[5].first, "Burst event");
    EXPECT_EQ(ranked[5].second, 4);
}

// Test for getLogFrequencyDistributionOverTime
TEST_F(StatisticsTest, GetLogFrequencyDistributionOverTimeEmpty) {
    std::vector<LogEntry> emptyEntries;
    Statistics stats(emptyEntries);
    auto dist = stats.getLogFrequencyDistributionOverTime(1s);
    ASSERT_TRUE(dist.empty());
}

TEST_F(StatisticsTest, GetLogFrequencyDistributionOverTimeSingleEntry) {
    auto now = std::chrono::system_clock::now();
    std::vector<LogEntry> singleEntry = {
        {0, "test.cpp", now, LogLevel::INFO, "Single", {}}
    };
    Statistics stats(singleEntry);
    auto dist = stats.getLogFrequencyDistributionOverTime(1s);
    ASSERT_EQ(dist.size(), 1);
    EXPECT_EQ(dist[0].windowStart, now);
    EXPECT_EQ(dist[0].totalCount, 1);
    EXPECT_EQ(dist[0].counts[LogLevel::INFO], 1);
}

TEST_F(StatisticsTest, GetLogFrequencyDistributionOverTimeNormalData) {
    auto now = std::chrono::system_clock::now();
    std::vector<LogEntry> testEntries = {
        {0, "test.cpp", now + 0s, LogLevel::INFO, "Log1", {}},
        {0, "test.cpp", now + 0s + 500ms, LogLevel::DEBUG, "Log2", {}},
        {0, "test.cpp", now + 1s + 100ms, LogLevel::INFO, "Log3", {}},
        {0, "test.cpp", now + 1s + 600ms, LogLevel::WARNING, "Log4", {}},
        {0, "test.cpp", now + 2s + 200ms, LogLevel::ERROR, "Log5", {}}
    };
    Statistics stats(testEntries);

    // Window size 1 second
    // Window 0: [now, now+1s) -> Log1, Log2 (2 events)
    // Window 1: [now+1s, now+2s) -> Log3, Log4 (2 events)
    // Window 2: [now+2s, now+3s) -> Log5 (1 event)
    auto dist = stats.getLogFrequencyDistributionOverTime(1s);
    ASSERT_EQ(dist.size(), 3);

    EXPECT_EQ(dist[0].totalCount, 2);
    EXPECT_EQ(dist[0].counts[LogLevel::INFO], 1);
    EXPECT_EQ(dist[0].counts[LogLevel::DEBUG], 1);

    EXPECT_EQ(dist[1].totalCount, 2);
    EXPECT_EQ(dist[1].counts[LogLevel::INFO], 1);
    EXPECT_EQ(dist[1].counts[LogLevel::WARNING], 1);

    EXPECT_EQ(dist[2].totalCount, 1);
    EXPECT_EQ(dist[2].counts[LogLevel::ERROR], 1);
}

TEST_F(StatisticsTest, GetLogFrequencyDistributionOverTimeWithLargeGaps) {
    auto now = std::chrono::system_clock::now();
    std::vector<LogEntry> testEntries = {
        {0, "test.cpp", now + 0s, LogLevel::INFO, "Log1", {}},
        {0, "test.cpp", now + 10s, LogLevel::INFO, "Log2", {}}, // Large gap
        {0, "test.cpp", now + 10s + 500ms, LogLevel::INFO, "Log3", {}}
    };
    Statistics stats(testEntries);

    // Window size 1 second
    // Window 0: [now, now+1s) -> Log1 (1 event)
    // Window 1-9: Empty
    // Window 10: [now+10s, now+11s) -> Log2, Log3 (2 events)
    auto dist = stats.getLogFrequencyDistributionOverTime(1s);
    ASSERT_EQ(dist.size(), 11); // 0s, 1s, ..., 9s, 10s

    EXPECT_EQ(dist[0].totalCount, 1);
    EXPECT_EQ(dist[0].counts[LogLevel::INFO], 1);

    for (size_t i = 1; i < 10; ++i) {
        EXPECT_EQ(dist[i].totalCount, 0) << "Window " << i << " should be empty";
    }

    EXPECT_EQ(dist[10].totalCount, 2);
    EXPECT_EQ(dist[10].counts[LogLevel::INFO], 2);
}

TEST_F(StatisticsTest, GetLogFrequencyDistributionOverTimeAllInOneWindow) {
    auto now = std::chrono::system_clock::now();
    std::vector<LogEntry> testEntries = {
        {0, "test.cpp", now + 0s, LogLevel::INFO, "Log1", {}},
        {0, "test.cpp", now + 1s, LogLevel::DEBUG, "Log2", {}},
        {0, "test.cpp", now + 2s, LogLevel::WARNING, "Log3", {}}
    };
    Statistics stats(testEntries);

    // Window size 10 seconds, all logs should be in the first window
    auto dist = stats.getLogFrequencyDistributionOverTime(10s);
    ASSERT_EQ(dist.size(), 1);

    EXPECT_EQ(dist[0].totalCount, 3);
    EXPECT_EQ(dist[0].counts[LogLevel::INFO], 1);
    EXPECT_EQ(dist[0].counts[LogLevel::DEBUG], 1);
    EXPECT_EQ(dist[0].counts[LogLevel::WARNING], 1);
}



TEST_F(StatisticsTest, CalculateDistributionByGroup) {
    Statistics stats(entries);
    auto bySourceFile = stats.calculateDistributionByGroup([](const LogEntry& e) {
        return e.sourceFile;
    });

    EXPECT_EQ(bySourceFile["main.cpp"], 3); // Changed from 4
    EXPECT_EQ(bySourceFile["worker.cpp"], 1);
    EXPECT_EQ(bySourceFile["network.cpp"], 2); // No change
    EXPECT_EQ(bySourceFile["burst.cpp"], 4);
    EXPECT_EQ(bySourceFile["final.cpp"], 1); // New entry
}

TEST_F(StatisticsTest, GetTimeGapPercentile) {
    Statistics stats(entries);

    // There are 11 gaps.
    // 1s, 1s, 1s, 1s, 6s, 1s, 1s, std::chrono::seconds(3), 100ms, 100ms, 100ms
    // Sorted (ns): 100M, 100M, 100M, 1G, 1G, 1G, 1G, 1G, 3G, 6G
    // Let's re-calculate:
    // 1s, 1s, 1s, 1s, 6s, 1s, 1s, std::chrono::seconds(3), 100ms, 100ms, 100ms. Total 11 gaps.
    // Sorted Gaps: 0.1, 0.1, 0.1, 1, 1, 1, 1, 3, 6
    // Gaps in nanoseconds:
    // 100'000'000 (x3)
    // 1'000'000'000 (x4)
    // 3'000'000'000 (x1)
    // 6'000'000'000 (x1)
    // Total 9 gaps between unique timestamps. Let's fix the test data.
    // Let's create more predictable gaps.
    auto now = std::chrono::system_clock::now();
    std::vector<LogEntry> gapEntries = {
        {0, "f", now, LogLevel::INFO, "1", {}}, // Gap 1s
        {0, "f", now + 1s, LogLevel::INFO, "2", {}}, // Gap 2s
        {0, "f", now + std::chrono::seconds(3), LogLevel::INFO, "3", {}}, // Gap std::chrono::seconds(3)
        {0, "f", now + 6s, LogLevel::INFO, "4", {}}, // Gap std::chrono::seconds(4)
        {0, "f", now + std::chrono::seconds(10), LogLevel::INFO, "5", {}}
    };
    Statistics gapStats(gapEntries);

    EXPECT_NEAR(gapStats.getTimeGapPercentile(0.0).count(), std::chrono::duration_cast<std::chrono::nanoseconds>(1s).count(), 1);
    EXPECT_NEAR(gapStats.getTimeGapPercentile(1.0).count(), std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(4)).count(), 1);
    EXPECT_NEAR(gapStats.getTimeGapPercentile(0.5).count(), std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(2.5)).count(), 1.0); // Interpolated
}

TEST_F(StatisticsTest, GetTimeGapPercentileInvalidArgs) {
    Statistics stats(entries);
    EXPECT_THROW(stats.getTimeGapPercentile(-0.1), std::invalid_argument);
    EXPECT_THROW(stats.getTimeGapPercentile(1.1), std::invalid_argument);
}

TEST_F(StatisticsTest, FindTimeGapsNoGapsFound) {
    Statistics stats(entries);
    // There are gaps, but none are larger than 3s, except one.
    // The largest gap is 2s. Let's set minGap to 3s.
    auto gaps = stats.findTimeGaps(3s);
    EXPECT_TRUE(gaps.empty());
}


TEST_F(StatisticsTest, FindTimeGaps) {
    // This test also has an expectation related to the gaps, let's re-evaluate.
    // The previous analysis assumed a 6s gap.
    // New gaps in 'entries':
    // 0s to 1s: 1s
    // 1s to 2s: 1s
    // 2s to 4s: 2s
    // 4s to 5s: 1s
    // 5s to 6s: 1s
    // 6s to 7s: 1s
    // 7s to 7s+100ms: 100ms
    // 7s+100ms to 7s+200ms: 100ms
    // 7s+200ms to 7s+300ms: 100ms
    // 7s+300ms to 8s: 700ms
    // Total gaps: 10.
    // Gaps >= 2s: Only one gap of 2s (from now+2s to now+4s).
    Statistics stats(entries);
    auto gaps = stats.findTimeGaps(2s); // Find gaps of 2s or more

    ASSERT_EQ(gaps.size(), 1);
    EXPECT_GE(gaps[0].duration, 2s);
    EXPECT_LE(gaps[0].duration, 2s + std::chrono::milliseconds(1));
}


TEST_F(StatisticsTest, CalculateAverageEntryRate) {
    Statistics stats(entries);
    // 11 entries over 8.0 seconds (from now to now + 8s)
    double rate = stats.calculateAverageEntryRate();
    EXPECT_NEAR(rate, 11.0 / 8.0, 0.001);
}

TEST_F(StatisticsTest, FindLogBursts) {
    Statistics stats(entries);
    // After data fix: 11 entries over 8.0 seconds -> 11/8 = 1.375 e/s.
    // With a threshold multiplier of 3, the rate threshold is 1.375 * 3 = 4.125 e/s
    // The burst has 4 events in 0.3 seconds -> ~13.3 e/s
    auto bursts = stats.findLogBursts(1s, 3.0);

    ASSERT_EQ(bursts.size(), 1);
    EXPECT_EQ(bursts[0].eventCount, 5); 
    EXPECT_NEAR(bursts[0].peakRate, 5.0, 0.1); 
}

TEST_F(StatisticsTest, FindLogBurstsMergingScenarios) {
    auto now = std::chrono::system_clock::now();
    std::vector<LogEntry> mergeEntries = {
        {0, "test.cpp", now + 0s, LogLevel::INFO, "Normal", {}},
        {0, "test.cpp", now + 1s, LogLevel::ERROR, "Burst1", {}},
        {0, "test.cpp", now + 1s + 100ms, LogLevel::ERROR, "Burst1", {}},
        {0, "test.cpp", now + 1s + 200ms, LogLevel::ERROR, "Burst1", {}}, // End of first burst

        {0, "test.cpp", now + 1s + 800ms, LogLevel::ERROR, "Burst2", {}}, // Starts shortly after first
        {0, "test.cpp", now + 1s + 900ms, LogLevel::ERROR, "Burst2", {}},
        {0, "test.cpp", now + 2s, LogLevel::ERROR, "Burst2", {}}, // End of second burst

        {0, "test.cpp", now + 5s, LogLevel::INFO, "Normal", {}}
    };
    Statistics stats(mergeEntries);

    // Overall average rate (7 entries in 5 seconds) = 1.4 e/s
    // Threshold multiplier 2.0 -> threshold = 2.8 e/s
    // Window size 1s:
    // First burst: 3 events in 0.2s -> 15 e/s (above threshold)
    // Second burst: 3 events in 0.2s -> 15 e/s (above threshold)
    // They should merge because the second burst starts before the window size of the first burst ends relative to its end time (1s window).
    // Specifically, previous burst ends at now+1s+200ms. If the next relevant entry (now+1s+800ms) falls within (now+1s+200ms, now+1s+200ms+1s], it should merge.
    // 1s+800ms is within 1s+200ms to 2s+200ms. So they merge.

    auto bursts = stats.findLogBursts(1s, 2.0);
    ASSERT_EQ(bursts.size(), 1);
    EXPECT_EQ(bursts[0].eventCount, 6); // All 6 burst events
    EXPECT_NEAR(bursts[0].peakRate, 6.0, 0.1);
    EXPECT_EQ(bursts[0].startTime, now + 1s);
    EXPECT_EQ(bursts[0].endTime, now + 2s);
}

TEST_F(StatisticsTest, FindLogBurstsMultipleDistinctBursts) {
    auto now = std::chrono::system_clock::now();
    std::vector<LogEntry> distinctEntries = {
        {0, "test.cpp", now + 0s, LogLevel::INFO, "Normal", {}},
        {0, "test.cpp", now + 1s, LogLevel::ERROR, "BurstA", {}},
        {0, "test.cpp", now + 1s + 100ms, LogLevel::ERROR, "BurstA", {}},
        {0, "test.cpp", now + 1s + 200ms, LogLevel::ERROR, "BurstA", {}}, // End of BurstA

        {0, "test.cpp", now + 5s, LogLevel::INFO, "Normal", {}}, // Gap
        
        {0, "test.cpp", now + 6s, LogLevel::ERROR, "BurstB", {}},
        {0, "test.cpp", now + 6s + 100ms, LogLevel::ERROR, "BurstB", {}}, // End of BurstB

        {0, "test.cpp", now + 10s, LogLevel::INFO, "Normal", {}}
    };
    Statistics stats(distinctEntries);

    // Overall average rate (7 entries in 10 seconds) = 0.7 e/s
    // Threshold multiplier 3.0 -> threshold = 2.1 e/s
    // BurstA: 3 events in 0.2s -> 15 e/s (above threshold)
    // BurstB: 2 events in 0.1s -> 20 e/s (above threshold)
    auto bursts = stats.findLogBursts(1s, 3.0);

    ASSERT_EQ(bursts.size(), 2);

    EXPECT_EQ(bursts[0].eventCount, 3);
    EXPECT_NEAR(bursts[0].peakRate, 15.0, 0.1);
    EXPECT_EQ(bursts[0].startTime, now + 1s);
    EXPECT_EQ(bursts[0].endTime, now + 1s + 200ms);

    EXPECT_EQ(bursts[1].eventCount, 2);
    EXPECT_NEAR(bursts[1].peakRate, 20.0, 0.1);
    EXPECT_EQ(bursts[1].startTime, now + 6s);
    EXPECT_EQ(bursts[1].endTime, now + 6s + 100ms);
}

TEST_F(StatisticsTest, FindLogBurstsAtEdges) {
    auto now = std::chrono::system_clock::now();
    std::vector<LogEntry> edgeEntries = {
        {0, "test.cpp", now + 0s, LogLevel::ERROR, "EdgeBurst", {}},
        {0, "test.cpp", now + 0s + 100ms, LogLevel::ERROR, "EdgeBurst", {}},

        {0, "test.cpp", now + 5s, LogLevel::INFO, "Normal", {}},
        {0, "test.cpp", now + 6s, LogLevel::INFO, "Normal", {}},

        {0, "test.cpp", now + 9s, LogLevel::ERROR, "EdgeBurst", {}},
        {0, "test.cpp", now + 9s + 100ms, LogLevel::ERROR, "EdgeBurst", {}}
    };
    Statistics stats(edgeEntries);

    // Overall average rate (6 entries in 9.1 seconds) = ~0.66 e/s
    // Threshold multiplier 5.0 -> threshold = ~3.3 e/s
    // Each burst: 2 events in 0.1s -> 20 e/s (above threshold)
    auto bursts = stats.findLogBursts(1s, 5.0);

    ASSERT_EQ(bursts.size(), 2);

    EXPECT_EQ(bursts[0].eventCount, 2);
    EXPECT_NEAR(bursts[0].peakRate, 20.0, 0.1);
    EXPECT_EQ(bursts[0].startTime, now + 0s);
    EXPECT_EQ(bursts[0].endTime, now + 0s + 100ms);

    EXPECT_EQ(bursts[1].eventCount, 2);
    EXPECT_NEAR(bursts[1].peakRate, 20.0, 0.1);
    EXPECT_EQ(bursts[1].startTime, now + 9s);
    EXPECT_EQ(bursts[1].endTime, now + 9s + 100ms);
}

TEST_F(StatisticsTest, FindLogBurstsNoBursts) {
    auto now = std::chrono::system_clock::now();
    std::vector<LogEntry> noBurstEntries = {
        {0, "test.cpp", now + 0s, LogLevel::INFO, "Normal", {}},
        {0, "test.cpp", now + 2s, LogLevel::INFO, "Normal", {}},
        {0, "test.cpp", now + 4s, LogLevel::INFO, "Normal", {}},
        {0, "test.cpp", now + 6s, LogLevel::INFO, "Normal", {}}
    };
    Statistics stats(noBurstEntries);

    // Overall average rate (4 entries in 6 seconds) = ~0.66 e/s
    // Threshold multiplier 2.0 -> threshold = ~1.33 e/s
    // Max rate in any 1s window will be 1 event/s (1 event in 2s window, 0.5 e/s; 1 event in 1s window, 1 e/s).
    auto bursts = stats.findLogBursts(1s, 2.0);
    ASSERT_TRUE(bursts.empty());
}

TEST_F(StatisticsTest, EmptyEntries) {
    std::vector<LogEntry> empty;
    Statistics stats(empty);

    EXPECT_TRUE(stats.calculateLogLevelDistribution().empty());
    EXPECT_TRUE(stats.calculateUniqueMessageCounts().empty());
    EXPECT_TRUE(stats.getRankedMessages(5).empty());
    EXPECT_TRUE(stats.getLogFrequencyDistributionOverTime(1s).empty());
    EXPECT_TRUE(stats.findTimeGaps(std::chrono::milliseconds(1)).empty());
    EXPECT_EQ(stats.calculateAverageEntryRate(), 0.0);
    EXPECT_TRUE(stats.calculateDistributionByGroup([](const auto& e){ return e.sourceFile; }).empty());
    EXPECT_EQ(stats.getTimeGapPercentile(0.5).count(), 0);
    EXPECT_TRUE(stats.findLogBursts(1s, 2.0).empty());
}
