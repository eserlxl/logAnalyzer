// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "analyzer/Core.h"
#include "filter/Expression.h"
#include "filter/Condition.h"
#include "filter/Types.h"
#include "stats/Core.h"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <future>
#include <chrono>
#include <atomic>
#include <thread>
#include <map>
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

TEST_F(LogAnalyzerTest, SetCustomLogLevelMappingDoesNotThrowAfterRejectedSettings) {
    LogAnalyzerSettings invalidSettings;
    invalidSettings.lineParsePattern = "[";
    auto setResult = analyzer.setSettings(invalidSettings);
    ASSERT_FALSE(setResult.has_value());

    EXPECT_NO_THROW({
        analyzer.setCustomLogLevelMapping("VERBOSE", LogLevel::DEBUG);
    });
}

TEST_F(LogAnalyzerTest, DefaultLogLevelMapping) {
    SUCCEED();
}

TEST_F(LogAnalyzerTest, AnalyzeStreamFileOpenError) {
    ErrorCode::Result<AnalysisReport> result;
    EXPECT_NO_THROW({
        result = analyzer.loadAndReplace("non_existent_file.log", ParserErrorAction::Warn);
    });
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::FileNotFound);
}

TEST_F(LogAnalyzerTest, PublicApiInvalidInputsReturnErrorsWithoutThrowing) {
    ErrorCode::Result<AnalysisReport> loadReplaceResult;
    EXPECT_NO_THROW({
        loadReplaceResult = analyzer.loadAndReplace("missing.log", ParserErrorAction::Warn);
    });
    ASSERT_FALSE(loadReplaceResult.has_value());
    EXPECT_EQ(loadReplaceResult.error().code, Code::FileNotFound);

    ErrorCode::Result<AnalysisReport> appendResult;
    EXPECT_NO_THROW({
        appendResult = analyzer.append("missing.log", ParserErrorAction::Warn);
    });
    ASSERT_FALSE(appendResult.has_value());
    EXPECT_EQ(appendResult.error().code, Code::FileNotReadable);

    ErrorCode::Result<void> streamResult;
    EXPECT_NO_THROW({
        streamResult = analyzer.analyzeStream({"missing.log"},
            [](const LogEntry&) { return true; },
            ParserErrorAction::Warn);
    });
    ASSERT_FALSE(streamResult.has_value());
    EXPECT_EQ(streamResult.error().code, Code::FileNotReadable);
}

TEST_F(LogAnalyzerTest, AnalyzeStreamInvalidRegexError) {
    LogAnalyzerSettings settings;
    settings.lineParsePattern = "["; // Invalid regex
    auto result = analyzer.setSettings(settings);
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidRegex);

    const std::string filePath = "test_set_settings_rollback.log";
    std::ofstream ofs(filePath);
    ofs << "2023-01-01 10:00:00 INFO: still-usable\n";
    ofs.close();

    auto loadResult = analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
    std::remove(filePath.c_str());
    ASSERT_TRUE(loadResult.has_value()) << loadResult.error().toString();
    EXPECT_EQ(loadResult->successfulParses, 1);
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
    auto reportResult = analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
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
    std::string logContent1 = "2023-01-01 10:00:00 INFO: Entry 1\n";
    std::string filePath1 = "test_append_1.log";
    std::ofstream ofs1(filePath1);
    ofs1 << logContent1;
    ofs1.close();
    auto result1 = analyzer.append(filePath1, ParserErrorAction::Warn);
    ASSERT_TRUE(result1.has_value());
    std::remove(filePath1.c_str());
}

