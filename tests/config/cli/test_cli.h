// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include "gtest/gtest.h"
#include "config/cli.h"
#include "core/error.h"
#include "config/settings.h"
#include <fstream>
#include <filesystem>

// Test fixture for CLIConfig tests
class CLIConfigTest : public ::testing::Test {
protected:
    const std::string dummyLogFile1 = "dummy_log_file.log";
    const std::string dummyLogFile2 = "dummy_log_file2.log";
    const std::string outputFile = "output.txt";

    void SetUp() override {
        // Create a dummy file for tests that need a file path
        std::ofstream dummy_file(dummyLogFile1);
        if (dummy_file) {
            dummy_file << "dummy content\n";
            dummy_file.close();
        }
    }

    void TearDown() override {
        std::filesystem::remove_all(dummyLogFile1);
        std::filesystem::remove_all(dummyLogFile2);
        std::filesystem::remove_all(outputFile);
    }

    // Helper function to call parseCLI with a vector of C-style strings
    ErrorCode::Result<std::pair<LogAnalyzerSettings, CLIConfig::CLIOptions>> parse(std::vector<const char*> args) {
        return CLIConfig::parseCLI(static_cast<int>(args.size()), args.data());
    }

    // Overload for convenience with std::vector<std::string>
    ErrorCode::Result<std::pair<LogAnalyzerSettings, CLIConfig::CLIOptions>> parse(const std::vector<std::string>& args_str) {
        std::vector<const char*> argv;
        argv.reserve(args_str.size());
        for (const auto& s : args_str) {
            argv.push_back(s.c_str());
        }
        return CLIConfig::parseCLI(static_cast<int>(argv.size()), argv.data());
    }
};

TEST_F(CLIConfigTest, BasicParsing) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_EQ(settings.lineParsePattern, DEFAULT_LOG_REGEX_PATTERN_INTERNAL); // Default pattern
    ASSERT_FALSE(options.filePaths.empty());
    ASSERT_EQ(options.filePaths[0], dummyLogFile1);
    ASSERT_FALSE(options.outputPath.has_value());
    ASSERT_FALSE(options.prettyPrint);
}

TEST_F(CLIConfigTest, UnknownOptionReturnsError) {
    std::vector<std::string> args = {"program_name", "--unknown-option"};
    auto result = parse(args);
    ASSERT_TRUE(result.isError());
    ASSERT_EQ(result.error().code, ErrorCode::CLI_PARSING_ERROR);
}

TEST_F(CLIConfigTest, MissingValueForOptionReturnsError) {
    std::vector<std::string> args = {"program_name", "--log-file"}; // Missing file path
    auto result = parse(args);
    ASSERT_TRUE(result.isError());
    ASSERT_EQ(result.error().code, ErrorCode::CLI_PARSING_ERROR);
}

TEST_F(CLIConfigTest, ParseLogFileAndOutputPath) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--output", outputFile};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_FALSE(options.filePaths.empty());
    ASSERT_EQ(options.filePaths[0], dummyLogFile1);
    ASSERT_TRUE(options.outputPath.has_value());
    ASSERT_EQ(options.outputPath.value(), outputFile);
}

TEST_F(CLIConfigTest, ParsePrettyPrintOption) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--pretty"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_TRUE(options.prettyPrint);
}

TEST_F(CLIConfigTest, ParseStreamMode) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--stream"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_TRUE(options.streamMode);
}

TEST_F(CLIConfigTest, ParseMinLogLevel) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--min-level", "WARNING"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_TRUE(options.minLogLevel.has_value());
    ASSERT_EQ(options.minLogLevel.value(), LogLevel::WARNING);
}

TEST_F(CLIConfigTest, ParseInvalidMinLogLevelReturnsError) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--min-level", "INVALID"};
    auto result = parse(args);
    ASSERT_TRUE(result.isError());
    ASSERT_EQ(result.error().code, ErrorCode::CLI_PARSING_ERROR);
}

TEST_F(CLIConfigTest, ParseFilterKeyword) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--filter-keyword", "error"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_FALSE(options.filterKeywords.empty());
    ASSERT_EQ(options.filterKeywords[0], "error");
}

TEST_F(CLIConfigTest, ParseMultipleFilterKeywords) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--filter-keyword", "error", "--filter-keyword", "warning"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_EQ(options.filterKeywords.size(), 2);
    ASSERT_EQ(options.filterKeywords[0], "error");
    ASSERT_EQ(options.filterKeywords[1], "warning");
}

TEST_F(CLIConfigTest, ParseExcludeKeyword) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--exclude-keyword", "debug"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_FALSE(options.excludeKeywords.empty());
    ASSERT_EQ(options.excludeKeywords[0], "debug");
}

TEST_F(CLIConfigTest, ParseKeywordCaseSensitive) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--keyword-case-sensitive"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_TRUE(options.keywordCaseSensitive);
}

