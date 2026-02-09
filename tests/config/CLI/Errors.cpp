// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "tests/include/CLI.h"

TEST_F(CLIConfigTest, NoArgsReturnsError) {
    auto result = parse({"log_analyzer"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidArgument);
}

TEST_F(CLIConfigTest, StdinAndFileError) {
    auto result = parse({"log_analyzer", "--stdin", "dummy_log_file.log"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidArgument);
}

TEST_F(CLIConfigTest, OptionValueMissing) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--level"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption); // CLI11 throws RequiredError
}

TEST_F(CLIConfigTest, UnknownOptionError) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--unknown-option"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption); // CLI11 throws Error
}

TEST_F(CLIConfigTest, HelpOption) {
    auto result = parse({"log_analyzer", "--help"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_TRUE(options.exitAfterParse);
}

TEST_F(CLIConfigTest, VersionOption) {
    // CLI11's standard behavior for --version is to print the version and exit gracefully.
    // This test is updated to reflect that CLIConfig should not flag it as an error.
    auto result = parse({"log_analyzer", "--version"});
    ASSERT_TRUE(result.has_value()); // Expecting a successful parse or non-error outcome
    auto& [settings, options] = result.value();
    ASSERT_TRUE(options.exitAfterParse);
}

TEST_F(CLIConfigTest, DurationWithoutTimeBoundariesError) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--duration", "1h"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidArgument);
}

TEST_F(CLIConfigTest, OnParseErrorThrow) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--on-parse-error", "throw"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.parserErrorAction, CLIConfig::ParserErrorAction::Throw);
    ASSERT_EQ(settings.parserErrorAction, CLIConfig::ParserErrorAction::Throw);
}

TEST_F(CLIConfigTest, OnParseErrorWarn) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--on-parse-error", "warn"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.parserErrorAction, CLIConfig::ParserErrorAction::Warn);
    ASSERT_EQ(settings.parserErrorAction, CLIConfig::ParserErrorAction::Warn);
}

TEST_F(CLIConfigTest, ParserErrorActionCaseInsensitivity) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--on-parse-error", "IGNORE"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value(); // Use structured binding
    ASSERT_EQ(options.parserErrorAction, CLIConfig::ParserErrorAction::Ignore);
}

// Test for invalid enum value for --on-parse-error
TEST_F(CLIConfigTest, OnParseErrorInvalidEnum) {
    // Test with an unrecognized enum value to ensure robust error handling.
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--on-parse-error", "fail_silently"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption); // Expecting an option parsing error
}

// Test for invalid value for --level
TEST_F(CLIConfigTest, LevelOptionInvalidValue) {
    // Test with an invalid string value for the --level option.
    // Assumes --level expects specific keywords like "info", "debug", etc.
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--level", "debugg"}); // "debugg" is likely invalid
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption); // Expecting an option parsing error
}

// Test for invalid duration format
TEST_F(CLIConfigTest, DurationInvalidFormat) {
    // Test with an invalid format for the --duration option.
    // The existing DurationWithoutTimeBoundariesError test covers duration alone being an error.
    // This test covers an invalid string format for duration.
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--duration", "invalid-time-format"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidArgument); // Following the pattern of DurationWithoutTimeBoundariesError
}

TEST_F(CLIConfigTest, TailIntervalMustBePositive) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--tail", "--tail-interval", "-10"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption);
}

TEST_F(CLIConfigTest, StatsWindowMustBePositive) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--stats-window", "0"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption);
}

TEST_F(CLIConfigTest, FindGapsMustBePositive) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--find-gaps", "-1"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption);
}

TEST_F(CLIConfigTest, CsvFieldsRejectEmptyFieldName) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--csv-fields", "level, ,message"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption);
}

TEST_F(CLIConfigTest, JsonFieldsRejectEmptyFieldName) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--json-fields", "timestamp,,level"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption);
}

TEST_F(CLIConfigTest, CsvFieldsRejectMissingAliasAfterAs) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--csv-fields", "level as "});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption);
}

TEST_F(CLIConfigTest, JsonFieldsRejectMissingAliasAfterAs) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--json-fields", "timestamp as "});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption);
}
