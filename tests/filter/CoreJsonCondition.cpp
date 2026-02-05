// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 eserlxl

#include <gtest/gtest.h>
#include "filter/Core.h"
#include "core/LogTypes.h"
#include <nlohmann/json.hpp>

// New test fixture for JSON serialization/deserialization tests
class FilterJsonTest : public ::testing::Test {
protected:
    // Utility to create a FilterCondition
    FilterCondition createFilterCondition(
        LogEntryField field,
        FilterOperator op,
        const std::string& value,
        FilterValueType valueType = FilterValueType::STRING,
        bool caseSensitive = false,
        std::optional<std::string> datetimeFormat = std::nullopt
    ) {
        FilterCondition fc;
        fc.field = field;
        fc.op = op;
        fc.value = value;
        fc.valueType = valueType;
        fc.caseSensitive = caseSensitive;
        fc.datetimeFormat = datetimeFormat;
        return fc;
    }
};

// FilterCondition JSON tests
TEST_F(FilterJsonTest, FilterConditionToJson) {
    FilterCondition fc = createFilterCondition(
        LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-01-01T00:00:00Z",
        FilterValueType::DATETIME, false, "%Y-%m-%dT%H:%M:%SZ"
    );

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "TIMESTAMP");
    EXPECT_EQ(j["op"], "GREATER_THAN");
    EXPECT_EQ(j["value"], "2023-01-01T00:00:00Z");
    EXPECT_EQ(j["value_type"], "DATETIME");
    EXPECT_EQ(j["caseSensitive"], false);
    EXPECT_EQ(j["datetimeFormat"], "%Y-%m-%dT%H:%M:%SZ");
}

TEST_F(FilterJsonTest, FilterConditionToJsonWithCustomField) {
    FilterCondition fc;
    fc.field = LogEntryField::CUSTOM;
    fc.op = FilterOperator::EQUALS;
    fc.value = "my_custom_value";
    fc.valueType = FilterValueType::STRING;
    fc.caseSensitive = false;
    fc.customField = "myCustomKey";

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "myCustomKey");
    EXPECT_EQ(j["op"], "EQUALS");
    EXPECT_EQ(j["value"], "my_custom_value");
    EXPECT_EQ(j["value_type"], "STRING");
    EXPECT_EQ(j["caseSensitive"], false);
    EXPECT_TRUE(j.contains("customField"));
    EXPECT_EQ(j["customField"], "myCustomKey");
    EXPECT_FALSE(j.contains("datetimeFormat"));
}

TEST_F(FilterJsonTest, FilterConditionFromJsonSuccess) {
    nlohmann::json j = {
        {"field", "message"},
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
    EXPECT_FALSE(fc.datetimeFormat.has_value());
}

TEST_F(FilterJsonTest, FilterConditionFromJsonWithCustomField) {
    nlohmann::json j = {
        {"field", "someDynamicKey"}, // Field name is directly the custom key
        {"op", "EQUALS"},
        {"value", "specific_value"},
        {"value_type", "STRING"},
        {"caseSensitive", false}
        // No explicit "customField" needed in JSON, it will be inferred and parsed by from_json.
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    EXPECT_EQ(fc.op, FilterOperator::EQUALS);
    EXPECT_EQ(fc.value, "specific_value");
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
    EXPECT_EQ(fc.caseSensitive, false);
    EXPECT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "someDynamicKey");
    EXPECT_FALSE(fc.datetimeFormat.has_value());
}

TEST_F(FilterJsonTest, FilterConditionFromJsonWithNullCustomField) {
    nlohmann::json j = {
        {"field", "custom"},
        {"op", "EQUALS"},
        {"value", "specific_value"},
        {"value_type", "STRING"},
        {"caseSensitive", false},
        {"customField", nullptr} // Explicitly null customField
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    EXPECT_EQ(fc.op, FilterOperator::EQUALS);
    EXPECT_EQ(fc.value, "specific_value");
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
    EXPECT_EQ(fc.caseSensitive, false);
    EXPECT_FALSE(fc.customField.has_value()); // Should be nullopt
    EXPECT_FALSE(fc.datetimeFormat.has_value());
}

TEST_F(FilterJsonTest, FilterConditionFromJsonWithoutCustomField) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "warning"},
        {"value_type", "STRING"},
        {"caseSensitive", true}
        // customField is entirely absent
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::MESSAGE);
    EXPECT_EQ(fc.op, FilterOperator::CONTAINS);
    EXPECT_EQ(fc.value, "warning");
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
    EXPECT_EQ(fc.caseSensitive, true);
    EXPECT_FALSE(fc.customField.has_value()); // Should be nullopt
    EXPECT_FALSE(fc.datetimeFormat.has_value());
}

