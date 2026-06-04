// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "stats/core.h"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <chrono>

using namespace std::chrono_literals;
using json = nlohmann::json;



class StatisticsTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto now = std::chrono::system_clock::now();
        entries = std::vector<LogEntry>{
            {0, "main.cpp", 1, now, LogLevel::INFO, "Application starting", {}, {}, {}, {{"session", "A"}}, {}},
            {0, "main.cpp", 2, now + 1s, LogLevel::INFO, "Application starting", {}, {}, {}, {{"session", "B"}}, {}},
            {0, "worker.cpp", 3, now + 2s, LogLevel::DEBUG, "Processing data", {}, {}, {}, {{"session", "A"}}, {}},
            {0, "worker.cpp", 4, now + 3s, LogLevel::WARNING, "High load detected", {}, {}, {}, {{"session", "A"}}, {}},
            {0, "network.cpp", 5, now + 4s, LogLevel::ERROR, "Connection failed", {}, {}, {}, {{"session", "B"}}, {}},
            {0, "network.cpp", 6, now + 5s, LogLevel::ERROR, "Connection failed", {}, {}, {}, {{"session", "C"}}, {}},
            {0, "main.cpp", 7, now + 6s, LogLevel::INFO, "Application shutdown", {}, {}, {}, {{"session", "A"}}, {}},
        };
    }

    std::vector<LogEntry> entries;
};

TEST_F(StatisticsTest, LogLevelCountCollector) {
    LogLevelCountCollector collector;
    for (const auto& entry : entries) {
        collector.collect(entry);
    }
    json report = collector.generateReport();

    ASSERT_EQ(report["name"], "log_level_count");
    ASSERT_EQ(report["total_entries"], 7);
    ASSERT_TRUE(report.contains("counts"));
    EXPECT_EQ(report["counts"]["INFO"], 3);
    EXPECT_EQ(report["counts"]["DEBUG"], 1);
    EXPECT_EQ(report["counts"]["WARNING"], 1);
    EXPECT_EQ(report["counts"]["ERROR"], 2);
}

TEST_F(StatisticsTest, FieldValueCountCollectorForLevel) {
    FieldValueCountCollector collector("level");
    for (const auto& entry : entries) {
        collector.collect(entry);
    }
    json report = collector.generateReport();

    ASSERT_EQ(report["name"], "field_value_count_level");
    ASSERT_EQ(report["target_field"], "level");
    ASSERT_EQ(report["total_unique_values"], 4);
    ASSERT_TRUE(report.contains("counts"));
    EXPECT_EQ(report["counts"]["INFO"], 3);
    EXPECT_EQ(report["counts"]["DEBUG"], 1);
    EXPECT_EQ(report["counts"]["WARNING"], 1);
    EXPECT_EQ(report["counts"]["ERROR"], 2);
}

TEST_F(StatisticsTest, FieldValueCountCollectorForCustomField) {
    FieldValueCountCollector collector("customFields", "session");
    for (const auto& entry : entries) {
        collector.collect(entry);
    }
    json report = collector.generateReport();
    
    ASSERT_EQ(report["name"], "field_value_count_customFields_session");
    ASSERT_EQ(report["target_field"], "customFields");
    ASSERT_EQ(report["custom_field_key"], "session");
    ASSERT_EQ(report["total_unique_values"], 3);
    ASSERT_TRUE(report.contains("counts"));
    EXPECT_EQ(report["counts"]["A"], 4);
    EXPECT_EQ(report["counts"]["B"], 2);
    EXPECT_EQ(report["counts"]["C"], 1);
}

TEST_F(StatisticsTest, FieldValueCountCollectorSupportsSourceAliases) {
    FieldValueCountCollector collector("source_file");
    for (const auto& entry : entries) {
        collector.collect(entry);
    }
    json report = collector.generateReport();

    ASSERT_EQ(report["name"], "field_value_count_sourceFile");
    ASSERT_EQ(report["target_field"], "sourceFile");
    ASSERT_EQ(report["total_unique_values"], 3);
    EXPECT_EQ(report["counts"]["main.cpp"], 3);
    EXPECT_EQ(report["counts"]["worker.cpp"], 2);
    EXPECT_EQ(report["counts"]["network.cpp"], 2);
}

