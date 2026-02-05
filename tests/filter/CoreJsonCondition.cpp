// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 eserlxl

#include <gtest/gtest.h>
#include "filter/Core.h"
#include "core/LogTypes.h"
#include <nlohmann/json.hpp>
#include <iostream> // Required for std::cerr

// New test fixture for JSON serialization/deserialization tests
class FilterJsonTest : public ::testing::Test {};

// --- to_json Serialization Tests ---

TEST_F(FilterJsonTest, ToJsonStandardField) {
    auto fcResult = FilterCondition::createDatetime(
        LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-01-01T00:00:00Z", "%Y-%m-%dT%H:%M:%SZ"
    );
    ASSERT_TRUE(fcResult.has_value());
    FilterCondition fc = *fcResult;

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "TIMESTAMP");
    EXPECT_EQ(j["op"], "GREATER_THAN");
    EXPECT_EQ(j["value"], "2023-01-01T00:00:00Z");
    EXPECT_EQ(j["value_type"], "DATETIME");
    EXPECT_EQ(j["caseSensitive"], false);
    EXPECT_EQ(j["datetimeFormat"], "%Y-%m-%dT%H:%M:%SZ");
    EXPECT_FALSE(j.contains("customField")); // Should not be present
}

TEST_F(FilterJsonTest, ToJsonCustomField) {
    auto fcResult = FilterCondition::createCustomString("myCustomKey", FilterOperator::EQUALS, "my_custom_value", true);
    ASSERT_TRUE(fcResult.has_value());
    FilterCondition fc = *fcResult;

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "myCustomKey"); // Custom field name is the value of "field"
    EXPECT_EQ(j["op"], "EQUALS");
    EXPECT_EQ(j["value"], "my_custom_value");
    EXPECT_EQ(j["value_type"], "STRING");
    EXPECT_EQ(j["caseSensitive"], true);
    EXPECT_FALSE(j.contains("datetimeFormat"));
    EXPECT_FALSE(j.contains("customField")); // Redundant key should NOT be serialized
}

// --- from_json Deserialization Tests ---

TEST_F(FilterJsonTest, FromJsonSuccessStandardField) {
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"op", "CONTAINS"},
        {"value", "warning"},
        {"value_type", "STRING"},
        {"caseSensitive", true}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::MESSAGE);
    EXPECT_EQ(fc.op, FilterOperator::CONTAINS);
    EXPECT_EQ(fc.value, "warning");
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
    EXPECT_TRUE(fc.caseSensitive);
    EXPECT_FALSE(fc.customField.has_value());
}

TEST_F(FilterJsonTest, FromJsonSuccessCustomField) {
    nlohmann::json j = {
        {"field", "someDynamicKey"}, // Field name is the custom key
        {"op", "EQUALS"},
        {"value", "specific_value"},
        {"value_type", "STRING"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    EXPECT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "someDynamicKey");
    EXPECT_EQ(fc.op, FilterOperator::EQUALS);
    EXPECT_EQ(fc.value, "specific_value");
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
    EXPECT_FALSE(fc.caseSensitive);
}

TEST_F(FilterJsonTest, FromJsonSuccessDatetime) {
    nlohmann::json j = {
        {"field", "timestamp"},
        {"op", "LESS_THAN"},
        {"value", "2023-12-31 23:59:59"},
        {"value_type", "DATETIME"},
        {"datetimeFormat", "%Y-%m-%d %H:%M:%S"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::TIMESTAMP);
    EXPECT_EQ(fc.op, FilterOperator::LESS_THAN);
    EXPECT_EQ(fc.valueType, FilterValueType::DATETIME);
    EXPECT_TRUE(fc.datetimeFormat.has_value());
    EXPECT_EQ(*fc.datetimeFormat, "%Y-%m-%d %H:%M:%S");
}

TEST_F(FilterJsonTest, FromJsonSuccessLegacyValueTypeInt) {
    nlohmann::json j = {
        {"field", "thread_id"},
        {"op", "EQUALS"},
        {"value", "42"},
        {"value_type", 2}, // Legacy integer for INT
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(fc.valueType, FilterValueType::INT);
}

// --- Custom Field Specific from_json Tests (based on audit recommendations) ---
// The audit recommends that the 'field' key should be the single source of truth for custom field names,
// and logic for a separate 'customField' JSON key should be removed.
// These tests verify that behaviour.

TEST_F(FilterJsonTest, FromJsonFailureFieldIsCustomLiteral) {
    // Test case where 'field' is literally "CUSTOM", which should be treated as a custom field name.
    nlohmann::json j = {
        {"field", "CUSTOM"},
        {"op", "EQUALS"},
        {"value", "some_value"},
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    EXPECT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "CUSTOM"); // The literal string "CUSTOM" is used as the custom field name.
}

TEST_F(FilterJsonTest, FromJsonIgnoresCustomFieldJsonKey) {
    // Test to explicitly verify that the 'customField' key in JSON is ignored,
    // as per the audit recommendation to remove such logic.
    nlohmann::json j = {
        {"field", "myCustomFieldKey"}, // The actual custom field name
        {"customField", "thisKeyShouldBeIgnored"}, // This should be ignored
        {"op", "EQUALS"},
        {"value", "specific_value"},
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    // Verify that the custom field name comes from the 'field' key and not 'customField'
    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    EXPECT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "myCustomFieldKey");
}

// --- from_json Failure Case Tests ---

TEST_F(FilterJsonTest, FromJsonFailureMissingField) {
    nlohmann::json j = {
        {"op", "CONTAINS"},
        {"value", "warning"},
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
}

TEST_F(FilterJsonTest, FromJsonFailureEmptyField) {
    nlohmann::json j = {
        {"field", ""},
        {"op", "EQUALS"},
        {"value", "value"},
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "Field name cannot be empty.");
}

TEST_F(FilterJsonTest, FromJsonFailureMissingOp) {
    nlohmann::json j = {
        {"field", "message"},
        {"value", "value"},
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
}

TEST_F(FilterJsonTest, FromJsonFailureInvalidOp) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "INVALID_OPERATOR"},
        {"value", "value"},
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "Unrecognized operator: INVALID_OPERATOR");
}


TEST_F(FilterJsonTest, FromJsonFailureMissingValue) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
}

TEST_F(FilterJsonTest, FromJsonFailureMissingValueType) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "some_value"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
}