TEST_F(LogAnalyzerTest, AppendUpdatesStatisticsWhenAnalyzerStartsEmpty) {
    analyzer.addStatisticCollector(std::make_shared<LogLevelCountCollector>());

    const std::string filePath = "test_append_stats_empty.log";
    {
        std::ofstream ofs(filePath);
        ofs << "2023-01-01 10:00:00 INFO: Entry 1\n";
        ofs << "2023-01-01 10:01:00 ERROR: Entry 2\n";
    }

    auto appendResult = analyzer.append(filePath, ParserErrorAction::Warn);
    std::remove(filePath.c_str());
    ASSERT_TRUE(appendResult.has_value()) << appendResult.error().toString();

    const auto reports = analyzer.getAllStatisticReports();
    ASSERT_TRUE(reports.contains("log_level_count"));
    const auto& report = reports.at("log_level_count");
    ASSERT_EQ(report["total_entries"], 2);
    ASSERT_EQ(report["counts"]["INFO"], 1);
    ASSERT_EQ(report["counts"]["ERROR"], 1);
}

TEST_F(LogAnalyzerTest, StreamInUpdatesStatisticsWhenAnalyzerStartsEmpty) {
    analyzer.addStatisticCollector(std::make_shared<LogLevelCountCollector>());

    std::stringstream ss;
    ss << "2023-01-01 10:00:00 INFO: Stream entry 1\n";
    ss << "2023-01-01 10:01:00 WARNING: Stream entry 2\n";

    auto streamResult = analyzer.streamIn(ss, "stats_stream", ParserErrorAction::Warn);
    ASSERT_TRUE(streamResult.has_value()) << streamResult.error().toString();

    const auto reports = analyzer.getAllStatisticReports();
    ASSERT_TRUE(reports.contains("log_level_count"));
    const auto& report = reports.at("log_level_count");
    ASSERT_EQ(report["total_entries"], 2);
    ASSERT_EQ(report["counts"]["INFO"], 1);
    ASSERT_EQ(report["counts"]["WARNING"], 1);
}

TEST_F(LogAnalyzerTest, StatisticCollectorLifecycleApisWork) {
    auto collector = std::make_shared<LogLevelCountCollector>();
    analyzer.addStatisticCollector(collector);

    const auto now = std::chrono::system_clock::now();
    std::vector<LogEntry> entries = {
        {1, "a.log", 1, now, LogLevel::INFO, "one"},
        {2, "a.log", 2, now + std::chrono::seconds(1), LogLevel::ERROR, "two"},
    };

    analyzer.processEntriesForStatistics(std::span<const LogEntry>(entries));
    auto reports = analyzer.getAllStatisticReports();
    ASSERT_TRUE(reports.contains("log_level_count"));
    EXPECT_EQ(reports["log_level_count"]["total_entries"], 2);
    EXPECT_EQ(reports["log_level_count"]["counts"]["INFO"], 1);
    EXPECT_EQ(reports["log_level_count"]["counts"]["ERROR"], 1);

    analyzer.resetStatisticCollectors();
    reports = analyzer.getAllStatisticReports();
    ASSERT_TRUE(reports.contains("log_level_count"));
    EXPECT_EQ(reports["log_level_count"]["total_entries"], 0);

    analyzer.removeStatisticCollector(collector);
    reports = analyzer.getAllStatisticReports();
    EXPECT_TRUE(reports.empty());

    analyzer.addStatisticCollector(std::make_shared<LogLevelCountCollector>());
    analyzer.clearStatisticCollectors();
    reports = analyzer.getAllStatisticReports();
    EXPECT_TRUE(reports.empty());
}

