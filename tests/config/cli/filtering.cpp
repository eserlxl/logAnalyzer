// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "tests/include/cli.h"
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
    auto expected_time = Utils::parseTime(time_str).value();
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--start", time_str.c_str()});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.startTime.has_value());
    ASSERT_EQ(options.startTime.value(), expected_time);
}

TEST_F(CLIConfigTest, FilterEndTimeRelative) {
    std::string time_str = "1 hour ago";
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--end", time_str.c_str()});
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
    auto expected_start_time = Utils::parseTime(start_time_str).value();
    auto expected_end_time = expected_start_time + std::chrono::hours(1);

    auto result = parse({"log_analyzer", "dummy_log_file.log", "--start", start_time_str.c_str(), "--duration", duration_str.c_str()});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    
    ASSERT_TRUE(options.startTime.has_value());
    ASSERT_EQ(options.startTime.value(), expected_start_time);
    ASSERT_TRUE(options.endTime.has_value());
    ASSERT_EQ(options.endTime.value(), expected_end_time);
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
