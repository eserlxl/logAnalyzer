// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "analyzer/core.h"
#include <fstream>
#include <filesystem>
#include <future>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>

using namespace filter;

class LogAnalyzerExportTest : public ::testing::Test {
protected:
    LogAnalyzer analyzer;
};

TEST_F(LogAnalyzerExportTest, ExportAsCsvEdgeCases) {
    std::stringstream ss;
    FilterExpression expression;
    (void)analyzer.exportAsCsv(ss, expression, true);
    ASSERT_FALSE(ss.str().empty());
}

TEST_F(LogAnalyzerExportTest, ExportAsJsonEdgeCases) {
    FilterExpression emptyFilter;
    std::stringstream ssEmptyNoSummary;
    (void)analyzer.exportAsJson(ssEmptyNoSummary, emptyFilter, false);
    nlohmann::json jEmpty = nlohmann::json::parse(ssEmptyNoSummary.str());
    ASSERT_EQ(jEmpty["summary"]["count"], 0);
    ASSERT_TRUE(jEmpty["entries"].empty());

    std::string logContent = "2023-01-01 10:00:00 INFO: First message.\n2023-01-01 10:01:00 DEBUG: Second message.\n";
    std::string filePath = "test_json_edge_cases.log";
    std::ofstream ofs(filePath);
    ofs << logContent;
    ofs.close();
    auto reportResult = analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
    ASSERT_TRUE(reportResult.has_value());
    std::remove(filePath.c_str());

    std::stringstream ss;
    FilterExpression allFilter;
    (void)analyzer.exportAsJson(ss, allFilter, true);
    nlohmann::json j = nlohmann::json::parse(ss.str());
    ASSERT_EQ(j["summary"]["count"], 2);
    ASSERT_EQ(j["entries"].size(), 2);
    ASSERT_EQ(j["entries"][0]["level"], "INFO");
    ASSERT_EQ(j["entries"][1]["level"], "DEBUG");
}

TEST_F(LogAnalyzerExportTest, ConcurrentLoadAsyncWithFilterAndExport) {
    const std::string filePath = "test_concurrent_async_filter_export.log";
    std::ofstream ofs(filePath);
    for (int i = 0; i < 1000; ++i) {
        ofs << "2023-01-01 12:00:" << (i % 60 < 10 ? "0" : "") << (i % 60) << " INFO: async-msg-" << i << "\n";
    }
    ofs.close();

    auto futureLoad = analyzer.loadAsync(filePath, ParserErrorAction::Warn);
    auto cond = FilterCondition::createString(LogEntryField::LEVEL, FilterOperator::NOT_EQUALS, "NONE");
    ASSERT_TRUE(cond.has_value());
    FilterExpression all(*cond);

    for (int i = 0; i < 500 && futureLoad.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready; ++i) {
        auto filtered = analyzer.getFilteredEntries(all);
        ASSERT_TRUE(filtered.has_value());
        std::stringstream jsonOut;
        ASSERT_NO_THROW((void)analyzer.exportAsJson(jsonOut, all, false));
        nlohmann::json j;
        ASSERT_NO_THROW(j = nlohmann::json::parse(jsonOut.str()));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    auto result = futureLoad.get();
    std::remove(filePath.c_str());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(analyzer.getEntriesSnapshot().size(), 1000);
}
