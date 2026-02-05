// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "analyzer/AnalyzerCore.h"
#include "config/CLIConfig.h"
#include <fstream>
#include <filesystem>

class LogAnalyzerTest : public ::testing::Test {
protected:
    LogAnalyzer analyzer;
};

TEST_F(LogAnalyzerTest, SetCustomLogLevelMapping) {
    analyzer.setCustomLogLevelMapping("VERBOSE", LogLevel::DEBUG);
    SUCCEED();
}

TEST_F(LogAnalyzerTest, DefaultLogLevelMapping) {
    SUCCEED();
}

TEST_F(LogAnalyzerTest, AnalyzeStreamFileOpenError) {
    auto result = analyzer.loadAndReplace("non_existent_file.log", CLIConfig::ParserErrorAction::Warn);
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::FileNotFound);
}

TEST_F(LogAnalyzerTest, AnalyzeStreamInvalidRegexError) {
    LogAnalyzerSettings settings;
    settings.lineParsePattern = "["; // Invalid regex
    auto result = analyzer.setSettings(settings);
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidRegex);
}

TEST_F(LogAnalyzerTest, GetFilteredEntriesInvalidRegex) {
    FilterCriteria criteria;
    criteria.regexPattern = "["; // Invalid regex
    auto result = analyzer.getFilteredEntries(criteria);
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidRegex);
}

TEST_F(LogAnalyzerTest, ExportAsCsvEdgeCases) {
    std::stringstream ss;
    FilterCriteria criteria;
    analyzer.exportAsCsv(ss, criteria, true);
    ASSERT_FALSE(ss.str().empty());
}

TEST_F(LogAnalyzerTest, ExportAsJsonEdgeCases) {
    FilterCriteria emptyFilter;
    std::stringstream ssEmptyNoSummary;
    analyzer.exportAsJson(ssEmptyNoSummary, emptyFilter, false);
    nlohmann::json jEmpty = nlohmann::json::parse(ssEmptyNoSummary.str());
    ASSERT_EQ(jEmpty["summary"]["count"], 0);
    ASSERT_TRUE(jEmpty["entries"].empty());

    std::string logContent = 
        "2023-01-01 10:00:00 INFO: First message.\n"
        "2023-01-01 10:01:00 DEBUG: Second message.\n";
    std::string filePath = "test_json_edge_cases.log";
    std::ofstream ofs(filePath);
    ofs << logContent;
    ofs.close();
    auto reportResult = analyzer.loadAndReplace(filePath, CLIConfig::ParserErrorAction::Warn);
    ASSERT_TRUE(reportResult.has_value()) << "Load failed with error: " << reportResult.error().toString();
    AnalysisReport report = reportResult.value();
    ASSERT_EQ(report.successfulParses, 2) << "Expected 2 successful parses, but got " << report.successfulParses;
    std::remove(filePath.c_str());

    std::stringstream ss;
    FilterCriteria allFilter;
    analyzer.exportAsJson(ss, allFilter, true);

    nlohmann::json j = nlohmann::json::parse(ss.str());
    ASSERT_EQ(j["summary"]["count"], 2);
    ASSERT_EQ(j["entries"].size(), 2);
    
    // Check first entry (expecting UPPERCASE keys because Exporter uses logEntryFieldToString)
    ASSERT_EQ(j["entries"][0]["LEVEL"], "INFO");
    ASSERT_NE(j["entries"][0]["MESSAGE"].get<std::string>().find("First message."), std::string::npos);
    
    // Check second entry
    ASSERT_EQ(j["entries"][1]["LEVEL"], "DEBUG");
    ASSERT_NE(j["entries"][1]["MESSAGE"].get<std::string>().find("Second message."), std::string::npos);
}

TEST_F(LogAnalyzerTest, AppendCorrectness) {
    std::string logContent1 = "2023-01-01 10:00:00 INFO Entry 1\n";
    std::string filePath1 = "test_append_1.log";
    std::ofstream ofs1(filePath1);
    ofs1 << logContent1;
    ofs1.close();
    auto result1 = analyzer.append(filePath1, CLIConfig::ParserErrorAction::Warn);
    ASSERT_TRUE(result1.has_value());
    std::remove(filePath1.c_str());
}

TEST_F(LogAnalyzerTest, Concurrency_Placeholder) {
    SUCCEED();
}