TEST_F(LogAnalyzerTest, ConcurrentAppendAndFilterIsStable) {
    const std::string filePath1 = "test_concurrent_append_1.log";
    const std::string filePath2 = "test_concurrent_append_2.log";

    {
        std::ofstream ofs(filePath1);
        for (int i = 0; i < 150; ++i) {
            ofs << "2023-01-01 10:00:" << (i % 60 < 10 ? "0" : "") << (i % 60)
                << " INFO: append-a-" << i << "\n";
        }
    }
    {
        std::ofstream ofs(filePath2);
        for (int i = 0; i < 150; ++i) {
            ofs << "2023-01-01 11:00:" << (i % 60 < 10 ? "0" : "") << (i % 60)
                << " DEBUG: append-b-" << i << "\n";
        }
    }

    std::atomic<bool> writerDone{false};
    std::atomic<bool> writerOk{true};

    auto writer = std::async(std::launch::async, [&]() {
        auto r1 = analyzer.append(filePath1, ParserErrorAction::Warn);
        auto r2 = analyzer.append(filePath2, ParserErrorAction::Warn);
        writerOk.store(r1.has_value() && r2.has_value());
        writerDone.store(true);
    });

    auto cond = FilterCondition::createString(LogEntryField::LEVEL, FilterOperator::NOT_EQUALS, "NONE");
    ASSERT_TRUE(cond.has_value());
    FilterExpression all(*cond);

    size_t observedMax = 0;
    for (int i = 0; i < 400 && !writerDone.load(); ++i) {
        auto filteredResult = analyzer.getFilteredEntries(all);
        ASSERT_TRUE(filteredResult.has_value()) << filteredResult.error().toString();
        observedMax = std::max(observedMax, filteredResult->size());

        std::stringstream ss;
        ASSERT_NO_THROW(analyzer.exportAsJson(ss, all, false));
        ASSERT_NO_THROW((void)nlohmann::json::parse(ss.str()));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    writer.wait();
    std::remove(filePath1.c_str());
    std::remove(filePath2.c_str());
    ASSERT_TRUE(writerOk.load());

    auto finalEntries = analyzer.getEntriesSnapshot();
    EXPECT_EQ(finalEntries.size(), 300);
    EXPECT_LE(observedMax, finalEntries.size());
}

TEST_F(LogAnalyzerTest, ConcurrentAppendFromTwoThreadsPreservesAllEntries) {
    const std::string filePath1 = "test_concurrent_append_threads_1.log";
    const std::string filePath2 = "test_concurrent_append_threads_2.log";

    {
        std::ofstream ofs(filePath1);
        for (int i = 0; i < 120; ++i) {
            ofs << "2023-01-01 10:10:" << (i % 60 < 10 ? "0" : "") << (i % 60)
                << " INFO: thread-a-" << i << "\n";
        }
    }
    {
        std::ofstream ofs(filePath2);
        for (int i = 0; i < 130; ++i) {
            ofs << "2023-01-01 10:20:" << (i % 60 < 10 ? "0" : "") << (i % 60)
                << " INFO: thread-b-" << i << "\n";
        }
    }

    auto f1 = std::async(std::launch::async, [&]() {
        return analyzer.append(filePath1, ParserErrorAction::Warn);
    });
    auto f2 = std::async(std::launch::async, [&]() {
        return analyzer.append(filePath2, ParserErrorAction::Warn);
    });

    auto r1 = f1.get();
    auto r2 = f2.get();
    std::remove(filePath1.c_str());
    std::remove(filePath2.c_str());
    ASSERT_TRUE(r1.has_value()) << r1.error().toString();
    ASSERT_TRUE(r2.has_value()) << r2.error().toString();

    auto snapshot = analyzer.getEntriesSnapshot();
    EXPECT_EQ(snapshot.size(), 250);
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

TEST(LogAnalyzerCtorTest, InvalidRegexSettingsFallsBackInConstructor) {
    LogAnalyzerSettings settings;
    settings.lineParsePattern = "[";

    std::unique_ptr<LogAnalyzer> localAnalyzer;
    EXPECT_NO_THROW({
        localAnalyzer = std::make_unique<LogAnalyzer>(settings);
    });
    ASSERT_NE(localAnalyzer, nullptr);

    const std::string filePath = "test_ctor_fallback.log";
    std::ofstream ofs(filePath);
    ofs << "2023-01-01 10:00:00 INFO: fallback works\n";
    ofs.close();

    auto loadResult = localAnalyzer->loadAndReplace(filePath, ParserErrorAction::Warn);
    std::remove(filePath.c_str());
    ASSERT_TRUE(loadResult.has_value()) << loadResult.error().toString();
    EXPECT_EQ(loadResult->successfulParses, 1);
}

TEST_F(LogAnalyzerTest, SortedFilteredEntriesDescendingUsesStrictComparator) {
    const std::string filePath = "test_desc_sort.log";
    std::ofstream ofs(filePath);
    ofs << "2023-01-01 10:00:00 INFO: alpha\n";
    ofs << "2023-01-01 10:00:01 INFO: zeta\n";
    ofs << "2023-01-01 10:00:02 INFO: middle\n";
    ofs << "2023-01-01 10:00:03 INFO: middle\n";
    ofs.close();

    auto loadResult = analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
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

TEST_F(LogAnalyzerTest, SortedFilteredEntriesMaintainOrderingAndMembershipAcrossDirections) {
    const std::string filePath = "test_sort_properties.log";
    std::ofstream ofs(filePath);
    ofs << "2023-01-01 10:00:00 INFO: beta\n";
    ofs << "2023-01-01 10:00:01 ERROR: alpha\n";
    ofs << "2023-01-01 10:00:02 WARNING: alpha\n";
    ofs << "2023-01-01 10:00:03 DEBUG: gamma\n";
    ofs << "2023-01-01 10:00:04 INFO: beta\n";
    ofs << "2023-01-01 10:00:05 ERROR: delta\n";
    ofs.close();

    auto loadResult = analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
    std::remove(filePath.c_str());
    ASSERT_TRUE(loadResult.has_value()) << loadResult.error().toString();

    auto entryId = [](const LogEntry& e) {
        const auto ts = e.timestamp.has_value()
            ? std::to_string(e.timestamp->time_since_epoch().count())
            : std::string("no-ts");
        return ts + "|" +
               std::to_string(static_cast<int>(e.level)) + "|" + e.sourceFile + "|" + e.message;
    };

    auto lessByField = [](const LogEntry& lhs, const LogEntry& rhs, SortBy key) {
        switch (key) {
            case SortBy::TIMESTAMP:
                return lhs.timestamp < rhs.timestamp;
            case SortBy::LEVEL:
                return lhs.level < rhs.level;
            case SortBy::MESSAGE:
                return lhs.message < rhs.message;
            case SortBy::SOURCE:
                return lhs.sourceFile < rhs.sourceFile;
            case SortBy::THREAD_ID:
                if (lhs.threadId.has_value() && rhs.threadId.has_value()) {
                    return *lhs.threadId < *rhs.threadId;
                }
                return lhs.threadId.has_value() < rhs.threadId.has_value();
        }
        return false;
    };

    const std::vector<SortBy> sortKeys = {
        SortBy::TIMESTAMP,
        SortBy::LEVEL,
        SortBy::MESSAGE,
        SortBy::SOURCE,
        SortBy::THREAD_ID
    };

    FilterExpression allEntries;
    for (const auto sortBy : sortKeys) {
        const auto ascending = analyzer.getSortedFilteredEntries(allEntries, sortBy, SortOrder::ASCENDING);
        const auto descending = analyzer.getSortedFilteredEntries(allEntries, sortBy, SortOrder::DESCENDING);

        ASSERT_EQ(ascending.size(), descending.size()) << "Sort key: " << static_cast<int>(sortBy);
        ASSERT_EQ(ascending.size(), 6u) << "Sort key: " << static_cast<int>(sortBy);

        for (size_t i = 1; i < ascending.size(); ++i) {
            EXPECT_FALSE(lessByField(ascending[i], ascending[i - 1], sortBy)) << "Sort key: " << static_cast<int>(sortBy);
        }
        for (size_t i = 1; i < descending.size(); ++i) {
            EXPECT_FALSE(lessByField(descending[i - 1], descending[i], sortBy)) << "Sort key: " << static_cast<int>(sortBy);
        }

        std::map<std::string, size_t> ascCounts;
        std::map<std::string, size_t> descCounts;
        for (const auto& e : ascending) {
            ++ascCounts[entryId(e)];
        }
        for (const auto& e : descending) {
            ++descCounts[entryId(e)];
        }
        EXPECT_EQ(ascCounts, descCounts) << "Sort key: " << static_cast<int>(sortBy);
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

    auto reportResult = analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
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
    settings.parserErrorAction = ParserErrorAction::Throw;

    auto settingsResult = analyzer.setSettings(settings);
    ASSERT_TRUE(settingsResult.has_value()) << settingsResult.error().toString();

    const std::string filePath = "test_throw_parser_action.log";
    std::ofstream ofs(filePath);
    ofs << "this line does not match parser pattern\n";
    ofs.close();

    ErrorCode::Result<AnalysisReport> loadResult;
    EXPECT_NO_THROW({
        loadResult = analyzer.loadAndReplace(filePath, ParserErrorAction::Throw);
    });
    std::remove(filePath.c_str());

    ASSERT_TRUE(loadResult.has_value());
    EXPECT_EQ(loadResult->status, ParseError::PARTIAL_FAILURE);
    ASSERT_FALSE(loadResult->parseErrors.empty());
}

TEST_F(LogAnalyzerTest, AppendDoesNotThrowWhenParserConfiguredToThrow) {
    LogAnalyzerSettings settings;
    settings.lineParsePattern = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) (\w+): (.*)$)";
    settings.fieldMappings = {
        FieldMapping{LogEntryField::TIMESTAMP, std::make_optional<size_t>(1), {"%Y-%m-%d %H:%M:%S"}},
        FieldMapping{LogEntryField::LEVEL, std::make_optional<size_t>(2), {}},
        FieldMapping{LogEntryField::MESSAGE, std::make_optional<size_t>(3), {}}
    };
    settings.parserErrorAction = ParserErrorAction::Throw;
    auto settingsResult = analyzer.setSettings(settings);
    ASSERT_TRUE(settingsResult.has_value()) << settingsResult.error().toString();

    const std::string seedFile = "test_append_throw_seed.log";
    {
        std::ofstream ofs(seedFile);
        ofs << "2023-01-01 10:00:00 INFO: seed line\n";
    }
    auto seedLoad = analyzer.loadAndReplace(seedFile, ParserErrorAction::Warn);
    std::remove(seedFile.c_str());
    ASSERT_TRUE(seedLoad.has_value()) << seedLoad.error().toString();

    const std::string appendFile = "test_append_throw.log";
    {
        std::ofstream ofs(appendFile);
        ofs << "bad line\n";
        ofs << "2023-01-01 10:00:01 INFO: valid append line\n";
    }

    ErrorCode::Result<AnalysisReport> appendResult;
    EXPECT_NO_THROW({
        appendResult = analyzer.append(appendFile, ParserErrorAction::Throw);
    });
    std::remove(appendFile.c_str());

    ASSERT_TRUE(appendResult.has_value());
    EXPECT_EQ(appendResult->status, ParseError::PARTIAL_FAILURE);

    const auto entries = analyzer.getEntriesSnapshot();
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].message, "seed line");
    EXPECT_EQ(entries[1].message, "valid append line");
}

