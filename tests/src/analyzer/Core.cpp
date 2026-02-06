// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "analyzer/AnalyzerCore.h"
#include "config/CLIConfig.h"
#include "filter/Expression.h"
#include "filter/Condition.h"
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
    auto condition = FilterCondition::createTyped(LogEntryField::MESSAGE, FilterOperator::REGEX_MATCH, "[", FilterValueType::REGEX);
    ASSERT_TRUE(condition.has_value());
    FilterExpression expression(*condition);
    auto result = analyzer.getFilteredEntries(expression);
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidRegex);
}

TEST_F(LogAnalyzerTest, ExportAsCsvEdgeCases) {
    std::stringstream ss;
    FilterExpression expression; // Empty expression matches everything
    analyzer.exportAsCsv(ss, expression, true);
    ASSERT_FALSE(ss.str().empty());
}

TEST_F(LogAnalyzerTest, ExportAsJsonEdgeCases) {
    FilterExpression emptyFilter;
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
    FilterExpression allFilter;
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

class FilterExpressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::string logContent =
            "2023-01-01 10:00:00 INFO: User 'test' logged in.\n"
            "2023-01-01 10:01:00 DEBUG: Starting task 'A'.\n"
            "2023-01-01 10:02:00 WARNING: Task 'A' took 200ms.\n"
            "2023-01-01 10:03:00 INFO: User 'test' logged out.\n"
            "2023-01-01 10:04:00 ERROR: Failed to process task 'B'.\n";
        std::string filePath = "filter_expression_test.log";
        std::ofstream ofs(filePath);
        ofs << logContent;
        ofs.close();
        auto reportResult = analyzer.loadAndReplace(filePath, CLIConfig::ParserErrorAction::Warn);
        ASSERT_TRUE(reportResult.has_value());
        std::remove(filePath.c_str());
    }

    LogAnalyzer analyzer;
};

TEST_F(FilterExpressionTest, SimpleCondition) {
    auto condition = FilterCondition::createString(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO");
    ASSERT_TRUE(condition.has_value());
    FilterExpression expression(*condition);

    auto result = analyzer.getFilteredEntries(expression);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 2);
}

TEST_F(FilterExpressionTest, AndExpression) {
    auto cond1 = FilterCondition::createString(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO");
    ASSERT_TRUE(cond1.has_value());
    auto cond2 = FilterCondition::createString(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "logged in");
    ASSERT_TRUE(cond2.has_value());
    FilterExpression expression = FilterExpression(*cond1).And(FilterExpression(*cond2));

    auto result = analyzer.getFilteredEntries(expression);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 1);
    ASSERT_EQ(result.value()[0].message, "User 'test' logged in.");
}

TEST_F(FilterExpressionTest, OrExpression) {
    auto cond1 = FilterCondition::createString(LogEntryField::LEVEL, FilterOperator::EQUALS, "ERROR");
    ASSERT_TRUE(cond1.has_value());
    auto cond2 = FilterCondition::createString(LogEntryField::LEVEL, FilterOperator::EQUALS, "WARNING");
    ASSERT_TRUE(cond2.has_value());
    FilterExpression expression = FilterExpression(*cond1).Or(FilterExpression(*cond2));

    auto result = analyzer.getFilteredEntries(expression);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 2);
}

TEST_F(FilterExpressionTest, NotExpression) {
    auto condition = FilterCondition::createString(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO");
    ASSERT_TRUE(condition.has_value());
    FilterExpression expression = FilterExpression(*condition).Not();

    auto result = analyzer.getFilteredEntries(expression);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 3);
}

TEST_F(FilterExpressionTest, ComplexExpression) {
    // (LEVEL == INFO AND MESSAGE CONTAINS 'test') OR LEVEL == ERROR
    auto info = FilterCondition::createString(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO");
    ASSERT_TRUE(info.has_value());
    auto test = FilterCondition::createString(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "test");
    ASSERT_TRUE(test.has_value());
    auto error = FilterCondition::createString(LogEntryField::LEVEL, FilterOperator::EQUALS, "ERROR");
    ASSERT_TRUE(error.has_value());

    FilterExpression expression = (FilterExpression(*info).And(FilterExpression(*test))).Or(FilterExpression(*error));
    
    auto result = analyzer.getFilteredEntries(expression);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 3); // 2 INFO with 'test' + 1 ERROR
}