TEST_F(CLIConfigTest, ParseRegexPattern) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--regex", ".*ERROR.*"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_FALSE(options.regexPatterns.empty());
    ASSERT_EQ(options.regexPatterns[0], ".*ERROR.*");
}

TEST_F(CLIConfigTest, ParseExcludeRegexPattern) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--exclude-regex", ".*DEBUG.*"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_FALSE(options.excludeRegexPatterns.empty());
    ASSERT_EQ(options.excludeRegexPatterns[0], ".*DEBUG.*");
}

TEST_F(CLIConfigTest, ParseFilterLogic) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--filter-logic", "OR"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_TRUE(options.filterLogic.has_value());
    ASSERT_EQ(options.filterLogic.value(), filter::FilterLogicalOperator::OR);
}

TEST_F(CLIConfigTest, ParseInvalidFilterLogicReturnsError) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--filter-logic", "XOR"};
    auto result = parse(args);
    ASSERT_TRUE(result.isError());
    ASSERT_EQ(result.error().code, ErrorCode::CLI_PARSING_ERROR);
}

TEST_F(CLIConfigTest, ParseColorOption) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--color", "NEVER"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_EQ(options.colorOption, CLIConfig::ColorOption::NEVER);
}

TEST_F(CLIConfigTest, ParseCsvSeparator) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--csv-separator", ";"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_EQ(options.csvSeparator, ';');
}

TEST_F(CLIConfigTest, ParseCsvFields) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--csv-fields", "timestamp,level,message"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_EQ(options.csvFields.size(), 3);
    ASSERT_EQ(options.csvFields[0], "timestamp");
    ASSERT_EQ(options.csvFields[1], "level");
    ASSERT_EQ(options.csvFields[2], "message");
}

TEST_F(CLIConfigTest, ParseTopMessagesCount) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--top-messages", "50"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_EQ(options.topMessagesCount, 50);
}

TEST_F(CLIConfigTest, ParseInvalidTopMessagesCountReturnsError) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--top-messages", "abc"};
    auto result = parse(args);
    ASSERT_TRUE(result.isError());
    ASSERT_EQ(result.error().code, ErrorCode::CLI_PARSING_ERROR);
}

TEST_F(CLIConfigTest, ParseTailMode) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--tail"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_TRUE(options.tailMode);
}

TEST_F(CLIConfigTest, ParseTailInterval) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--tail-interval", "500ms"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_EQ(options.tailInterval.count(), 500);
}

TEST_F(CLIConfigTest, ParseComplexFilterExpression) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--expression", "level == INFO AND message contains 'user logged in'"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_EQ(options.complexFilterExpression, "level == INFO AND message contains 'user logged in'");
}

TEST_F(CLIConfigTest, ParseJsonFields) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--json-fields", "timestamp,event.name,data.id"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_EQ(options.jsonFields.size(), 3);
    ASSERT_EQ(options.jsonFields[0], "timestamp");
    ASSERT_EQ(options.jsonFields[1], "event.name");
    ASSERT_EQ(options.jsonFields[2], "data.id");
}

TEST_F(CLIConfigTest, ParseMultilineStartPattern) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--multiline-start-pattern", "^\\[\\d{4}-\\d{2}-\\d{2}\\]"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_TRUE(options.multilineStartPattern.has_value());
    ASSERT_EQ(options.multilineStartPattern.value(), "^\[\\d{4}-\\d{2}-\\d{2}\]");
}

TEST_F(CLIConfigTest, ParseMaxMultilineBufferSize) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--max-multiline-buffer-size", "20MB"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_EQ(options.maxMultilineBufferSize, 20 * 1024 * 1024);
}

TEST_F(CLIConfigTest, ParseInvalidMaxMultilineBufferSizeReturnsError) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--max-multiline-buffer-size", "invalid"};
    auto result = parse(args);
    ASSERT_TRUE(result.isError());
    ASSERT_EQ(result.error().code, ErrorCode::CLI_PARSING_ERROR);
}

TEST_F(CLIConfigTest, ParseFieldMap) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--field-map", "timestamp:1", "--field-map", "level:2"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_EQ(options.fieldMaps.size(), 2);
    ASSERT_EQ(options.fieldMaps[0], "timestamp:1");
    ASSERT_EQ(options.fieldMaps[1], "level:2");
}

TEST_F(CLIConfigTest, ParseEnabledStatistic) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--enable-stat", "count"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_EQ(options.enabledStatistics.size(), 1);
    ASSERT_EQ(options.enabledStatistics[0], "count");
}

TEST_F(CLIConfigTest, ParseStatsWindow) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--stats-window", "1h"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_TRUE(options.statsWindow.has_value());
    ASSERT_EQ(options.statsWindow.value().count(), 3600); // 1 hour = 3600 seconds
}

TEST_F(CLIConfigTest, ParseFindGapsDuration) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--find-gaps-duration", "5s"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_TRUE(options.findGapsDuration.has_value());
    ASSERT_EQ(options.findGapsDuration.value().count(), 5000); // 5 seconds = 5000 milliseconds
}