TEST_F(LogAnalyzerTest, AnalyzeStreamDoesNotThrowWhenParserConfiguredToThrow) {
    LogAnalyzerSettings settings;
    settings.lineParsePattern = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) (\w+): (.*)$)";
    settings.fieldMappings = {
        FieldMapping{LogEntryField::TIMESTAMP, std::make_optional<size_t>(1), {"%Y-%m-%d %H:%M:%S"}},
        FieldMapping{LogEntryField::LEVEL, std::make_optional<size_t>(2), {}},
        FieldMapping{LogEntryField::MESSAGE, std::make_optional<size_t>(3), {}}
    };
    settings.parserErrorAction = ParserErrorAction::Throw;
    auto settingsResult = analyzer.setSettings(settings);
    ASSERT_TRUE(settingsResult.has_value()) << settingsResult.error().toString();

    const std::string filePath = "test_analyze_stream_throw.log";
    {
        std::ofstream ofs(filePath);
        ofs << "bad line\n";
        ofs << "2023-01-01 10:00:01 INFO: valid stream line\n";
    }

    size_t callbackCount = 0;
    ErrorCode::Result<void> streamResult;
    EXPECT_NO_THROW({
        streamResult = analyzer.analyzeStream({filePath}, [&](const LogEntry&) {
            ++callbackCount;
            return true;
        }, ParserErrorAction::Throw);
    });
    std::remove(filePath.c_str());

    ASSERT_TRUE(streamResult.has_value()) << streamResult.error().toString();
    EXPECT_EQ(callbackCount, 2u);
}

