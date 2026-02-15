// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "analyzer/core.h"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <future>
#include <atomic>
#include <thread>
#include <span>

using namespace filter;

class LogAnalyzerStreamTest : public ::testing::Test {
protected:
    LogAnalyzer analyzer;
};

TEST_F(LogAnalyzerStreamTest, AppendCorrectness) {
    std::string logContent1 = "2023-01-01 10:00:00 INFO: Entry 1\n";
    std::string filePath1 = "test_append_1.log";
    std::ofstream ofs1(filePath1);
    ofs1 << logContent1;
    ofs1.close();
    auto result1 = analyzer.append(filePath1, ParserErrorAction::Warn);
    ASSERT_TRUE(result1.has_value());
    std::remove(filePath1.c_str());
}

TEST_F(LogAnalyzerStreamTest, AppendUpdatesLastReportSnapshot) {
    const std::string filePath = "test_append_last_report.log";
    {
        std::ofstream ofs(filePath);
        ofs << "2023-01-01 10:00:00 INFO: Entry 1\n";
        ofs << "2023-01-01 10:01:00 ERROR: Entry 2\n";
    }

    auto appendResult = analyzer.append(filePath, ParserErrorAction::Warn);
    std::remove(filePath.c_str());
    ASSERT_TRUE(appendResult.has_value()) << appendResult.error().toString();

    const auto reportSnapshot = analyzer.getLastReportSnapshot();
    EXPECT_EQ(reportSnapshot.successfulParses, appendResult->successfulParses);
    EXPECT_EQ(reportSnapshot.status, appendResult->status);
}

TEST_F(LogAnalyzerStreamTest, AppendUpdatesStatisticsWhenAnalyzerStartsEmpty) {
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

TEST_F(LogAnalyzerStreamTest, StreamInUpdatesStatisticsWhenAnalyzerStartsEmpty) {
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

TEST_F(LogAnalyzerStreamTest, StreamInUpdatesLastReportSnapshot) {
    std::stringstream ss;
    ss << "2023-01-01 10:00:00 INFO: Stream entry 1\n";
    ss << "2023-01-01 10:01:00 WARNING: Stream entry 2\n";

    auto streamResult = analyzer.streamIn(ss, "report_stream", ParserErrorAction::Warn);
    ASSERT_TRUE(streamResult.has_value()) << streamResult.error().toString();

    const auto reportSnapshot = analyzer.getLastReportSnapshot();
    EXPECT_EQ(reportSnapshot.successfulParses, streamResult->successfulParses);
    EXPECT_EQ(reportSnapshot.status, streamResult->status);
}

TEST_F(LogAnalyzerStreamTest, ConcurrentAppendAndFilterIsStable) {
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
        nlohmann::json j;
        ASSERT_NO_THROW(j = nlohmann::json::parse(ss.str()));
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

TEST_F(LogAnalyzerStreamTest, ConcurrentAppendFromTwoThreadsPreservesAllEntries) {
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

TEST_F(LogAnalyzerStreamTest, ConcurrentAnalyzeStreamUsesIndependentParserState) {
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

TEST_F(LogAnalyzerStreamTest, AppendDoesNotThrowWhenParserConfiguredToThrow) {
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

TEST_F(LogAnalyzerStreamTest, AnalyzeStreamDoesNotThrowWhenParserConfiguredToThrow) {
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
