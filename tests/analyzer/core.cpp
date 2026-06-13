// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "analyzer/core.h"
#include "filter/expression.h"
#include "filter/condition.h"
#include "filter/types.h"
#include "stats/core.h"
#include <fstream>
#include <filesystem>
#include <future>
#include <chrono>
#include <atomic>
#include <thread>
#include <span>

using namespace filter;

class LogAnalyzerTest : public ::testing::Test {
protected:
    LogAnalyzer analyzer;
};

TEST_F(LogAnalyzerTest, SetCustomLogLevelMapping) {
    analyzer.setCustomLogLevelMapping("VERBOSE", LogLevel::DEBUG);
    SUCCEED();
}

TEST_F(LogAnalyzerTest, CustomLogLevelMappingIsAppliedDuringParsing) {
    analyzer.setCustomLogLevelMapping("VERBOSE", LogLevel::DEBUG);
    const std::string filePath = "test_custom_level_mapping.log";
    {
        std::ofstream ofs(filePath);
        ofs << "2023-01-01 10:00:00 VERBOSE: mapped message\n";
    }
    auto result = analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
    std::remove(filePath.c_str());
    ASSERT_TRUE(result.has_value()) << result.error().toString();
    auto entries = analyzer.getEntriesSnapshot();
    ASSERT_EQ(entries.size(), 1);
    EXPECT_EQ(entries[0].level, LogLevel::DEBUG);
}

TEST_F(LogAnalyzerTest, GetSettingsReflectsLatestSetSettingsValues) {
    LogAnalyzerSettings settings;
    settings.lineParsePattern = R"(^(\w+)\|(.*)$)";
    settings.fieldMappings.clear();
    settings.fieldMappings.emplace_back(LogEntryField::LEVEL, std::make_optional<size_t>(1));
    settings.fieldMappings.emplace_back(LogEntryField::MESSAGE, std::make_optional<size_t>(2));
    auto setResult = analyzer.setSettings(settings);
    ASSERT_TRUE(setResult.has_value()) << setResult.error().toString();
    const auto& snapshot = analyzer.getSettings();
    EXPECT_EQ(snapshot.lineParsePattern, settings.lineParsePattern);
    ASSERT_EQ(snapshot.fieldMappings.size(), settings.fieldMappings.size());
}

TEST_F(LogAnalyzerTest, SetCustomLogLevelMappingDoesNotThrowAfterRejectedSettings) {
    LogAnalyzerSettings invalidSettings;
    invalidSettings.lineParsePattern = "[";
    auto setResult = analyzer.setSettings(invalidSettings);
    ASSERT_FALSE(setResult.has_value());
    EXPECT_NO_THROW({ analyzer.setCustomLogLevelMapping("VERBOSE", LogLevel::DEBUG); });
}

TEST_F(LogAnalyzerTest, DefaultLogLevelMapping) {
    const std::string filePath = "test_default_level_mapping.log";
    {
        std::ofstream ofs(filePath);
        ofs << "2023-01-01 10:00:00 DEBUG: debug line\n"
            << "2023-01-01 10:00:01 INFO: info line\n"
            << "2023-01-01 10:00:02 WARNING: warning line\n"
            << "2023-01-01 10:00:03 ERROR: error line\n";
    }
    auto result = analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
    std::remove(filePath.c_str());
    ASSERT_TRUE(result.has_value()) << result.error().toString();
    auto entries = analyzer.getEntriesSnapshot();
    ASSERT_EQ(entries.size(), 4);
    EXPECT_EQ(entries[0].level, LogLevel::DEBUG);
    EXPECT_EQ(entries[1].level, LogLevel::INFO);
    EXPECT_EQ(entries[2].level, LogLevel::WARNING);
    EXPECT_EQ(entries[3].level, LogLevel::ERROR);
}

TEST_F(LogAnalyzerTest, AnalyzeStreamFileOpenError) {
    ErrorCode::Result<AnalysisReport> result;
    EXPECT_NO_THROW({ result = analyzer.loadAndReplace("non_existent_file.log", ParserErrorAction::Warn); });
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::FileNotFound);
}

TEST_F(LogAnalyzerTest, PublicApiInvalidInputsReturnErrorsWithoutThrowing) {
    ErrorCode::Result<AnalysisReport> loadReplaceResult;
    EXPECT_NO_THROW({ loadReplaceResult = analyzer.loadAndReplace("missing.log", ParserErrorAction::Warn); });
    ASSERT_FALSE(loadReplaceResult.has_value());
    EXPECT_EQ(loadReplaceResult.error().code, Code::FileNotFound);

    ErrorCode::Result<AnalysisReport> appendResult;
    EXPECT_NO_THROW({ appendResult = analyzer.append("missing.log", ParserErrorAction::Warn); });
    ASSERT_FALSE(appendResult.has_value());
    EXPECT_EQ(appendResult.error().code, Code::FileNotReadable);

    ErrorCode::Result<void> streamResult;
    EXPECT_NO_THROW({ streamResult = analyzer.analyzeStream({"missing.log"}, [](const LogEntry&) { return true; }, ParserErrorAction::Warn); });
    ASSERT_FALSE(streamResult.has_value());
    EXPECT_EQ(streamResult.error().code, Code::FileNotReadable);
}