TEST_F(LogAnalyzerTest, SnapshotAccessorsReturnIndependentCopies) {
    const std::string filePath = "test_snapshot_access.log";
    std::ofstream ofs(filePath);
    ofs << "2023-01-01 10:00:00 INFO: first\n";
    ofs.close();

    auto firstLoad = analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
    ASSERT_TRUE(firstLoad.has_value());

    auto entriesSnapshot = analyzer.getEntriesSnapshot();
    auto reportSnapshot = analyzer.getLastReportSnapshot();
    ASSERT_EQ(entriesSnapshot.size(), 1);
    ASSERT_EQ(reportSnapshot.successfulParses, 1);

    std::ofstream ofs2(filePath, std::ios::trunc);
    ofs2 << "2023-01-01 10:00:01 INFO: second\n";
    ofs2 << "2023-01-01 10:00:02 INFO: third\n";
    ofs2.close();

    auto secondLoad = analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
    std::remove(filePath.c_str());
    ASSERT_TRUE(secondLoad.has_value());
    ASSERT_EQ(analyzer.getEntries().size(), 2);

    // Previous snapshots remain unchanged.
    EXPECT_EQ(entriesSnapshot.size(), 1);
    EXPECT_EQ(entriesSnapshot[0].message, "first");
    EXPECT_EQ(reportSnapshot.successfulParses, 1);
}

