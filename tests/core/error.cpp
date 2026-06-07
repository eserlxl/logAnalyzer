// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "core/error.h"
#include "core/log/types.h"
#include <nlohmann/json.hpp>

// Regression guard: every Code enum value must stringify to its own name, never
// the "UnknownCode" default. JsonTypeError, MissingField, and UnknownJsonError
// were missing from the switch and silently serialized as "UnknownCode".
TEST(ErrorCodeToStringTest, MissingJsonCodesStringifyToTheirName) {
    EXPECT_EQ(ErrorCode::toString(Code::JsonTypeError), "JsonTypeError");
    EXPECT_EQ(ErrorCode::toString(Code::MissingField), "MissingField");
    EXPECT_EQ(ErrorCode::toString(Code::UnknownJsonError), "UnknownJsonError");
}

// None of the three previously-missing codes may collide with the default.
TEST(ErrorCodeToStringTest, MissingJsonCodesAreNotUnknownCode) {
    EXPECT_NE(ErrorCode::toString(Code::JsonTypeError), "UnknownCode");
    EXPECT_NE(ErrorCode::toString(Code::MissingField), "UnknownCode");
    EXPECT_NE(ErrorCode::toString(Code::UnknownJsonError), "UnknownCode");
}

// Guard a representative sample of the previously-correct codes so this test
// also pins the existing stringification behavior.
TEST(ErrorCodeToStringTest, ExistingCodesUnchanged) {
    EXPECT_EQ(ErrorCode::toString(Code::Unknown), "Unknown");
    EXPECT_EQ(ErrorCode::toString(Code::InvalidArgument), "InvalidArgument");
    EXPECT_EQ(ErrorCode::toString(Code::JsonParseError), "JsonParseError");
    EXPECT_EQ(ErrorCode::toString(Code::FieldNotFound), "FieldNotFound");
    EXPECT_EQ(ErrorCode::toString(Code::Unexpected), "Unexpected");
}

// Externally-observable behavior: a LogEntry carrying a MissingField parse error
// must serialize the real code name in its JSON, not "UnknownCode".
TEST(ErrorCodeToStringTest, LogEntryJsonEmitsRealParseErrorCode) {
    LogEntry entry;
    entry.parsingErrors.emplace_back(Code::MissingField,
                                     "Missing or invalid 'message' field");

    const nlohmann::json j = entry.toJson();
    ASSERT_TRUE(j.contains("parsingErrors"));
    ASSERT_TRUE(j["parsingErrors"].is_array());
    ASSERT_EQ(j["parsingErrors"].size(), 1U);
    EXPECT_EQ(j["parsingErrors"][0]["code"].get<std::string>(), "MissingField");
    EXPECT_EQ(j["parsingErrors"][0]["message"].get<std::string>(),
              "Missing or invalid 'message' field");
}