TEST_F(FilterJsonTest, FromJsonFailureInvalidValueTypeString) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "warning"},
        {"value_type", "BAD_TYPE"},
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "Unrecognized value_type: BAD_TYPE");
}

TEST_F(FilterJsonTest, FromJsonFailureInvalidValueTypeInt) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "warning"},
        {"value_type", 99},
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "Invalid integer for 'value_type'.");
}

TEST_F(FilterJsonTest, FromJsonFailureDatetimeMissingFormat) {
    nlohmann::json j = {
        {"field", "timestamp"},
        {"op", "LESS_THAN"},
        {"value", "2023-12-31 23:59:59"},
        {"value_type", "DATETIME"},
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "DATETIME value_type requires 'datetimeFormat'.");
}

// --- Factory Function Tests ---

TEST_F(FilterJsonTest, FactorySuccess) {
    auto r1 = FilterCondition::createString(LogEntryField::MESSAGE, FilterOperator::EQUALS, "test", true);
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(r1->field, LogEntryField::MESSAGE);
    EXPECT_TRUE(r1->caseSensitive);

    auto r2 = FilterCondition::createTyped(LogEntryField::LEVEL, FilterOperator::NOT_EQUALS, "ERROR", FilterValueType::STRING);
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r2->field, LogEntryField::LEVEL);
    EXPECT_EQ(r2->valueType, FilterValueType::STRING);
    
    auto r3 = FilterCondition::createDatetime(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "now", "YYYY-MM-DD");
    ASSERT_TRUE(r3.has_value());
    EXPECT_EQ(r3->field, LogEntryField::TIMESTAMP);
    EXPECT_EQ(r3->valueType, FilterValueType::DATETIME);
    EXPECT_EQ(r3->datetimeFormat, "YYYY-MM-DD");
}

TEST_F(FilterJsonTest, FactorySuccessCustom) {
    auto r1 = FilterCondition::createCustomString("myField", FilterOperator::CONTAINS, "value", false);
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(r1->field, LogEntryField::CUSTOM);
    EXPECT_TRUE(r1->customField.has_value());
    EXPECT_EQ(r1->customField, "myField");

    auto r2 = FilterCondition::createCustomTyped("myOtherField", FilterOperator::LESS_THAN, "123", FilterValueType::INT);
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r2->field, LogEntryField::CUSTOM);
    EXPECT_EQ(r2->valueType, FilterValueType::INT);
    EXPECT_TRUE(r2->customField.has_value());
    EXPECT_EQ(r2->customField, "myOtherField");

    auto r3 = FilterCondition::createCustomDatetime("myTimeField", FilterOperator::LESS_THAN, "yesterday", "magic");
    ASSERT_TRUE(r3.has_value());
    EXPECT_EQ(r3->field, LogEntryField::CUSTOM);
    EXPECT_EQ(r3->valueType, FilterValueType::DATETIME);
    EXPECT_TRUE(r3->customField.has_value());
    EXPECT_EQ(r3->customField, "myTimeField");
    EXPECT_TRUE(r3->datetimeFormat.has_value());
    EXPECT_EQ(r3->datetimeFormat, "magic");
}

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