TEST_F(LogAnalyzerTest, ReferenceAccessorsReturnStableSnapshots) {
    const std::string filePath = "test_reference_snapshot.log";
    std::ofstream ofs(filePath);
    ofs << "2023-01-01 10:00:00 INFO: one\n";
    ofs.close();

    auto firstLoad = analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
    ASSERT_TRUE(firstLoad.has_value());

    const auto& entriesRefSnapshot = analyzer.getEntries();
    const auto& reportRefSnapshot = analyzer.getLastReport();
    ASSERT_EQ(entriesRefSnapshot.size(), 1);
    ASSERT_EQ(reportRefSnapshot.successfulParses, 1);

    std::ofstream ofs2(filePath, std::ios::trunc);
    ofs2 << "2023-01-01 10:00:01 INFO: two\n";
    ofs2 << "2023-01-01 10:00:02 INFO: three\n";
    ofs2.close();

    auto secondLoad = analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
    std::remove(filePath.c_str());
    ASSERT_TRUE(secondLoad.has_value());

    // Reference-returning accessors expose thread-local snapshots and remain stable.
    EXPECT_EQ(entriesRefSnapshot.size(), 1);
    EXPECT_EQ(entriesRefSnapshot[0].message, "one");
    EXPECT_EQ(reportRefSnapshot.successfulParses, 1);
}

TEST_F(LogAnalyzerTest, ConcurrentSnapshotAccessDuringLoad) {
    const std::string filePath = "test_concurrent_snapshot.log";
    std::ofstream ofs(filePath);
    for (int i = 0; i < 300; ++i) {
        ofs << "2023-01-01 10:00:" << (i % 60 < 10 ? "0" : "") << (i % 60)
            << " INFO: message-" << i << "\n";
    }
    ofs.close();

    auto futureLoad = std::async(std::launch::async, [&]() {
        return analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
    });

    while (futureLoad.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready) {
        auto snapshot = analyzer.getEntriesSnapshot();
        auto report = analyzer.getLastReportSnapshot();
        (void)snapshot.size();
        (void)report.linesProcessed;
    }

    auto result = futureLoad.get();
    std::remove(filePath.c_str());
    ASSERT_TRUE(result.has_value()) << result.error().toString();
    EXPECT_GE(analyzer.getEntriesSnapshot().size(), 250);
}