TEST_F(StatisticsTest, TopNFieldValuesCollectorSupportsThreadIdAlias) {
    entries[0].threadId = "t1";
    entries[1].threadId = "t2";
    entries[2].threadId = "t1";
    entries[3].threadId = "t3";
    entries[4].threadId = "t2";
    entries[5].threadId = "t2";
    entries[6].threadId = "t1";

    TopNFieldValuesCollector collector(2, "thread_id");
    for (const auto& entry : entries) {
        collector.collect(entry);
    }
    json report = collector.generateReport();
    ASSERT_EQ(report["target_field"], "threadId");
    ASSERT_EQ(report["values"].size(), 2);
}

TEST_F(StatisticsTest, TopNFieldValuesCollectorForMessage) {
    TopNFieldValuesCollector collector(2, "message");
    for (const auto& entry : entries) {
        collector.collect(entry);
    }
    json report = collector.generateReport();

    ASSERT_EQ(report["name"], "top_n_field_values_message");
    ASSERT_EQ(report["top_n"], 2);
    ASSERT_EQ(report["target_field"], "message");
    ASSERT_TRUE(report.contains("values"));
    
    json values = report["values"];
    ASSERT_EQ(values.size(), 2);

    // The top 2 messages are "Application starting" and "Connection failed" (both with count 2)
    // The order between them is not strictly guaranteed by the implementation, so we check for presence.
    bool foundAppStarting = false;
    bool foundConnFailed = false;
    for (const auto& item : values) {
        if (item["value"] == "Application starting") {
            EXPECT_EQ(item["count"], 2);
            foundAppStarting = true;
        } else if (item["value"] == "Connection failed") {
            EXPECT_EQ(item["count"], 2);
            foundConnFailed = true;
        }
    }
    EXPECT_TRUE(foundAppStarting);
    EXPECT_TRUE(foundConnFailed);
}

TEST_F(StatisticsTest, CreateCollectorFactory) {
    // Test creating a LogLevelCountCollector
    StatisticConfig logLevelConfig;
    logLevelConfig.type = StatisticType::LOG_LEVEL_COUNT;
    auto logLevelCollector = Statistics::createCollector(logLevelConfig);
    ASSERT_NE(logLevelCollector, nullptr);
    EXPECT_EQ(logLevelCollector->getName(), "log_level_count");

    // Test creating a FieldValueCountCollector for a custom field
    StatisticConfig fieldValueConfig;
    fieldValueConfig.type = StatisticType::FIELD_VALUE_COUNT;
    fieldValueConfig.params["target_field"] = "customFields";
    fieldValueConfig.params["custom_field_key"] = "session";
    auto fieldValueCollector = Statistics::createCollector(fieldValueConfig);
    ASSERT_NE(fieldValueCollector, nullptr);
    EXPECT_EQ(fieldValueCollector->getName(), "field_value_count_customFields_session");

    // Test creating a TopNFieldValuesCollector
    StatisticConfig topNConfig;
    topNConfig.type = StatisticType::TOP_N_FIELD_VALUES;
    topNConfig.params["target_field"] = "message";
    topNConfig.params["top_n"] = "5";
    auto topNCollector = Statistics::createCollector(topNConfig);
    ASSERT_NE(topNCollector, nullptr);
    EXPECT_EQ(topNCollector->getName(), "top_n_field_values_message");

    StatisticConfig aliasConfig;
    aliasConfig.type = StatisticType::FIELD_VALUE_COUNT;
    aliasConfig.params["target_field"] = "SOURCE_FILE";
    auto aliasCollector = Statistics::createCollector(aliasConfig);
    ASSERT_NE(aliasCollector, nullptr);
    EXPECT_EQ(aliasCollector->getName(), "field_value_count_sourceFile");

    // Test creating a collector with missing required params
    StatisticConfig invalidConfig;
    invalidConfig.type = StatisticType::FIELD_VALUE_COUNT; // Missing target_field
    auto invalidCollector = Statistics::createCollector(invalidConfig);
    ASSERT_EQ(invalidCollector, nullptr);
}

