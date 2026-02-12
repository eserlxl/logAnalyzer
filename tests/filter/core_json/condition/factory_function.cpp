// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2024 Eser KUBALI

#include <gtest/gtest.h>
#include "core/log/types.h"
#include <nlohmann/json.hpp>
#include "filter_json_fixture.h"

using namespace filter;

// --- Factory Function Failure Tests ---

TEST_F(FilterJsonTest, FactoryFailureForStandardFactoriesWithCustomField) {
    auto r1 = FilterCondition::createString(LogEntryField::CUSTOM, FilterOperator::EQUALS, "test");
    ASSERT_FALSE(r1.has_value());
    EXPECT_EQ(r1.error().message, "Use createCustomString for CUSTOM fields.");

    auto r2 = FilterCondition::createTyped(LogEntryField::CUSTOM, FilterOperator::EQUALS, "test", FilterValueType::STRING);
    ASSERT_FALSE(r2.has_value());
    EXPECT_EQ(r2.error().message, "Use createCustomTyped for CUSTOM fields.");

    auto r3 = FilterCondition::createDatetime(LogEntryField::CUSTOM, FilterOperator::EQUALS, "test", "format");
    ASSERT_FALSE(r3.has_value());
    EXPECT_EQ(r3.error().message, "Use createCustomDatetime for CUSTOM fields.");
}

TEST_F(FilterJsonTest, FactoryFailureForCustomFactoriesWithEmptyName) {
    auto r1 = FilterCondition::createCustomString("", FilterOperator::EQUALS, "test");
    ASSERT_FALSE(r1.has_value());
    EXPECT_EQ(r1.error().message, "Custom field name cannot be empty.");

    auto r2 = FilterCondition::createCustomTyped("", FilterOperator::EQUALS, "test", FilterValueType::STRING);
    ASSERT_FALSE(r2.has_value());
    EXPECT_EQ(r2.error().message, "Custom field name cannot be empty.");

    auto r3 = FilterCondition::createCustomDatetime("", FilterOperator::EQUALS, "test", "format");
    ASSERT_FALSE(r3.has_value());
    EXPECT_EQ(r3.error().message, "Custom field name cannot be empty.");
}

TEST_F(FilterJsonTest, FactoryFailureForTypedWithDatetime) {
    auto result = FilterCondition::createTyped(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-01-01", FilterValueType::DATETIME);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "Use createDatetime for DATETIME valueType.");
}

TEST_F(FilterJsonTest, FactoryFailureForDatetimeWithEmptyFormat) {
    auto result = FilterCondition::createDatetime(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-01-01", "");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "datetimeFormat cannot be empty for DATETIME type.");
}