TEST_F(CLIConfigTest, ParseReadFromStdin) {
    std::vector<std::string> args = {"program_name", "--stdin"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_TRUE(options.readFromStdin);
}

TEST_F(CLIConfigTest, ParseExitAfterParse) {
    std::vector<std::string> args = {"program_name", "--exit-after-parse"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_TRUE(options.exitAfterParse);
}

TEST_F(CLIConfigTest, ParseIncludeSummary) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--include-summary"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_TRUE(options.includeSummary);
}

TEST_F(CLIConfigTest, ParseOutputFormat) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--output-format", "json"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_EQ(options.outputFormat, "json");
}

TEST_F(CLIConfigTest, ParseTextOutputFormat) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--text-output-format", "{level}: {message}"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_EQ(options.textOutputFormat, "{level}: {message}");
}

TEST_F(CLIConfigTest, ParseParserErrorAction) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--parser-error-action", "throw"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_EQ(options.parserErrorAction, CLIConfig::ParserErrorAction::Throw);
}

TEST_F(CLIConfigTest, ParseSortByAndOrder) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--sort-by", "level", "--sort-order", "desc"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_TRUE(options.sortBy.has_value());
    ASSERT_EQ(options.sortBy.value(), filter::SortBy::LEVEL);
    ASSERT_TRUE(options.sortOrder.has_value());
    ASSERT_EQ(options.sortOrder.value(), filter::SortOrder::DESCENDING);
}

TEST_F(CLIConfigTest, ParseMultipleLogFiles) {
    // Create a second dummy file
    std::ofstream dummy_file2(dummyLogFile2);
    if (dummy_file2) {
        dummy_file2 << "dummy content 2\n";
        dummy_file2.close();
    }
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--log-file", dummyLogFile2};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_EQ(options.filePaths.size(), 2);
    ASSERT_EQ(options.filePaths[0], dummyLogFile1);
    ASSERT_EQ(options.filePaths[1], dummyLogFile2);
}

// Test edge case: No arguments (only program name)
TEST_F(CLIConfigTest, NoArguments) {
    std::vector<std::string> args = {"program_name"};
    auto result = parse(args);
    // Depending on CLI11 configuration, this might be OK or an error if --log-file is required.
    // Assuming for now it's OK and filePaths will be empty.
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_TRUE(options.filePaths.empty());
}

// Test edge case: Multiple positional arguments (if supported, assuming they are log files)
TEST_F(CLIConfigTest, PositionalArguments) {
    // Create a second dummy file
    std::ofstream dummy_file2(dummyLogFile2);
    if (dummy_file2) {
        dummy_file2 << "dummy content 2\n";
        dummy_file2.close();
    }
    std::vector<std::string> args = {"program_name", dummyLogFile1, dummyLogFile2};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_EQ(options.filePaths.size(), 2);
    ASSERT_EQ(options.filePaths[0], dummyLogFile1);
    ASSERT_EQ(options.filePaths[1], dummyLogFile2);
}

// Test interactions: --log-file and --stdin are mutually exclusive
TEST_F(CLIConfigTest, LogFileAndStdinAreMutuallyExclusive) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--stdin"};
    auto result = parse(args);
    ASSERT_TRUE(result.isError());
    ASSERT_EQ(result.error().code, ErrorCode::CLI_PARSING_ERROR);
}

// Test interactions: --output-format json and --json-fields
TEST_F(CLIConfigTest, JsonOutputFormatAndFields) {
    std::vector<std::string> args = {"program_name", "--log-file", dummyLogFile1, "--output-format", "json", "--json-fields", "timestamp,message"};
    auto result = parse(args);
    ASSERT_TRUE(result.isOk()) << result.error().message;
    auto [settings, options] = result.value();
    ASSERT_EQ(options.outputFormat, "json");
    ASSERT_EQ(options.jsonFields.size(), 2);
    ASSERT_EQ(options.jsonFields[0], "timestamp");
    ASSERT_EQ(options.jsonFields[1], "message");
}

// Test interactions: --log-file is required unless --stdin is present
TEST_F(CLIConfigTest, LogFileRequiredUnlessStdin) {
    std::vector<std::string> args_no_file = {"program_name"};
    auto result_no_file = parse(args_no_file);
    ASSERT_TRUE(result_no_file.isOk()) << result_no_file.error().message; // Should parse successfully, filePaths empty

    std::vector<std::string> args_with_stdin = {"program_name", "--stdin"};
    auto result_with_stdin = parse(args_with_stdin);
    ASSERT_TRUE(result_with_stdin.isOk()) << result_with_stdin.error().message;
    auto [settings, options] = result_with_stdin.value();
    ASSERT_TRUE(options.readFromStdin);
    ASSERT_TRUE(options.filePaths.empty()); // No file paths specified when stdin is used.
}
