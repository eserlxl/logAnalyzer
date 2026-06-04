// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "cli_helper.h"
#include "filter/types.h"
#include "core/log/types.h" // For LogLevel
#include "utils/time.h" // For Utils::parseTime

using namespace filter;

TEST_F(CLIConfigTest, ParseLogLevel) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--level", "INFO"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.filterLevels.size(), 1);
    ASSERT_EQ(options.filterLevels[0], LogLevel::INFO);
}

TEST_F(CLIConfigTest, ParseMultipleLogLevels) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--level", "INFO", "--level", "DEBUG"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.filterLevels.size(), 2);
    ASSERT_EQ(options.filterLevels[0], LogLevel::INFO);
    ASSERT_EQ(options.filterLevels[1], LogLevel::DEBUG);
}

TEST_F(CLIConfigTest, InvalidLogLevel) {
    auto result = parse({"log_analyzer", "--level", "INVALID", "dummy_log_file.log"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption);
}

TEST_F(CLIConfigTest, FilterKeyword) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--keyword", "error"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.filterKeywords.size(), 1);
    ASSERT_EQ(options.filterKeywords[0], "error");
}

TEST_F(CLIConfigTest, FilterMultipleKeywords) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--keyword", "error", "--keyword", "warn"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.filterKeywords.size(), 2);
    ASSERT_EQ(options.filterKeywords[0], "error");
    ASSERT_EQ(options.filterKeywords[1], "warn");
}

TEST_F(CLIConfigTest, ExcludeKeyword) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--exclude-keyword", "debug"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.excludeKeywords.size(), 1);
    ASSERT_EQ(options.excludeKeywords[0], "debug");
}

TEST_F(CLIConfigTest, KeywordCaseSensitive) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--keyword", "error", "--case-sensitive"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.keywordCaseSensitive);
}

TEST_F(CLIConfigTest, FilterRegex) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--regex", ".*ERROR.*"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.regexPatterns.size(), 1);
    ASSERT_EQ(options.regexPatterns[0], ".*ERROR.*");
}

TEST_F(CLIConfigTest, ExcludeRegex) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--exclude-regex", ".*DEBUG.*"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.excludeRegexPatterns.size(), 1);
    ASSERT_EQ(options.excludeRegexPatterns[0], ".*DEBUG.*");
}

TEST_F(CLIConfigTest, FilterLogic) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--logic", "OR"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.filterLogic.has_value());
    ASSERT_EQ(options.filterLogic.value(), FilterLogicalOperator::OR);
}

TEST_F(CLIConfigTest, ComplexFilterExpression) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--expression", "level == INFO AND message contains 'user'"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.complexFilterExpression, "level == INFO AND message contains 'user'");
}

TEST_F(CLIConfigTest, FilterStartTimeISO) {
    std::string time_str = "2023-01-01 10:00:00";
    auto expectedTime = Utils::parseTime(time_str).value();
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--start", time_str});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.startTime.has_value());
    ASSERT_EQ(options.startTime.value(), expectedTime);
}

TEST_F(CLIConfigTest, FilterEndTimeRelative) {
    std::string time_str = "1 hour ago";
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--end", time_str});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.endTime.has_value());

    auto now = std::chrono::system_clock::now();
    ASSERT_LT(options.endTime.value(), now);
    ASSERT_GT(options.endTime.value(), now - std::chrono::hours(2));
}

TEST_F(CLIConfigTest, FilterDurationWithStartTime) {
    std::string start_time_str = "2023-01-01 00:00:00";
    std::string duration_str = "1h";
    auto expectedStartTime = Utils::parseTime(start_time_str).value();
    auto expectedEndTime = expectedStartTime + std::chrono::hours(1);

    auto result = parse({"log_analyzer", "dummy_log_file.log", "--start", start_time_str, "--duration", duration_str});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    
    ASSERT_TRUE(options.startTime.has_value());
    ASSERT_EQ(options.startTime.value(), expectedStartTime);
    ASSERT_TRUE(options.endTime.has_value());
    ASSERT_EQ(options.endTime.value(), expectedEndTime);
    ASSERT_TRUE(options.duration.has_value());
    ASSERT_EQ(options.duration.value(), std::chrono::hours(1));
}