TEST_F(FilterJsonTest, FilterConditionFromJsonDatetimeSuccess) {
    nlohmann::json j = {
        {"field", "timestamp"},
        {"op", "LESS_THAN"},
        {"value", "2023-12-31 23:59:59"},
        {"value_type", "DATETIME"},
        {"caseSensitive", false},
        {"datetimeFormat", "%Y-%m-%d %H:%M:%S"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::TIMESTAMP);
    EXPECT_EQ(fc.op, FilterOperator::LESS_THAN);
    EXPECT_EQ(fc.value, "2023-12-31 23:59:59");
    EXPECT_EQ(fc.valueType, FilterValueType::DATETIME);
    EXPECT_TRUE(fc.datetimeFormat.has_value());
    EXPECT_EQ(*fc.datetimeFormat, "%Y-%m-%d %H:%M:%S");
}

TEST_F(FilterJsonTest, FilterConditionFromJsonInvalidField) {
    nlohmann::json j = {
        {"field", "bad_field"},
        {"op", "EQUALS"},
        {"value", "value"},
        {"value_type", static_cast<int>(FilterValueType::STRING)}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "bad_field");
}

TEST_F(FilterJsonTest, FilterConditionFromJsonMissingOp) {
    nlohmann::json j = {
        {"field", "message"},
        {"value", "value"},
        {"value_type", static_cast<int>(FilterValueType::STRING)}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("missing or has invalid 'op'"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterConditionFromJsonInvalidValueTypeString) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "warning"},
        {"value_type", "BAD_TYPE"}, // Invalid value_type string
        {"caseSensitive", true}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("unrecognized 'value_type' string"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterConditionFromJsonNumericSuccess) {
    nlohmann::json j = {
        {"field", "thread_id"},
        {"op", "GREATER_THAN"},
        {"value", "100"},
        {"value_type", "INT"},
        {"caseSensitive", false}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::THREAD_ID);
    EXPECT_EQ(fc.op, FilterOperator::GREATER_THAN);
    EXPECT_EQ(fc.value, "100");
    EXPECT_EQ(fc.valueType, FilterValueType::INT);
    EXPECT_FALSE(fc.datetimeFormat.has_value());
}

TEST_F(FilterJsonTest, FilterConditionFromJsonDatetimeMissingFormat) {
    nlohmann::json j = {
        {"field", "timestamp"},
        {"op", "LESS_THAN"},
        {"value", "2023-12-31 23:59:59"},
        {"value_type", "DATETIME"},
        {"caseSensitive", false}
        // datetimeFormat is missing
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("with DATETIME 'value_type' requires a 'datetimeFormat'"), std::string::npos);
}

// FilterCondition datetime constructor validation
TEST_F(FilterJsonTest, FilterConditionDatetimeConstructorValidation) {
    // Should throw if DATETIME type is used without format
    EXPECT_THROW(
        FilterCondition(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-01-01", FilterValueType::DATETIME),
        std::invalid_argument
    );

    // Should not throw if DATETIME type is used with format
    EXPECT_NO_THROW(
        FilterCondition(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-01-01", "%Y-%m-%d")
    );

    // Should not throw for other value types
    EXPECT_NO_THROW(
        FilterCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "error", FilterValueType::STRING)
    );
}

TEST_F(FilterJsonTest, FilterConditionFromJsonMissingValue) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("missing or has invalid 'value'"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterConditionFromJsonMissingValueType) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "some_value"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("missing 'value_type'"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterConditionFromJsonMalformedDatetimeFormat) {
    nlohmann::json j = {
        {"field", "timestamp"},
        {"op", "LESS_THAN"},
        {"value", "2023-12-31"},
        {"value_type", "DATETIME"},
        {"datetimeFormat", 12345} // Invalid type, should be string
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("'datetimeFormat' must be a string or null"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterConditionFromJsonInvalidValueTypeInt) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "warning"},
        {"value_type", 99}, // Invalid integer value
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("invalid integer for 'value_type'"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterConditionFromJsonLegacyValueTypeInt) {
    nlohmann::json j = {
        {"field", "thread_id"},
        {"op", "EQUALS"},
        {"value", "42"},
        {"value_type", 2}, // Legacy integer for NUMERIC (INT)
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(fc.valueType, FilterValueType::INT);
}