TEST_F(StatisticsTest, CreateCollectorFactoryRejectsMalformedTopNForTopNFieldValues) {
    StatisticConfig invalidConfig;
    invalidConfig.type = StatisticType::TOP_N_FIELD_VALUES;
    invalidConfig.params["target_field"] = "message";
    invalidConfig.params["top_n"] = "5abc";
    auto invalidCollector = Statistics::createCollector(invalidConfig);
    ASSERT_EQ(invalidCollector, nullptr);
}

TEST_F(StatisticsTest, CreateCollectorFactoryRejectsWhitespaceOnlyCustomFieldKey) {
    StatisticConfig invalidConfig;
    invalidConfig.type = StatisticType::FIELD_VALUE_COUNT;
    invalidConfig.params["target_field"] = "custom_fields";
    invalidConfig.params["custom_field_key"] = "   ";
    auto invalidCollector = Statistics::createCollector(invalidConfig);
    ASSERT_EQ(invalidCollector, nullptr);
}

TEST_F(StatisticsTest, TimeBucketHistogramCollector) {
    // 6 entries spanning 3 one-minute buckets: t, t+61s, t+61s, t+122s, t+122s, t+122s
    auto base = std::chrono::system_clock::now();
    // Align base to a minute boundary to avoid bucket edge ambiguity
    auto epoch_s = std::chrono::duration_cast<std::chrono::seconds>(base.time_since_epoch()).count();
    long long aligned = (epoch_s / 60) * 60;
    auto t0 = std::chrono::system_clock::time_point(std::chrono::seconds(aligned));

    std::vector<LogEntry> histEntries = {
        {0, "", 1, t0,             LogLevel::INFO, "a", {}, {}, {}, {}, {}},
        {0, "", 2, t0 + 61s,       LogLevel::INFO, "b", {}, {}, {}, {}, {}},
        {0, "", 3, t0 + 61s,       LogLevel::INFO, "c", {}, {}, {}, {}, {}},
        {0, "", 4, t0 + 122s,      LogLevel::INFO, "d", {}, {}, {}, {}, {}},
        {0, "", 5, t0 + 122s,      LogLevel::INFO, "e", {}, {}, {}, {}, {}},
        {0, "", 6, t0 + 122s,      LogLevel::INFO, "f", {}, {}, {}, {}, {}},
    };
    // Entry without timestamp — must be silently skipped
    LogEntry noTs{0, "", 7, std::nullopt, LogLevel::DEBUG, "no-ts", {}, {}, {}, {}, {}};

    TimeBucketHistogramCollector collector(60);
    for (const auto& e : histEntries) collector.collect(e);
    collector.collect(noTs);

    json report = collector.generateReport();
    ASSERT_EQ(report["name"], "time_bucket_histogram");
    ASSERT_EQ(report["bucket_seconds"], 60);
    ASSERT_EQ(report["buckets"].size(), 3u);

    // Buckets must be sorted ascending
    long long prev = -1;
    for (const auto& b : report["buckets"]) {
        long long start = b["start_time"].get<long long>();
        ASSERT_GT(start, prev);
        prev = start;
    }

    // Counts: bucket0=1, bucket1=2, bucket2=3
    ASSERT_EQ(report["buckets"][0]["count"], 1);
    ASSERT_EQ(report["buckets"][1]["count"], 2);
    ASSERT_EQ(report["buckets"][2]["count"], 3);
}

TEST_F(StatisticsTest, TimeBucketHistogramRoundTrip) {
    ASSERT_EQ(Utils::statisticTypeToString(StatisticType::TIME_BUCKET_HISTOGRAM), "TIME_BUCKET_HISTOGRAM");
    auto opt = Utils::stringToStatisticType("TIME_BUCKET_HISTOGRAM");
    ASSERT_TRUE(opt.has_value());
    ASSERT_EQ(*opt, StatisticType::TIME_BUCKET_HISTOGRAM);
}

