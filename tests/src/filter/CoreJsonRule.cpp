// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2024 Eser KUBALI

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "filter/Legacy.h"
#include "core/LogTypes.h"
#include <nlohmann/json.hpp>

// Test fixture for FilterRule JSON serialization/deserialization tests
class FilterRuleJsonTest : public ::testing::Test {};

// FilterRule JSON tests
TEST_F(FilterRuleJsonTest, FilterRuleToJson) {
    FilterRule fr;
    fr.field = LogEntryField::MESSAGE;
    fr.op = FilterOperator::CONTAINS;
    fr.value = "error";
    fr.caseSensitive = true;

    nlohmann::json j;
    to_json(j, fr);

    EXPECT_EQ(j["field"], "message");
    EXPECT_EQ(j["op"], "CONTAINS");
    EXPECT_EQ(j["value"], "error");
    EXPECT_EQ(j["caseSensitive"], true);
}

TEST_F(FilterRuleJsonTest, FilterRuleFromJsonSuccess) {
    nlohmann::json j = {
        {"field", "level"},
        {"op", "EQUALS"},
        {"value", "INFO"},
        {"caseSensitive", false}
    };

    auto result = from_json(j);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    FilterRule fr = result.value();

    EXPECT_EQ(fr.field, LogEntryField::LEVEL);
    EXPECT_EQ(fr.op, FilterOperator::EQUALS);
    EXPECT_EQ(fr.value, "INFO");
    EXPECT_EQ(fr.caseSensitive, false);
}

TEST_F(FilterRuleJsonTest, FilterRuleFromJsonInvalidField) {
    nlohmann::json j = {
        {"field", "non_existent_field"},
        {"op", "EQUALS"},
        {"value", "INFO"}
    };

    auto result = from_json(j);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_THAT(result.error().message, testing::HasSubstr("Unrecognized field"));
}

TEST_F(FilterRuleJsonTest, FilterRuleFromJsonMissingField) {
    nlohmann::json j = {
        {"op", "EQUALS"},
        {"value", "INFO"}
    };

    auto result = from_json(j);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_THAT(result.error().message, testing::HasSubstr("Missing required key"));
}

TEST_F(FilterRuleJsonTest, FilterRuleFromJsonMissingOp) {
    nlohmann::json j = {
        {"field", "level"},
        {"value", "INFO"}
    };

    auto result = from_json(j);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_THAT(result.error().message, testing::HasSubstr("Missing required key"));
}

TEST_F(FilterRuleJsonTest, FilterRuleFromJsonMissingValue) {
    nlohmann::json j = {
        {"field", "level"},
        {"op", "EQUALS"}
    };

    auto result = from_json(j);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_THAT(result.error().message, testing::HasSubstr("Missing required key"));
}

TEST_F(FilterRuleJsonTest, FilterRuleFromJsonInvalidOp) {
    nlohmann::json j = {
        {"field", "level"},
        {"op", "INVALID_OP"},
        {"value", "INFO"}
    };

    auto result = from_json(j);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_THAT(result.error().message, testing::HasSubstr("Unrecognized operator"));
}

// Test for round-trip serialization
TEST_F(FilterRuleJsonTest, FilterRuleRoundTrip) {
    FilterRule original_fr;
    original_fr.field = LogEntryField::MESSAGE;
    original_fr.op = FilterOperator::CONTAINS;
    original_fr.value = "error";
    original_fr.caseSensitive = true;

    nlohmann::json j;
    to_json(j, original_fr);

    auto result = from_json(j);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    FilterRule round_tripped_fr = result.value();

    EXPECT_EQ(original_fr.field, round_tripped_fr.field);
    EXPECT_EQ(original_fr.op, round_tripped_fr.op);
    EXPECT_EQ(original_fr.value, round_tripped_fr.value);
    EXPECT_EQ(original_fr.caseSensitive, round_tripped_fr.caseSensitive);
}

// Test for empty string value
TEST_F(FilterRuleJsonTest, FilterRuleFromJsonEmptyValue) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", ""}, // Empty string
        {"caseSensitive", true}
    };

    auto result = from_json(j);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    FilterRule fr = result.value();

    EXPECT_EQ(fr.field, LogEntryField::MESSAGE);
    EXPECT_EQ(fr.op, FilterOperator::CONTAINS);
    EXPECT_EQ(fr.value, "");
    EXPECT_EQ(fr.caseSensitive, true);
}

// Test for value with only whitespace
TEST_F(FilterRuleJsonTest, FilterRuleFromJsonWhitespaceValue) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "   "}, // Whitespace string
        {"caseSensitive", false}
    };

    auto result = from_json(j);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    FilterRule fr = result.value();

    EXPECT_EQ(fr.field, LogEntryField::MESSAGE);
    EXPECT_EQ(fr.op, FilterOperator::CONTAINS);
    EXPECT_EQ(fr.value, "   ");
    EXPECT_EQ(fr.caseSensitive, false);
}

// Test for default caseSensitive value
TEST_F(FilterRuleJsonTest, FilterRuleFromJsonDefaultCaseSensitive) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "test"}
    };

    auto result = from_json(j);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_FALSE(result.value().caseSensitive);
}