TEST_F(LogAnalyzerTest, ConcurrentLoadAsyncWithFilterAndExport) {
    const std::string filePath = "test_concurrent_async_filter_export.log";
    std::ofstream ofs(filePath);
    for (int i = 0; i < 1000; ++i) {
        ofs << "2023-01-01 12:00:" << (i % 60 < 10 ? "0" : "") << (i % 60)
            << " INFO: async-msg-" << i << "\n";
    }
    ofs.close();

    auto futureLoad = analyzer.loadAsync(filePath, ParserErrorAction::Warn);

    auto cond = FilterCondition::createString(LogEntryField::LEVEL, FilterOperator::NOT_EQUALS, "NONE");
    ASSERT_TRUE(cond.has_value());
    FilterExpression all(*cond);

    for (int i = 0; i < 500 && futureLoad.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready; ++i) {
        auto filtered = analyzer.getFilteredEntries(all);
        ASSERT_TRUE(filtered.has_value()) << filtered.error().toString();

        std::stringstream jsonOut;
        ASSERT_NO_THROW(analyzer.exportAsJson(jsonOut, all, false));
        ASSERT_NO_THROW((void)nlohmann::json::parse(jsonOut.str()));

        std::stringstream csvOut;
        ASSERT_NO_THROW(analyzer.exportAsCsv(csvOut, all, true));
        ASSERT_FALSE(csvOut.str().empty());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    auto result = futureLoad.get();
    std::remove(filePath.c_str());
    ASSERT_TRUE(result.has_value()) << result.error().toString();
    EXPECT_EQ(analyzer.getEntriesSnapshot().size(), 1000);
}

TEST_F(LogAnalyzerTest, ConcurrentAnalyzeStreamUsesIndependentParserState) {
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

    const std::string filePath1 = "test_stream_concurrent_1.log";
    const std::string filePath2 = "test_stream_concurrent_2.log";
    {
        std::ofstream ofs(filePath1);
        for (int i = 0; i < 100; ++i) {
            ofs << "2023-01-01 12:00:" << (i % 60 < 10 ? "0" : "") << (i % 60)
                << " INFO: entry-a-" << i << "\n";
            ofs << "  continuation-a-" << i << "\n";
        }
    }
    {
        std::ofstream ofs(filePath2);
        for (int i = 0; i < 100; ++i) {
            ofs << "2023-01-01 13:00:" << (i % 60 < 10 ? "0" : "") << (i % 60)
                << " INFO: entry-b-" << i << "\n";
            ofs << "  continuation-b-" << i << "\n";
        }
    }

    std::atomic<int> countA{0};
    std::atomic<int> countB{0};

    auto f1 = std::async(std::launch::async, [&]() {
        return analyzer.analyzeStream({filePath1}, [&](const LogEntry& entry) {
            if (entry.message.find("entry-a-") != std::string::npos) {
                ++countA;
            }
            return true;
        }, ParserErrorAction::Warn);
    });
    auto f2 = std::async(std::launch::async, [&]() {
        return analyzer.analyzeStream({filePath2}, [&](const LogEntry& entry) {
            if (entry.message.find("entry-b-") != std::string::npos) {
                ++countB;
            }
            return true;
        }, ParserErrorAction::Warn);
    });

    auto r1 = f1.get();
    auto r2 = f2.get();
    std::remove(filePath1.c_str());
    std::remove(filePath2.c_str());

    ASSERT_TRUE(r1.has_value()) << r1.error().toString();
    ASSERT_TRUE(r2.has_value()) << r2.error().toString();
    EXPECT_EQ(countA.load(), 100);
    EXPECT_EQ(countB.load(), 100);
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
        auto reportResult = analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
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