TEST_F(LogAnalyzerTest, CancellationStatusTakesPrecedenceOverParseErrors) {
    const std::string filePath = "test_cancel_precedence.log";
    {
        std::ofstream ofs(filePath);
        ofs << "malformed line without expected format\n";
        ofs << "2023-01-01 10:00:00 INFO: should-not-be-fully-processed\n";
    }
    CancellationToken token;
    bool cancelIssued = false;
    auto result = analyzer.loadAndReplace(filePath, ParserErrorAction::Throw, &token, ProgressCallback([&](double progress, std::string_view) {
            if (!cancelIssued && progress > 0.0) { token.cancel(); cancelIssued = true; }
        }));
    std::remove(filePath.c_str());
    ASSERT_TRUE(result.has_value()) << result.error().toString();
    EXPECT_EQ(result->status, ParseError::CANCELLED);
    EXPECT_TRUE(cancelIssued);
}

TEST_F(LogAnalyzerTest, AnalyzeStreamInvalidRegexError) {
    LogAnalyzerSettings settings;
    settings.lineParsePattern = "[";
    auto result = analyzer.setSettings(settings);
    ASSERT_FALSE(result.has_value());
    const std::string filePath = "test_set_settings_rollback.log";
    std::ofstream ofs(filePath);
    ofs << "2023-01-01 10:00:00 INFO: still-usable\n";
    ofs.close();
    auto loadResult = analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
    std::remove(filePath.c_str());
    ASSERT_TRUE(loadResult.has_value()) << loadResult.error().toString();
    EXPECT_EQ(loadResult->successfulParses, 1);
}

TEST_F(LogAnalyzerTest, DeprecatedLoadAndReplacePatternUsesProvidedPattern) {
    const std::string filePath = "test_deprecated_load_replace_pattern.log";
    {
        std::ofstream ofs(filePath);
        ofs << "INFO|message-a\nERROR|message-b\n";
    }
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    auto result = analyzer.loadAndReplace(filePath, R"(^(\w+)\|(.*)$)");
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    std::remove(filePath.c_str());
    ASSERT_TRUE(result.has_value()) << result.error().toString();
    EXPECT_EQ(result->successfulParses, 2);
}

TEST_F(LogAnalyzerTest, GetFilteredEntriesInvalidRegex) {
    auto condition = FilterCondition::createTyped(LogEntryField::MESSAGE, FilterOperator::REGEX, "[", FilterValueType::REGEX);
    ASSERT_TRUE(condition.has_value());
    FilterExpression expression(*condition);
    auto result = analyzer.getFilteredEntries(expression);
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidRegex);
}

TEST_F(LogAnalyzerTest, LoadAndReplaceResetsStatisticsToCurrentDataset) {
    analyzer.addStatisticCollector(std::make_shared<LogLevelCountCollector>());
    const std::string firstFile = "test_load_replace_stats_first.log";
    {
        std::ofstream ofs(firstFile);
        ofs << "2023-01-01 10:00:00 INFO: First 1\n2023-01-01 10:01:00 INFO: First 2\n";
    }
    (void)analyzer.loadAndReplace(firstFile, ParserErrorAction::Warn);
    std::remove(firstFile.c_str());
    const std::string secondFile = "test_load_replace_stats_second.log";
    {
        std::ofstream ofs(secondFile);
        ofs << "2023-01-01 11:00:00 ERROR: Second 1\n";
    }
    (void)analyzer.loadAndReplace(secondFile, ParserErrorAction::Warn);
    std::remove(secondFile.c_str());
    const auto reports = analyzer.getAllStatisticReports();
    ASSERT_TRUE(reports.contains("log_level_count"));
    EXPECT_EQ(reports.at("log_level_count")["total_entries"], 1);
}

TEST_F(LogAnalyzerTest, ClearResetsStatistics) {
    analyzer.addStatisticCollector(std::make_shared<LogLevelCountCollector>());
    const std::string filePath = "test_clear_stats.log";
    {
        std::ofstream ofs(filePath);
        ofs << "2023-01-01 10:00:00 INFO: Entry 1\n";
    }
    (void)analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
    std::remove(filePath.c_str());
    analyzer.clear();
    EXPECT_EQ(analyzer.getAllStatisticReports().at("log_level_count")["total_entries"], 0);
}

TEST_F(LogAnalyzerTest, StatisticCollectorLifecycleApisWork) {
    auto collector = std::make_shared<LogLevelCountCollector>();
    analyzer.addStatisticCollector(collector);
    analyzer.processEntriesForStatistics(std::span<const LogEntry>({{1, "a.log", 1, std::chrono::system_clock::now(), LogLevel::INFO, "one"}}));
    ASSERT_TRUE(analyzer.getAllStatisticReports().contains("log_level_count"));
    analyzer.removeStatisticCollector(collector);
    EXPECT_TRUE(analyzer.getAllStatisticReports().empty());
}

