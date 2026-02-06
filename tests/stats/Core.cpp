// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "stats/Core.h"
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

    // Test creating a collector with missing required params
    StatisticConfig invalidConfig;
    invalidConfig.type = StatisticType::FIELD_VALUE_COUNT; // Missing target_field
    auto invalidCollector = Statistics::createCollector(invalidConfig);
    ASSERT_EQ(invalidCollector, nullptr);
}