TEST_F(CLIConfigTest, LogLevelCaseInsensitivity) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--level", "debug", "--level", "WARNING"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.filterLevels.size(), 2);
    ASSERT_EQ(options.filterLevels[0], LogLevel::DEBUG);
    ASSERT_EQ(options.filterLevels[1], LogLevel::WARNING);
}

TEST_F(CLIConfigTest, CustomLogLevelMapping) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--map-level", "CRITICAL=FATAL", "--map-level", "VERBOSE=DEBUG"});
    ASSERT_TRUE(result.has_value());
    auto& settings = result.value().first;
    ASSERT_EQ(settings.customLogLevelMappings.size(), 2);
    ASSERT_EQ(settings.customLogLevelMappings.at("CRITICAL"), LogLevel::FATAL);
    ASSERT_EQ(settings.customLogLevelMappings.at("VERBOSE"), LogLevel::DEBUG);
}

TEST_F(CLIConfigTest, CustomLogLevelMappingTrimsWhitespace) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--map-level", "  CRITICAL  =  fatal  "});
    ASSERT_TRUE(result.has_value());
    auto& settings = result.value().first;
    ASSERT_EQ(settings.customLogLevelMappings.size(), 1);
    ASSERT_EQ(settings.customLogLevelMappings.at("CRITICAL"), LogLevel::FATAL);
}

TEST_F(CLIConfigTest, LimitOption) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--limit", "2"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.limit.has_value());
    ASSERT_EQ(*options.limit, 2u);
}

TEST_F(CLIConfigTest, OffsetOption) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--offset", "5"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.offset.has_value());
    ASSERT_EQ(*options.offset, 5u);
}

TEST_F(CLIConfigTest, OffsetZeroIsValid) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--offset", "0"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.offset.has_value());
    ASSERT_EQ(*options.offset, 0u);
}

TEST_F(CLIConfigTest, SinceOption) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--since", "1h"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.startTime.has_value());
    auto now = std::chrono::system_clock::now();
    ASSERT_LT(options.startTime.value(), now);
    ASSERT_GT(options.startTime.value(), now - std::chrono::hours(2));
}

TEST_F(CLIConfigTest, SinceConflictsWithStart) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--since", "1h", "--start", "2023-01-01 00:00:00"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption);
}

TEST_F(CLIConfigTest, DedupFieldOption) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--dedup-field", "session_id"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.dedupField, "session_id");
}

TEST_F(CLIConfigTest, MinLevel) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--min-level", "WARNING"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.minLogLevel.has_value());
    ASSERT_EQ(options.minLogLevel.value(), LogLevel::WARNING);
}

TEST_F(CLIConfigTest, MinLevelCaseInsensitive) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--min-level", "warning"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.minLogLevel.has_value());
    ASSERT_EQ(options.minLogLevel.value(), LogLevel::WARNING);
}

TEST_F(CLIConfigTest, MaxLevel) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--max-level", "WARNING"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.maxLogLevel.has_value());
    ASSERT_EQ(options.maxLogLevel.value(), LogLevel::WARNING);
}

TEST_F(CLIConfigTest, MaxLevelCaseInsensitive) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--max-level", "warning"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.maxLogLevel.has_value());
    ASSERT_EQ(options.maxLogLevel.value(), LogLevel::WARNING);
}

TEST_F(CLIConfigTest, MinLevelInvalid) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--min-level", "INVALID"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption);
}

TEST_F(CLIConfigTest, MaxLevelInvalid) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--max-level", "INVALID"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption);
}

TEST_F(CLIConfigTest, MinLevelMaxLevelRange) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--min-level", "INFO", "--max-level", "WARNING"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.minLogLevel.has_value());
    ASSERT_EQ(options.minLogLevel.value(), LogLevel::INFO);
    ASSERT_TRUE(options.maxLogLevel.has_value());
    ASSERT_EQ(options.maxLogLevel.value(), LogLevel::WARNING);
}
