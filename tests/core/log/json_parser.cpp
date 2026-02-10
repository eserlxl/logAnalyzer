// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "core/log/jsonparser.h"

TEST(JsonLogParserTest, ParseValidJsonLine) {
    JsonLogParser parser({}, ParserErrorAction::Warn, std::nullopt);
    const std::string line = R"({"timestamp":"2023-01-01 10:00:00","level":"INFO","message":"hello","request_id":"abc-123"})";

    auto result = parser.parseLine(line, 7, "json.log");
    ASSERT_TRUE(result.has_value()) << result.error().toString();
    const auto& entry = result.value();
    ASSERT_FALSE(entry.hasParsingErrors());
    EXPECT_EQ(entry.level, LogLevel::INFO);
    EXPECT_EQ(entry.message, "hello");
    EXPECT_EQ(entry.sourceLineNumber, 7);
    EXPECT_EQ(entry.sourceFile, "json.log");
    ASSERT_TRUE(entry.customFields.contains("request_id"));
    EXPECT_EQ(entry.customFields.at("request_id"), "\"abc-123\"");
}

TEST(JsonLogParserTest, WarnModeReturnsErrorEntryForMalformedJson) {
    JsonLogParser parser({}, ParserErrorAction::Warn, std::nullopt);
    const std::string line = R"({"timestamp":"2023-01-01 10:00:00","level":"INFO","message":"missing brace")";

    auto result = parser.parseLine(line, 10, "json.log");
    ASSERT_TRUE(result.has_value());
    const auto& entry = result.value();
    EXPECT_EQ(entry.level, LogLevel::UNKNOWN);
    EXPECT_TRUE(entry.hasParsingErrors());
    ASSERT_FALSE(entry.parsingErrors.empty());
    EXPECT_EQ(entry.parsingErrors.front().code, Code::JsonParseError);
}

TEST(JsonLogParserTest, ThrowModeReturnsUnexpectedForMalformedJson) {
    JsonLogParser parser({}, ParserErrorAction::Throw, std::nullopt);
    const std::string line = R"({"timestamp":"2023-01-01 10:00:00","level":"INFO","message":"missing brace")";

    auto result = parser.parseLine(line, 11, "json.log");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::JsonParseError);
}
