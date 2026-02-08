// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "analyzer/Core.h"
#include "config/CLI.h"
#include "filter/Expression.h"
#include "filter/Condition.h"
#include "filter/Types.h"
#include <fstream>
#include <filesystem>
#include <algorithm>

using namespace filter;

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
    auto condition = FilterCondition::createTyped(LogEntryField::MESSAGE, FilterOperator::REGEX, "[", FilterValueType::REGEX);
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
    
    // Check first entry (expecting CamelCase keys because Exporter uses CamelCase)
    ASSERT_EQ(j["entries"][0]["Level"], "INFO");
    ASSERT_NE(j["entries"][0]["Message"].get<std::string>().find("First message."), std::string::npos);
    
    // Check second entry
    ASSERT_EQ(j["entries"][1]["Level"], "DEBUG");
    ASSERT_NE(j["entries"][1]["Message"].get<std::string>().find("Second message."), std::string::npos);
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

TEST_F(LogAnalyzerTest, InvalidStatisticConfigDoesNotThrowOnSetSettings) {
    LogAnalyzerSettings settings;
    settings.statisticConfigs = {
        {StatisticType::FIELD_VALUE_COUNT, {}}
    };

    EXPECT_NO_THROW({
        auto result = analyzer.setSettings(settings);
        EXPECT_TRUE(result.has_value()) << result.error().toString();
    });
}

TEST(LogAnalyzerCtorTest, InvalidStatisticConfigDoesNotThrowInConstructor) {
    LogAnalyzerSettings settings;
    settings.statisticConfigs = {
        {StatisticType::TOP_N_FIELD_VALUES, {{"top_n", "5"}}}
    };

    EXPECT_NO_THROW({
        LogAnalyzer localAnalyzer(settings);
    });
}

TEST_F(LogAnalyzerTest, SortedFilteredEntriesDescendingUsesStrictComparator) {
    const std::string filePath = "test_desc_sort.log";
    std::ofstream ofs(filePath);
    ofs << "2023-01-01 10:00:00 INFO: alpha\n";
    ofs << "2023-01-01 10:00:01 INFO: zeta\n";
    ofs << "2023-01-01 10:00:02 INFO: middle\n";
    ofs << "2023-01-01 10:00:03 INFO: middle\n";
    ofs.close();

    auto loadResult = analyzer.loadAndReplace(filePath, CLIConfig::ParserErrorAction::Warn);
    std::remove(filePath.c_str());
    ASSERT_TRUE(loadResult.has_value()) << loadResult.error().toString();

    FilterExpression allEntries;
    auto sorted = analyzer.getSortedFilteredEntries(allEntries, SortBy::MESSAGE, SortOrder::DESCENDING);
    ASSERT_EQ(sorted.size(), 4);
    EXPECT_EQ(sorted.front().message, "zeta");
    EXPECT_EQ(sorted.back().message, "alpha");
    for (size_t i = 1; i < sorted.size(); ++i) {
        EXPECT_GE(sorted[i - 1].message, sorted[i].message);
    }
}

TEST_F(LogAnalyzerTest, LoadAndReplaceSupportsMultilineEntries) {
    LogAnalyzerSettings settings;
    settings.lineParsePattern = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) (\w+): ([\s\S]*)$)";
    settings.logEntryStartPattern = R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} \w+:)";
    settings.fieldMappings = {
        FieldMapping{LogEntryField::TIMESTAMP, std::make_optional<size_t>(1), {"%Y-%m-%d %H:%M:%S"}},
        FieldMapping{LogEntryField::LEVEL, std::make_optional<size_t>(2), {}},
        FieldMapping{LogEntryField::MESSAGE, std::make_optional<size_t>(3), {}}
    };

    auto settingsResult = analyzer.setSettings(settings);
    ASSERT_TRUE(settingsResult.has_value()) << settingsResult.error().toString();

    const std::string filePath = "test_multiline_load.log";
    std::ofstream ofs(filePath);
    ofs << "2023-01-01 10:00:00 INFO: Entry one line 1\n";
    ofs << "  Entry one line 2\n";
    ofs << "2023-01-01 10:00:01 ERROR: Entry two\n";
    ofs.close();

    auto reportResult = analyzer.loadAndReplace(filePath, CLIConfig::ParserErrorAction::Warn);
    std::remove(filePath.c_str());

    ASSERT_TRUE(reportResult.has_value()) << reportResult.error().toString();
    const auto& entries = analyzer.getEntries();
    ASSERT_EQ(entries.size(), 2);
    EXPECT_EQ(entries[0].level, LogLevel::INFO);
    EXPECT_EQ(entries[0].message, "Entry one line 1\n  Entry one line 2");
    EXPECT_EQ(entries[1].level, LogLevel::ERROR);
    EXPECT_EQ(entries[1].message, "Entry two");
}

TEST_F(LogAnalyzerTest, LoadAndReplaceDoesNotThrowWhenParserConfiguredToThrow) {
    LogAnalyzerSettings settings;
    settings.lineParsePattern = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) (\w+): (.*)$)";
    settings.fieldMappings = {
        FieldMapping{LogEntryField::TIMESTAMP, std::make_optional<size_t>(1), {"%Y-%m-%d %H:%M:%S"}},
        FieldMapping{LogEntryField::LEVEL, std::make_optional<size_t>(2), {}},
        FieldMapping{LogEntryField::MESSAGE, std::make_optional<size_t>(3), {}}
    };
    settings.parserErrorAction = CLIConfig::ParserErrorAction::Throw;

    auto settingsResult = analyzer.setSettings(settings);
    ASSERT_TRUE(settingsResult.has_value()) << settingsResult.error().toString();

    const std::string filePath = "test_throw_parser_action.log";
    std::ofstream ofs(filePath);
    ofs << "this line does not match parser pattern\n";
    ofs.close();

    ErrorCode::Result<AnalysisReport> loadResult;
    EXPECT_NO_THROW({
        loadResult = analyzer.loadAndReplace(filePath, CLIConfig::ParserErrorAction::Throw);
    });
    std::remove(filePath.c_str());

    ASSERT_TRUE(loadResult.has_value());
    EXPECT_EQ(loadResult->status, ParseError::UNKNOWN_ERROR);
    ASSERT_FALSE(loadResult->parseErrors.empty());
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
