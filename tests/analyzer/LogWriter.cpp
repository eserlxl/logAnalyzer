// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "analyzer/Log/Writer.h"
#include "analyzer/Core.h"
#include "core/Error.h"
#include "utils/String.h"
#include "config/Core.h"
#include <string>
#include <sstream>
#include <vector>
#include <chrono>
#include <expected>

using namespace filter;

// Test fixture for LogWriter tests
class LogWriterTest : public ::testing::Test {
protected:
    std::unique_ptr<LogAnalyzer> analyzer;
    std::unique_ptr<LogWriter> logWriter;

    void SetUp() override {
        // Configure Analyzer to parse our test data
        LogAnalyzerSettings settings;
        // Simple pattern: Date Time Level Message
        // e.g. "2023-03-15 00:00:00 INFO User logged in."
        settings.lineParsePattern = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) (\w+) (.*)$)";
        
        settings.fieldMappings.clear();
        settings.fieldMappings.emplace_back(LogEntryField::TIMESTAMP, std::make_optional<size_t>(1), std::vector<std::string>{"%Y-%m-%d %H:%M:%S"});
        settings.fieldMappings.emplace_back(LogEntryField::LEVEL, std::make_optional<size_t>(2));
        settings.fieldMappings.emplace_back(LogEntryField::MESSAGE, std::make_optional<size_t>(3));

        analyzer = std::make_unique<LogAnalyzer>(settings);
        
        // Create test data
        std::stringstream ss;
        ss << "2023-03-15 00:00:00 INFO User logged in.\n";
        ss << "2023-03-15 00:01:00 WARNING Disk space low.\n";
        ss << "2023-03-15 00:02:00 ERROR Failed to connect.\n";
        ss << "2023-03-15 00:03:00 DEBUG Debug message.\n";

        // Load data into analyzer
        auto result = analyzer->streamIn(ss, "test_stream", CLIConfig::ParserErrorAction::Ignore);
        ASSERT_TRUE(result.has_value()) << "Failed to parse test data: " << result.error().toString();
        
        // Verify entries count
        ASSERT_EQ(analyzer->getEntries().size(), 4);

        logWriter = std::make_unique<LogWriter>(*analyzer);
    }
};

// --- LogWriter::formatEntry Tests ---

TEST_F(LogWriterTest, FormatEntry_DefaultOptions) {
    FormattingOptions options;
    LogEntry entry;
    
    // Set a fixed time (assuming UTC or system-independent enough for substring match)
    std::tm tm = {};
    tm.tm_year = 2023 - 1900;
    tm.tm_mon = 2; // March
    tm.tm_mday = 15;
    tm.tm_hour = 10;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    entry.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    entry.level = LogLevel::INFO;
    entry.message = "Test message";
    
    std::string result = logWriter->formatEntry(entry, options);
    EXPECT_THAT(result, testing::HasSubstr("INFO: Test message"));
    // Since mktime depends on local TZ, we just check for parts of it or the whole string if it matches
    // But formatEntry uses Utils::formatTimestamp.
}

TEST_F(LogWriterTest, FormatEntry_WithColor) {
    FormattingOptions options;
    options.useColor = true;
    
    LogEntry entry;
    entry.level = LogLevel::ERROR;
    entry.message = "Error occurred";
    
    std::string result = logWriter->formatEntry(entry, options);
    // Expect ANSI codes for RED (31)
    EXPECT_THAT(result, testing::HasSubstr("\033[31mERROR\033[0m"));
    EXPECT_THAT(result, testing::HasSubstr("Error occurred"));
}

TEST_F(LogWriterTest, FormatEntry_CustomFormat) {
    FormattingOptions options;
    options.overallFormat = "[{level}] {message}";
    
    LogEntry entry;
    entry.level = LogLevel::DEBUG;
    entry.message = "Debug info";
    
    std::string result = logWriter->formatEntry(entry, options);
    EXPECT_EQ(result, "[DEBUG] Debug info");
}

// --- LogWriter::printFilteredEntries Tests ---

TEST_F(LogWriterTest, PrintFilteredEntries_All) {
    std::stringstream ss;
    FilterCriteria criteria; // Empty matches all
    FormattingOptions options;
    options.overallFormat = "{level} {message}";
    
    logWriter->printFilteredEntries(ss, criteria, options);
    
    std::string output = ss.str();
    EXPECT_THAT(output, testing::HasSubstr("INFO User logged in."));
    EXPECT_THAT(output, testing::HasSubstr("WARNING Disk space low."));
    EXPECT_THAT(output, testing::HasSubstr("ERROR Failed to connect."));
    EXPECT_THAT(output, testing::HasSubstr("DEBUG Debug message."));
}

TEST_F(LogWriterTest, PrintFilteredEntries_FilteredByLevel) {
    std::stringstream ss;
    FilterCriteria criteria;
    // Add WARNING, ERROR, CRITICAL, FATAL
    criteria.levels = {LogLevel::WARNING, LogLevel::ERROR, LogLevel::CRITICAL, LogLevel::FATAL};
    
    FormattingOptions options;
    options.overallFormat = "{level} {message}";
    
    logWriter->printFilteredEntries(ss, criteria, options);
    
    std::string output = ss.str();
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("INFO User logged in.")));
    EXPECT_THAT(output, testing::HasSubstr("WARNING Disk space low."));
    EXPECT_THAT(output, testing::HasSubstr("ERROR Failed to connect."));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("DEBUG Debug message.")));
}

TEST_F(LogWriterTest, PrintFilteredEntries_FilteredByKeyword) {
    std::stringstream ss;
    FilterCriteria criteria;
    criteria.keyword = "connect";
    
    FormattingOptions options;
    options.overallFormat = "{level} {message}";
    
    logWriter->printFilteredEntries(ss, criteria, options);
    
    std::string output = ss.str();
    EXPECT_THAT(output, testing::HasSubstr("ERROR Failed to connect."));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("INFO User logged in.")));
}

TEST_F(LogWriterTest, PrintFilteredEntries_OverloadStringFormat) {
    std::stringstream ss;
    FilterCriteria criteria;
    criteria.keyword = "logged";
    
    logWriter->printFilteredEntries(ss, criteria, "MATCH: {message}");
    
    std::string output = ss.str();
    EXPECT_THAT(output, testing::HasSubstr("MATCH: User logged in."));
}
