// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2024 Eser KUBALI

#include <gtest/gtest.h>
#include "filter/condition.h"
#include "filter/types.h"
#include "core/log/types.h"
#include <nlohmann/json.hpp>
#include <optional>
#include "filter_json_fixture.h"

using namespace filter;

TEST_F(FilterJsonTest, FilterConditionToJson_Datetime) {
    auto createResult = FilterCondition::createDatetime(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-01-01T00:00:00Z", "%Y-%m-%dT%H:%M:%SZ");
    ASSERT_TRUE(createResult) << createResult.error().message;
    nlohmann::json j;
    to_json(j, createResult.value());
    EXPECT_EQ(j["field"], "TIMESTAMP");
    EXPECT_EQ(j["value_type"], "DATETIME");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_Datetime) {
    nlohmann::json j = { {"field", "TIMESTAMP"}, {"op", "GREATER_THAN"}, {"value", "2023-01-01T00:00:00Z"}, {"value_type", "DATETIME"}, {"datetimeFormat", "%Y-%m-%dT%H:%M:%SZ"} };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(fc.valueType, FilterValueType::DATETIME);
}

TEST_F(FilterJsonTest, FilterConditionToJson_String) {
    auto createResult = FilterCondition::createString(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "error message");
    ASSERT_TRUE(createResult) << createResult.error().message;
    nlohmann::json j;
    to_json(j, createResult.value());
    EXPECT_EQ(j["field"], "MESSAGE");
    EXPECT_EQ(j["value_type"], "STRING");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_String) {
    nlohmann::json j = { {"field", "MESSAGE"}, {"op", "CONTAINS"}, {"value", "error message"}, {"value_type", "STRING"} };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
}

TEST_F(FilterJsonTest, FilterConditionToJson_NumberInt) {
    FilterCondition fc;
    fc.field = LogEntryField::LINE_NUMBER;
    fc.op = FilterOperator::GREATER_THAN;
    fc.value = static_cast<int64_t>(100);
    fc.valueType = FilterValueType::INT;
    nlohmann::json j;
    to_json(j, fc);
    EXPECT_EQ(j["value_type"], "INT");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_NumberInt) {
    nlohmann::json j = { {"field", "LINE_NUMBER"}, {"op", "GREATER_THAN"}, {"value", 100}, {"value_type", "INT"} };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(fc.valueType, FilterValueType::INT);
}

TEST_F(FilterJsonTest, FilterConditionToJson_NumberDouble) {
    FilterCondition fc;
    fc.field = LogEntryField::CUSTOM;
    fc.customField = "duration";
    fc.op = FilterOperator::LESS_THAN;
    fc.value = 5.5;
    fc.valueType = FilterValueType::DOUBLE;
    nlohmann::json j;
    to_json(j, fc);
    EXPECT_EQ(j["value_type"], "DOUBLE");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_NumberDouble) {
    nlohmann::json j = { {"field", "duration"}, {"op", "LESS_THAN"}, {"value", 5.5}, {"value_type", "DOUBLE"} };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(fc.valueType, FilterValueType::DOUBLE);
}

TEST_F(FilterJsonTest, FilterConditionToJson_Boolean) {
    FilterCondition fc;
    fc.field = LogEntryField::CUSTOM;
    fc.customField = "is_error";
    fc.op = FilterOperator::EQUALS;
    fc.value = true;
    fc.valueType = FilterValueType::BOOL;
    nlohmann::json j;
    to_json(j, fc);
    EXPECT_EQ(j["value_type"], "BOOL");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_Boolean) {
    nlohmann::json j = { {"field", "is_error"}, {"op", "EQUALS"}, {"value", true}, {"value_type", "BOOL"} };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(fc.valueType, FilterValueType::BOOL);
}

TEST_F(FilterJsonTest, FilterConditionFromJson_MissingField) {
    nlohmann::json j = { {"op", "EQUALS"}, {"value", "test"}, {"value_type", "STRING"} };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
}

TEST_F(FilterJsonTest, FilterConditionFromJson_InvalidOperatorString) {
    nlohmann::json j = { {"field", "MESSAGE"}, {"op", "INVALID_OP"}, {"value", "test"}, {"value_type", "STRING"} };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result);
}