TEST_F(StatisticsTest, CreateCollectorFactoryDefaultsMalformedTopNForTopMessages) {
    StatisticConfig config;
    config.type = StatisticType::TOP_MESSAGES;
    config.params["top_n"] = "7abc";
    auto collector = Statistics::createCollector(config);
    ASSERT_NE(collector, nullptr);
    for (const auto& entry : entries) {
        collector->collect(entry);
    }
    const json report = collector->generateReport();
    ASSERT_EQ(report["name"], "top_messages");
    ASSERT_EQ(report["top_n"], 10);
}

TEST_F(StatisticsTest, CreateCollectorFactoryTimeBucketHistogramDefault) {
    StatisticConfig config;
    config.type = StatisticType::TIME_BUCKET_HISTOGRAM;
    auto collector = Statistics::createCollector(config);
    ASSERT_NE(collector, nullptr);
    ASSERT_EQ(collector->getName(), "time_bucket_histogram");
    const json report = collector->generateReport();
    ASSERT_EQ(report["bucket_seconds"], 60);
    ASSERT_TRUE(report["buckets"].is_array());
    ASSERT_TRUE(report["buckets"].empty());
}

TEST_F(StatisticsTest, CreateCollectorFactoryTimeBucketHistogramBucketParam) {
    StatisticConfig config;
    config.type = StatisticType::TIME_BUCKET_HISTOGRAM;
    config.params["bucket"] = "30";
    auto collector = Statistics::createCollector(config);
    ASSERT_NE(collector, nullptr);
    ASSERT_EQ(collector->getName(), "time_bucket_histogram");

    LogEntry e;
    auto tp = std::chrono::system_clock::from_time_t(1000);
    e.timestamp = tp;
    e.message = "test";
    collector->collect(e);

    const json report = collector->generateReport();
    ASSERT_EQ(report["bucket_seconds"], 30);
    ASSERT_EQ(report["buckets"].size(), 1u);
}

TEST_F(StatisticsTest, CreateCollectorFactoryTimeBucketHistogramInvalidBucketFallsBackToDefault) {
    StatisticConfig config;
    config.type = StatisticType::TIME_BUCKET_HISTOGRAM;
    config.params["bucket"] = "0";
    auto collector = Statistics::createCollector(config);
    ASSERT_NE(collector, nullptr);
    const json report = collector->generateReport();
    ASSERT_EQ(report["bucket_seconds"], 60);
}

TEST_F(StatisticsTest, PercentileStatsCollectorBasic) {
    PercentileStatsCollector collector("latency_ms");
    // Collect 5 numeric values: 10, 20, 30, 40, 50
    for (double v : {10.0, 20.0, 30.0, 40.0, 50.0}) {
        LogEntry e;
        e.message = "test";
        e.customFields["latency_ms"] = std::to_string(v);
        collector.collect(e);
    }
    const json report = collector.generateReport();
    ASSERT_EQ(report["name"], "percentile_stats");
    ASSERT_EQ(report["field"], "latency_ms");
    ASSERT_EQ(report["count"], 5u);
    // p50 of [10,20,30,40,50] = 30
    ASSERT_DOUBLE_EQ(report["p50"].get<double>(), 30.0);
    // p95: idx=3.8 → 40 + 0.8*(50-40) = 48.0
    ASSERT_DOUBLE_EQ(report["p95"].get<double>(), 48.0);
    // p99: idx=3.96 → 40 + 0.96*(50-40) = 49.6
    ASSERT_DOUBLE_EQ(report["p99"].get<double>(), 49.6);
}

