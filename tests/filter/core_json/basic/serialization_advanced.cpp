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

TEST_F(FilterJsonTest, FilterConditionToJson_ThreadId) {
    FilterCondition fc;
    fc.field = LogEntryField::THREAD_ID;
    fc.op = FilterOperator::EQUALS;
    fc.value = static_cast<int64_t>(12345);
    fc.valueType = FilterValueType::INT;
    nlohmann::json j;
    to_json(j, fc);
    EXPECT_EQ(j["field"], "THREAD_ID");
    EXPECT_EQ(j["op"], "EQUALS");
    EXPECT_EQ(j["value"].get<int64_t>(), 12345);
    EXPECT_EQ(j["value_type"], "INT");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_ThreadId) {
    nlohmann::json j = { {"field", "THREAD_ID"}, {"op", "EQUALS"}, {"value", 12345}, {"value_type", "INT"} };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(fc.field, LogEntryField::THREAD_ID);
    EXPECT_EQ(fc.op, FilterOperator::EQUALS);
    EXPECT_EQ(std::get<int64_t>(fc.value), 12345);
}

TEST_F(FilterJsonTest, FilterConditionToJson_LogLevel) {
    auto createResult = FilterCondition::createTyped(LogEntryField::LEVEL, FilterOperator::EQUALS, "ERROR", FilterValueType::LOG_LEVEL);
    ASSERT_TRUE(createResult) << createResult.error().message;
    nlohmann::json j;
    to_json(j, createResult.value());
    EXPECT_EQ(j["field"], "LEVEL");
    EXPECT_EQ(j["value_type"], "LOG_LEVEL");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_LogLevel) {
    nlohmann::json j = { {"field", "LEVEL"}, {"op", "EQUALS"}, {"value", "ERROR"}, {"value_type", "LOG_LEVEL"} };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(fc.valueType, FilterValueType::LOG_LEVEL);
}

TEST_F(FilterJsonTest, FilterConditionToJson_IpAddress) {
    auto createResult = FilterCondition::createCustomTyped("client_ip", FilterOperator::EQUALS, "127.0.0.1", FilterValueType::IP_ADDRESS);
    ASSERT_TRUE(createResult) << createResult.error().message;
    nlohmann::json j;
    to_json(j, createResult.value());
    EXPECT_EQ(j["field"], "client_ip");
    EXPECT_EQ(j["value_type"], "IP_ADDRESS");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_IpAddress) {
    nlohmann::json j = { {"field", "client_ip"}, {"op", "EQUALS"}, {"value", "127.0.0.1"}, {"value_type", "IP_ADDRESS"} };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(fc.valueType, FilterValueType::IP_ADDRESS);
}

TEST_F(FilterJsonTest, FilterConditionToJson_Version) {
    auto createResult = FilterCondition::createCustomTyped("app_version", FilterOperator::GREATER_THAN, "1.2.3", FilterValueType::VERSION);
    ASSERT_TRUE(createResult) << createResult.error().message;
    nlohmann::json j;
    to_json(j, createResult.value());
    EXPECT_EQ(j["field"], "app_version");
    EXPECT_EQ(j["value_type"], "VERSION");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_Version) {
    nlohmann::json j = { {"field", "app_version"}, {"op", "GREATER_THAN"}, {"value", "1.2.3"}, {"value_type", "VERSION"} };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(fc.valueType, FilterValueType::VERSION);
}

TEST_F(FilterJsonTest, FilterConditionToJson_SetOperator) {
    std::vector<std::string> values = {"debug", "trace"};
    auto createResult = FilterCondition::createSet(LogEntryField::LEVEL, FilterOperator::NOT_IN, values);
    ASSERT_TRUE(createResult) << createResult.error().message;
    nlohmann::json j;
    to_json(j, createResult.value());
    EXPECT_EQ(j["op"], "NOT_IN");
    EXPECT_EQ(j["value"].get<std::vector<std::string>>(), values);
}

TEST_F(FilterJsonTest, FilterConditionFromJson_SetOperator) {
    nlohmann::json j = { {"field", "MESSAGE"}, {"op", "IN"}, {"value", {"value1", "value2"}}, {"value_type", "AUTO"} };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(fc.op, FilterOperator::IN);
    ASSERT_TRUE(std::holds_alternative<std::vector<std::string>>(fc.value));
}

TEST_F(FilterJsonTest, FilterConditionToJson_IsNotNullOperator) {
    FilterCondition fc;
    fc.field = LogEntryField::HOST;
    fc.op = FilterOperator::IS_PRESENT;
    fc.valueType = FilterValueType::STRING;
    nlohmann::json j;
    to_json(j, fc);
    EXPECT_EQ(j["op"], "IS_PRESENT");
    EXPECT_FALSE(j.contains("value"));
}

TEST_F(FilterJsonTest, FilterConditionFromJson_IsNotNullOperator) {
    nlohmann::json j = { {"field", "HOST"}, {"op", "IS_PRESENT"}, {"value_type", "STRING"} };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(fc.op, FilterOperator::IS_PRESENT);
}