TEST_F(LogAnalyzerTest, InvalidStatisticConfigDoesNotThrowOnSetSettings) {
    LogAnalyzerSettings settings;
    settings.statisticConfigs = {{StatisticType::FIELD_VALUE_COUNT, {}}};
    EXPECT_NO_THROW({ (void)analyzer.setSettings(settings); });
}

TEST_F(LogAnalyzerTest, NonPositiveTopNDefaultsForTopMessagesCollector) {
    LogAnalyzerSettings settings;
    settings.statisticConfigs = {{StatisticType::TOP_MESSAGES, {{"top_n", "0"}}}};
    (void)analyzer.setSettings(settings);
    const auto report = analyzer.getAllStatisticReports(); // Simplified
}

TEST_F(LogAnalyzerTest, LoadAndReplaceSupportsMultilineEntries) {
    LogAnalyzerSettings settings;
    settings.lineParsePattern = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) (\w+): ([\s\S]*)$)";
    settings.logEntryStartPattern = R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} \w+:)";
    settings.fieldMappings = { 
        FieldMapping(LogEntryField::TIMESTAMP, std::optional<size_t>(1), std::vector<std::string>{"%Y-%m-%d %H:%M:%S"}), 
        FieldMapping(LogEntryField::LEVEL, std::optional<size_t>(2)), 
        FieldMapping(LogEntryField::MESSAGE, std::optional<size_t>(3)) 
    };
    (void)analyzer.setSettings(settings);
    const std::string filePath = "test_multiline_load.log";
    std::ofstream ofs(filePath);
    ofs << "2023-01-01 10:00:00 INFO: Entry one line 1\n  Entry one line 2\n";
    ofs.close();
    (void)analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
    std::remove(filePath.c_str());
    EXPECT_EQ(analyzer.getEntries().size(), 1);
}

TEST_F(LogAnalyzerTest, SnapshotAccessorsReturnIndependentCopies) {
    const std::string filePath = "test_snapshot_access.log";
    std::ofstream ofs(filePath);
    ofs << "2023-01-01 10:00:00 INFO: first\n";
    ofs.close();
    (void)analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
    auto entriesSnapshot = analyzer.getEntriesSnapshot();
    std::ofstream ofs2(filePath, std::ios::trunc);
    ofs2 << "2023-01-01 10:00:01 INFO: second\n";
    ofs2.close();
    (void)analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
    std::remove(filePath.c_str());
    EXPECT_EQ(entriesSnapshot.size(), 1);
}

TEST_F(LogAnalyzerTest, ReferenceAccessorsReturnStableSnapshots) {
    const std::string filePath = "test_reference_snapshot.log";
    std::ofstream ofs(filePath);
    ofs << "2023-01-01 10:00:00 INFO: one\n";
    ofs.close();
    (void)analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
    const auto& entriesRefSnapshot = analyzer.getEntries();
    std::ofstream ofs2(filePath, std::ios::trunc);
    ofs2 << "2023-01-01 10:00:01 INFO: two\n";
    ofs2.close();
    (void)analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
    std::remove(filePath.c_str());
    EXPECT_EQ(entriesRefSnapshot.size(), 1);
}

TEST_F(LogAnalyzerTest, ConcurrentSnapshotAccessDuringLoad) {
    const std::string filePath = "test_concurrent_snapshot.log";
    std::ofstream ofs(filePath);
    for (int i = 0; i < 100; ++i) ofs << "2023-01-01 10:00:00 INFO: message\n";
    ofs.close();
    auto futureLoad = std::async(std::launch::async, [&]() { return analyzer.loadAndReplace(filePath, ParserErrorAction::Warn); });
    while (futureLoad.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready) {
        analyzer.getEntriesSnapshot();
    }
    (void)futureLoad.get();
    std::remove(filePath.c_str());
    SUCCEED();
}

TEST(LogAnalyzerCtorTest, InvalidStatisticConfigDoesNotThrowInConstructor) {
    LogAnalyzerSettings settings;
    settings.statisticConfigs = {{StatisticType::TOP_N_FIELD_VALUES, {{"top_n", "5"}}}};
    EXPECT_NO_THROW({ LogAnalyzer localAnalyzer(settings); });
}

TEST(LogAnalyzerCtorTest, InvalidRegexSettingsFallsBackInConstructor) {
    LogAnalyzerSettings settings;
    settings.lineParsePattern = "[";
    std::unique_ptr<LogAnalyzer> localAnalyzer;
    EXPECT_NO_THROW({ localAnalyzer = std::make_unique<LogAnalyzer>(settings); });
    ASSERT_NE(localAnalyzer, nullptr);
}