TEST_F(StatisticsTest, PercentileStatsCollectorSkipsNonNumericAndAbsent) {
    PercentileStatsCollector collector("latency_ms");
    LogEntry good;
    good.message = "ok";
    good.customFields["latency_ms"] = "100";
    collector.collect(good);

    LogEntry bad;
    bad.message = "bad";
    bad.customFields["latency_ms"] = "not_a_number";
    collector.collect(bad);

    LogEntry absent;
    absent.message = "absent";
    collector.collect(absent);

    const json report = collector.generateReport();
    ASSERT_EQ(report["count"], 1u);
    ASSERT_DOUBLE_EQ(report["p50"].get<double>(), 100.0);
}

TEST_F(StatisticsTest, PercentileStatsCollectorEmptyReturnsNullPercentiles) {
    PercentileStatsCollector collector("latency_ms");
    const json report = collector.generateReport();
    ASSERT_EQ(report["count"], 0u);
    ASSERT_TRUE(report["p50"].is_null());
    ASSERT_TRUE(report["p95"].is_null());
    ASSERT_TRUE(report["p99"].is_null());
}

TEST_F(StatisticsTest, PercentileStatsRoundTrip) {
    ASSERT_EQ(Utils::statisticTypeToString(StatisticType::PERCENTILE_STATS), "PERCENTILE_STATS");
    auto opt = Utils::stringToStatisticType("PERCENTILE_STATS");
    ASSERT_TRUE(opt.has_value());
    ASSERT_EQ(*opt, StatisticType::PERCENTILE_STATS);
}

TEST_F(StatisticsTest, CreateCollectorFactoryPercentileStats) {
    StatisticConfig config;
    config.type = StatisticType::PERCENTILE_STATS;
    config.params["field"] = "latency_ms";
    auto collector = Statistics::createCollector(config);
    ASSERT_NE(collector, nullptr);
    ASSERT_EQ(collector->getName(), "percentile_stats");
}

TEST_F(StatisticsTest, CreateCollectorFactoryPercentileStatsMissingField) {
    StatisticConfig config;
    config.type = StatisticType::PERCENTILE_STATS;
    auto collector = Statistics::createCollector(config);
    ASSERT_EQ(collector, nullptr);
}

TEST_F(StatisticsTest, PercentileStatsCollectorSingleValue) {
    PercentileStatsCollector collector("latency_ms");
    LogEntry e;
    e.message = "test";
    e.customFields["latency_ms"] = "42.0";
    collector.collect(e);
    const json report = collector.generateReport();
    ASSERT_EQ(report["count"], 1u);
    ASSERT_DOUBLE_EQ(report["p50"].get<double>(), 42.0);
    ASSERT_DOUBLE_EQ(report["p95"].get<double>(), 42.0);
    ASSERT_DOUBLE_EQ(report["p99"].get<double>(), 42.0);
}

TEST_F(StatisticsTest, PercentileStatsCollectorTwoValues) {
    PercentileStatsCollector collector("latency_ms");
    for (const char* v : {"10.0", "20.0"}) {
        LogEntry e;
        e.message = "test";
        e.customFields["latency_ms"] = v;
        collector.collect(e);
    }
    const json report = collector.generateReport();
    ASSERT_EQ(report["count"], 2u);
    // p50: idx=0.5*1=0.5, lo=0, frac=0.5 → 10+0.5*10=15
    ASSERT_DOUBLE_EQ(report["p50"].get<double>(), 15.0);
    // p99: idx=0.99*1=0.99, lo=0, frac=0.99 → 10+0.99*10=19.9
    ASSERT_DOUBLE_EQ(report["p99"].get<double>(), 19.9);
}

TEST_F(StatisticsTest, PercentileStatsCollectorReset) {
    PercentileStatsCollector collector("latency_ms");
    LogEntry e1;
    e1.message = "first";
    e1.customFields["latency_ms"] = "100.0";
    collector.collect(e1);
    collector.reset();

    LogEntry e2;
    e2.message = "second";
    e2.customFields["latency_ms"] = "7.0";
    collector.collect(e2);

    const json report = collector.generateReport();
    ASSERT_EQ(report["count"], 1u);
    ASSERT_DOUBLE_EQ(report["p50"].get<double>(), 7.0);
}
