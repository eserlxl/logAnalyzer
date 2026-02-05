// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 eserlxl

#include "tests/config/CLIConfigTest.h"

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
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption); // CLI11 throws CallForHelp
}

TEST_F(CLIConfigTest, VersionOption) {
    // Assuming --version exists and behaves like --help for now, might need adjustment
    // if CLIConfig handles --version differently (e.g., custom callback)
    auto result = parse({"log_analyzer", "--version"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption); // CLI11 throws CallForHelp if version is not explicitly handled
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
    auto& options = result.value().second;
    ASSERT_EQ(options.parserErrorAction, CLIConfig::ParserErrorAction::Ignore);
}
