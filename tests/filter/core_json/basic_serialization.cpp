// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2024 Eser KUBALI

#include <gtest/gtest.h>
#include "filter/condition.h"
#include "filter/types.h"
#include "core/log/types.h"
#include <nlohmann/json.hpp>
#include <optional>
#include "../core_json/filter_json_fixture.h"

using namespace filter;

// FilterCondition JSON tests
TEST_F(FilterJsonTest, FilterConditionToJson_Datetime) {
    auto createResult = FilterCondition::createDatetime(
        LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-01-01T00:00:00Z", "%Y-%m-%dT%H:%M:%SZ"
    );
    ASSERT_TRUE(createResult) << createResult.error().message;
    const FilterCondition& fc = createResult.value();

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "TIMESTAMP");
    EXPECT_EQ(j["op"], "GREATER_THAN");
    EXPECT_EQ(j["value"].get<std::string>(), "2023-01-01T00:00:00Z");
    EXPECT_EQ(j["value_type"], "DATETIME");
    EXPECT_FALSE(j.contains("caseSensitive"));
    EXPECT_EQ(j["datetimeFormat"], "%Y-%m-%dT%H:%M:%SZ");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_Datetime) {
    nlohmann::json j = {
        {"field", "TIMESTAMP"},
        {"op", "GREATER_THAN"},
        {"value", "2023-01-01T00:00:00Z"},
        {"value_type", "DATETIME"},
        {"caseSensitive", false},
        {"datetimeFormat", "%Y-%m-%dT%H:%M:%SZ"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::TIMESTAMP);
    EXPECT_EQ(fc.op, FilterOperator::GREATER_THAN);
    ASSERT_TRUE(std::holds_alternative<std::string>(fc.value));
    EXPECT_EQ(std::get<std::string>(fc.value), "2023-01-01T00:00:00Z");
    EXPECT_EQ(fc.valueType, FilterValueType::DATETIME);
    EXPECT_EQ(fc.caseSensitive, false);
    ASSERT_TRUE(fc.datetimeFormat.has_value());

    EXPECT_EQ(fc.datetimeFormat.value(), "%Y-%m-%dT%H:%M:%SZ");
}

TEST_F(FilterJsonTest, FilterConditionToJson_String) {
    auto createResult = FilterCondition::createString(
        LogEntryField::MESSAGE, FilterOperator::CONTAINS, "error message"
    );
    ASSERT_TRUE(createResult) << createResult.error().message;
    const FilterCondition& fc = createResult.value();

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "MESSAGE");
    EXPECT_EQ(j["op"], "CONTAINS");
    EXPECT_EQ(j["value"].get<std::string>(), "error message");
    EXPECT_EQ(j["value_type"], "STRING");
    EXPECT_FALSE(j.contains("caseSensitive"));
    EXPECT_FALSE(j.contains("datetimeFormat"));
}

TEST_F(FilterJsonTest, FilterConditionFromJson_String) {
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"op", "CONTAINS"},
        {"value", "error message"},
        {"value_type", "STRING"},
        {"caseSensitive", false}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::MESSAGE);
    EXPECT_EQ(fc.op, FilterOperator::CONTAINS);
    ASSERT_TRUE(std::holds_alternative<std::string>(fc.value));
    EXPECT_EQ(std::get<std::string>(fc.value), "error message");
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
    EXPECT_EQ(fc.caseSensitive, false);
    EXPECT_FALSE(fc.datetimeFormat.has_value());
}

TEST_F(FilterJsonTest, FilterConditionToJson_StringCaseSensitive) {
    auto createResult = FilterCondition::createString(
        LogEntryField::SOURCE_FILE, FilterOperator::EQUALS, "main.cpp", true
    );
    ASSERT_TRUE(createResult) << createResult.error().message;
    const FilterCondition& fc = createResult.value();

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "SOURCE_FILE");
    EXPECT_EQ(j["op"], "EQUALS");
    EXPECT_EQ(j["value"].get<std::string>(), "main.cpp");
    EXPECT_EQ(j["value_type"], "STRING");
    EXPECT_EQ(j["caseSensitive"], true);
    EXPECT_FALSE(j.contains("datetimeFormat"));
}

TEST_F(FilterJsonTest, FilterConditionFromJson_StringCaseSensitive) {
    nlohmann::json j = {
        {"field", "SOURCE_FILE"},
        {"op", "EQUALS"},
        {"value", "main.cpp"},
        {"value_type", "STRING"},
        {"caseSensitive", true}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::SOURCE_FILE);
    EXPECT_EQ(fc.op, FilterOperator::EQUALS);
    ASSERT_TRUE(std::holds_alternative<std::string>(fc.value));
    EXPECT_EQ(std::get<std::string>(fc.value), "main.cpp");
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
    EXPECT_EQ(fc.caseSensitive, true);
    EXPECT_FALSE(fc.datetimeFormat.has_value());
}

TEST_F(FilterJsonTest, FilterConditionToJson_StringRegex) {
    auto createResult = FilterCondition::createString(
        LogEntryField::MESSAGE, FilterOperator::REGEX, ".*error.*", false
    );
    ASSERT_TRUE(createResult) << createResult.error().message;
    const FilterCondition& fc = createResult.value();

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "MESSAGE");
    EXPECT_EQ(j["op"], "REGEX");
    EXPECT_EQ(j["value"].get<std::string>(), ".*error.*");
    EXPECT_EQ(j["value_type"], "STRING");
    EXPECT_FALSE(j.contains("caseSensitive"));
}

TEST_F(FilterJsonTest, FilterConditionFromJson_StringRegex) {
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"op", "REGEX"},
        {"value", ".*error.*"},
        {"value_type", "STRING"},
        {"caseSensitive", false}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::MESSAGE);
    EXPECT_EQ(fc.op, FilterOperator::REGEX);
    ASSERT_TRUE(std::holds_alternative<std::string>(fc.value));
    EXPECT_EQ(std::get<std::string>(fc.value), ".*error.*");
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
    EXPECT_EQ(fc.caseSensitive, false);
}

TEST_F(FilterJsonTest, FilterConditionToJson_StringStartsWith) {
    auto createResult = FilterCondition::createString(
        LogEntryField::HOST, FilterOperator::STARTS_WITH, "web-", true
    );
    ASSERT_TRUE(createResult) << createResult.error().message;
    const FilterCondition& fc = createResult.value();

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "HOST");
    EXPECT_EQ(j["op"], "STARTS_WITH");
    EXPECT_EQ(j["value"].get<std::string>(), "web-");
    EXPECT_EQ(j["value_type"], "STRING");
    EXPECT_EQ(j["caseSensitive"], true);
}

TEST_F(FilterJsonTest, FilterConditionFromJson_StringStartsWith) {
    nlohmann::json j = {
        {"field", "HOST"},
        {"op", "STARTS_WITH"},
        {"value", "web-"},
        {"value_type", "STRING"},
        {"caseSensitive", true}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::HOST);
    EXPECT_EQ(fc.op, FilterOperator::STARTS_WITH);
    ASSERT_TRUE(std::holds_alternative<std::string>(fc.value));
    EXPECT_EQ(std::get<std::string>(fc.value), "web-");
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
    EXPECT_EQ(fc.caseSensitive, true);
}

TEST_F(FilterJsonTest, FilterConditionToJson_StringNotEquals) {
    auto createResult = FilterCondition::createString(
        LogEntryField::MESSAGE, FilterOperator::NOT_EQUALS, "success", false
    );
    ASSERT_TRUE(createResult) << createResult.error().message;
    const FilterCondition& fc = createResult.value();

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "MESSAGE");
    EXPECT_EQ(j["op"], "NOT_EQUALS");
    EXPECT_EQ(j["value"].get<std::string>(), "success");
    EXPECT_EQ(j["value_type"], "STRING");
    EXPECT_FALSE(j.contains("caseSensitive"));
}

TEST_F(FilterJsonTest, FilterConditionFromJson_StringNotEquals) {
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"op", "NOT_EQUALS"},
        {"value", "success"},
        {"value_type", "STRING"},
        {"caseSensitive", false}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::MESSAGE);
    EXPECT_EQ(fc.op, FilterOperator::NOT_EQUALS);
    ASSERT_TRUE(std::holds_alternative<std::string>(fc.value));
    EXPECT_EQ(std::get<std::string>(fc.value), "success");
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
    EXPECT_EQ(fc.caseSensitive, false);
}

TEST_F(FilterJsonTest, FilterConditionToJson_NumberInt) {
    FilterCondition fc;
    fc.field = LogEntryField::LINE_NUMBER;
    fc.op = FilterOperator::GREATER_THAN;
    fc.value = static_cast<int64_t>(100);
    fc.valueType = FilterValueType::INT;

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "LINE_NUMBER");
    EXPECT_EQ(j["op"], "GREATER_THAN");
    EXPECT_EQ(j["value"].get<int64_t>(), 100);
    EXPECT_EQ(j["value_type"], "INT");
    EXPECT_FALSE(j.contains("datetimeFormat"));
}

TEST_F(FilterJsonTest, FilterConditionFromJson_NumberInt) {
    nlohmann::json j = {
        {"field", "LINE_NUMBER"},
        {"op", "GREATER_THAN"},
        {"value", 100}, // Test with integer type in JSON
        {"value_type", "INT"},
        {"caseSensitive", false}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::LINE_NUMBER);
    EXPECT_EQ(fc.op, FilterOperator::GREATER_THAN);
    ASSERT_TRUE(std::holds_alternative<int64_t>(fc.value));
    EXPECT_EQ(std::get<int64_t>(fc.value), 100);
    EXPECT_EQ(fc.valueType, FilterValueType::INT);
    EXPECT_EQ(fc.caseSensitive, false);
    EXPECT_FALSE(fc.datetimeFormat.has_value());
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

    EXPECT_EQ(j["field"], "duration");
    EXPECT_EQ(j["op"], "LESS_THAN");
    EXPECT_EQ(j["value"].get<double>(), 5.5);
    EXPECT_EQ(j["value_type"], "DOUBLE");
    EXPECT_FALSE(j.contains("datetimeFormat"));
}

TEST_F(FilterJsonTest, FilterConditionFromJson_NumberDouble) {
    nlohmann::json j = {
        {"field", "duration"},
        {"op", "LESS_THAN"},
        {"value", 5.5}, // Test with double type in JSON
        {"value_type", "DOUBLE"},
        {"caseSensitive", false}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(fc.customField.value(), "duration");
    EXPECT_EQ(fc.op, FilterOperator::LESS_THAN);
    ASSERT_TRUE(std::holds_alternative<double>(fc.value));
    EXPECT_DOUBLE_EQ(std::get<double>(fc.value), 5.5);
    EXPECT_EQ(fc.valueType, FilterValueType::DOUBLE);
    EXPECT_EQ(fc.caseSensitive, false);
    EXPECT_FALSE(fc.datetimeFormat.has_value());
}

TEST_F(FilterJsonTest, FilterConditionToJson_NumberLessThanOrEqual) {
    FilterCondition fc;
    fc.field = LogEntryField::LINE_NUMBER;
    fc.op = FilterOperator::LESS_THAN_OR_EQUAL;
    fc.value = static_cast<int64_t>(42);
    fc.valueType = FilterValueType::INT;

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "LINE_NUMBER");
    EXPECT_EQ(j["op"], "LESS_THAN_OR_EQUAL");
    EXPECT_EQ(j["value"].get<int64_t>(), 42);
    EXPECT_EQ(j["value_type"], "INT");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_NumberLessThanOrEqual) {
    nlohmann::json j = {
        {"field", "LINE_NUMBER"},
        {"op", "LESS_THAN_OR_EQUAL"},
        {"value", 42},
        {"value_type", "INT"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::LINE_NUMBER);
    EXPECT_EQ(fc.op, FilterOperator::LESS_THAN_OR_EQUAL);
    ASSERT_TRUE(std::holds_alternative<int64_t>(fc.value));
    EXPECT_EQ(std::get<int64_t>(fc.value), 42);
    EXPECT_EQ(fc.valueType, FilterValueType::INT);
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

    EXPECT_EQ(j["field"], "is_error");
    EXPECT_EQ(j["op"], "EQUALS");
    EXPECT_EQ(j["value"].get<bool>(), true);
    EXPECT_EQ(j["value_type"], "BOOL");
    EXPECT_FALSE(j.contains("datetimeFormat"));
}

TEST_F(FilterJsonTest, FilterConditionFromJson_Boolean) {
    nlohmann::json j = {
        {"field", "is_error"},
        {"op", "EQUALS"},
        {"value", true}, // Test with boolean type in JSON
        {"value_type", "BOOL"},
        {"caseSensitive", false}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(fc.customField.value(), "is_error");
    EXPECT_EQ(fc.op, FilterOperator::EQUALS);
    ASSERT_TRUE(std::holds_alternative<bool>(fc.value));
    EXPECT_EQ(std::get<bool>(fc.value), true);
    EXPECT_EQ(fc.valueType, FilterValueType::BOOL);
    EXPECT_EQ(fc.caseSensitive, false);
    EXPECT_FALSE(fc.datetimeFormat.has_value());
}

TEST_F(FilterJsonTest, FilterConditionFromJson_MissingField) {
    nlohmann::json j = {
        {"op", "EQUALS"},
        {"value", "test"},
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "Missing required key: 'field'");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_InvalidFieldString) {
    nlohmann::json j = {
        {"field", "NON_EXISTENT_FIELD"},
        {"op", "EQUALS"},
        {"value", "test"},
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    // Should pass, custom field behavior: field becomes CUSTOM, customField = "NON_EXISTENT_FIELD"
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(fc.customField.value(), "NON_EXISTENT_FIELD");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_InvalidOperatorString) {
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"op", "INVALID_OP"},
        {"value", "test"},
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "Unrecognized operator: INVALID_OP");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_InvalidValueTypeString) {
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"op", "EQUALS"},
        {"value", "test"},
        {"value_type", "INVALID_TYPE"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "Unrecognized value_type: INVALID_TYPE");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_DatetimeTypeMissingFormat) {
    nlohmann::json j = {
        {"field", "TIMESTAMP"},
        {"op", "GREATER_THAN"},
        {"value", "2023-01-01T00:00:00Z"},
        {"value_type", "DATETIME"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "DATETIME value_type requires a non-empty 'datetimeFormat'.");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_DatetimeTypeEmptyFormat) {
    nlohmann::json j = {
        {"field", "TIMESTAMP"},
        {"op", "GREATER_THAN"},
        {"value", "2023-01-01T00:00:00Z"},
        {"value_type", "DATETIME"},
        {"datetimeFormat", ""}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "DATETIME value_type requires a non-empty 'datetimeFormat'.");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_InvalidCaseSensitiveType) {
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"op", "EQUALS"},
        {"value", "test"},
        {"value_type", "STRING"},
        {"caseSensitive", "not_a_bool"} // Invalid type
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "Invalid type for 'caseSensitive', must be boolean.");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_InvalidValueForBoolType) {
    nlohmann::json j = {
        {"field", "IS_ERROR"},
        {"op", "EQUALS"},
        {"value", "not_a_boolean"},
        {"value_type", "BOOL"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "Type mismatch: value for BOOL must be a boolean.");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_InvalidValueForIntType) {
    nlohmann::json j = {
        {"field", "LINE_NUMBER"},
        {"op", "EQUALS"},
        {"value", "100a"},
        {"value_type", "INT"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "Type mismatch: value for INT must be an integer.");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_InvalidValueForDoubleType) {
    nlohmann::json j = {
        {"field", "DURATION"},
        {"op", "EQUALS"},
        {"value", "1.2.3"},
        {"value_type", "DOUBLE"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "Type mismatch: value for FLOAT/DOUBLE must be a number.");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_InvalidValueForDatetimeType) {
    nlohmann::json j = {
        {"field", "TIMESTAMP"},
        {"op", "EQUALS"},
        {"value", "not-a-date"},
        {"value_type", "DATETIME"},
        {"datetimeFormat", "%Y-%m-%dT%H:%M:%SZ"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_TRUE(result.error().message.find("Failed to parse datetime value") != std::string::npos);
}

TEST_F(FilterJsonTest, FilterConditionFromJson_ArrayValueNotForInOperator) {
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"op", "EQUALS"},
        {"value", {"item1", "item2"}},
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "Type mismatch: value for STRING must be a string.");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_ValueArrayWithInvalidElementType) {
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"op", "IN"},
        {"value", {"item1", 123, true, {"nested"}}},
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "Invalid type in 'value' array.");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_MissingValueForNonNullableOperator) {
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"op", "EQUALS"},
        // {"value", "test"}, // Value is missing
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "Missing required key: 'value'");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_ValueNullForNonNullableOperator) {
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"op", "EQUALS"},
        {"value", nullptr}, // Value is null
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "Invalid type for key: 'value'");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_IsNullOperatorWithValue) {
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"op", "IS_ABSENT"},
        {"value", "some_value"}, // Value should not be present or should be null/empty string
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result);
    // The value will be overwritten to a monostate for IS_NULL/IS_NOT_NULL
    ASSERT_TRUE(std::holds_alternative<std::monostate>(fc.value));
    EXPECT_EQ(fc.op, FilterOperator::IS_ABSENT);
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
}

TEST_F(FilterJsonTest, FilterConditionToJson_IsNotNullOperator) {
    FilterCondition fc;
    fc.field = LogEntryField::HOST;
    fc.op = FilterOperator::IS_PRESENT;
    fc.valueType = FilterValueType::STRING;

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "HOST");
    EXPECT_EQ(j["op"], "IS_PRESENT");
    EXPECT_FALSE(j.contains("value"));
    EXPECT_EQ(j["value_type"], "STRING");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_IsNotNullOperator) {
    nlohmann::json j = {
        {"field", "HOST"},
        {"op", "IS_PRESENT"},
        {"value_type", "STRING"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::HOST);
    EXPECT_EQ(fc.op, FilterOperator::IS_PRESENT);
    // For IS_PRESENT and IS_ABSENT, the value is expected to be a monostate.
    ASSERT_TRUE(std::holds_alternative<std::monostate>(fc.value));
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
}

TEST_F(FilterJsonTest, FilterConditionFromJson_CustomField) {
    nlohmann::json j = {
        {"field", "custom_log_field"},
        {"op", "EQUALS"},
        {"value", "custom_value"},
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result);
    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(fc.customField.value(), "custom_log_field");
    ASSERT_TRUE(std::holds_alternative<std::string>(fc.value));
    EXPECT_EQ(std::get<std::string>(fc.value), "custom_value");
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
}

TEST_F(FilterJsonTest, FilterConditionFromJson_SetOperator) {
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"op", "IN"},
        {"value", {"value1", "value2", "123", true}},
        {"value_type", "AUTO"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result);
    EXPECT_EQ(fc.field, LogEntryField::MESSAGE);
    EXPECT_EQ(fc.op, FilterOperator::IN);
    ASSERT_TRUE(std::holds_alternative<std::vector<std::string>>(fc.value));
    std::vector<std::string> expectedValues = {"value1", "value2", "123", "true"};
    EXPECT_EQ(std::get<std::vector<std::string>>(fc.value), expectedValues);
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
}

TEST_F(FilterJsonTest, FilterConditionToJson_SetOperatorNotIn) {
    std::vector<std::string> values = {"debug", "trace"};
    auto createResult = FilterCondition::createSet(
        LogEntryField::LEVEL, FilterOperator::NOT_IN, values
    );
    ASSERT_TRUE(createResult) << createResult.error().message;
    const FilterCondition& fc = createResult.value();

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "LEVEL");
    EXPECT_EQ(j["op"], "NOT_IN");
    ASSERT_TRUE(j["value"].is_array());
    EXPECT_EQ(j["value"].get<std::vector<std::string>>(), values);
    EXPECT_EQ(j["value_type"], "STRING");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_SetOperatorNotIn) {
    nlohmann::json j = {
        {"field", "LEVEL"},
        {"op", "NOT_IN"},
        {"value", {"debug", "trace"}},
        {"value_type", "AUTO"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::LEVEL);
    EXPECT_EQ(fc.op, FilterOperator::NOT_IN);
    ASSERT_TRUE(std::holds_alternative<std::vector<std::string>>(fc.value));
    std::vector<std::string> expectedValues = {"debug", "trace"};
    EXPECT_EQ(std::get<std::vector<std::string>>(fc.value), expectedValues);
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
}

// New tests added below

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
    nlohmann::json j = {
        {"field", "THREAD_ID"},
        {"op", "EQUALS"},
        {"value", 12345},
        {"value_type", "INT"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::THREAD_ID);
    EXPECT_EQ(fc.op, FilterOperator::EQUALS);
    ASSERT_TRUE(std::holds_alternative<int64_t>(fc.value));
    EXPECT_EQ(std::get<int64_t>(fc.value), 12345);
    EXPECT_EQ(fc.valueType, FilterValueType::INT);
}

TEST_F(FilterJsonTest, FilterConditionToJson_LogLevel) {
    auto createResult = FilterCondition::createTyped(
        LogEntryField::LEVEL, FilterOperator::EQUALS, "ERROR", FilterValueType::LOG_LEVEL
    );
    ASSERT_TRUE(createResult) << createResult.error().message;
    const FilterCondition& fc = createResult.value();

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "LEVEL");
    EXPECT_EQ(j["op"], "EQUALS");
    EXPECT_EQ(j["value"].get<std::string>(), "ERROR");
    EXPECT_EQ(j["value_type"], "LOG_LEVEL");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_LogLevel) {
    nlohmann::json j = {
        {"field", "LEVEL"},
        {"op", "EQUALS"},
        {"value", "ERROR"},
        {"value_type", "LOG_LEVEL"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::LEVEL);
    EXPECT_EQ(fc.op, FilterOperator::EQUALS);
    ASSERT_TRUE(std::holds_alternative<std::string>(fc.value));
    EXPECT_EQ(std::get<std::string>(fc.value), "ERROR");
    EXPECT_EQ(fc.valueType, FilterValueType::LOG_LEVEL);
}

TEST_F(FilterJsonTest, FilterConditionToJson_IpAddress) {
    auto createResult = FilterCondition::createCustomTyped(
        "client_ip", FilterOperator::EQUALS, "127.0.0.1", FilterValueType::IP_ADDRESS
    );

    ASSERT_TRUE(createResult) << createResult.error().message;
    const FilterCondition& fc = createResult.value();

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "client_ip");
    EXPECT_EQ(j["op"], "EQUALS");
    EXPECT_EQ(j["value"].get<std::string>(), "127.0.0.1");
    EXPECT_EQ(j["value_type"], "IP_ADDRESS");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_IpAddress) {
    nlohmann::json j = {
        {"field", "client_ip"},
        {"op", "EQUALS"},
        {"value", "127.0.0.1"},
        {"value_type", "IP_ADDRESS"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(fc.customField.value(), "client_ip");
    EXPECT_EQ(fc.op, FilterOperator::EQUALS);
    ASSERT_TRUE(std::holds_alternative<std::string>(fc.value));
    EXPECT_EQ(std::get<std::string>(fc.value), "127.0.0.1");
    EXPECT_EQ(fc.valueType, FilterValueType::IP_ADDRESS);
}

TEST_F(FilterJsonTest, FilterConditionToJson_Version) {
    auto createResult = FilterCondition::createCustomTyped(
        "app_version", FilterOperator::GREATER_THAN, "1.2.3", FilterValueType::VERSION
    );
    ASSERT_TRUE(createResult) << createResult.error().message;
    const FilterCondition& fc = createResult.value();

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "app_version");
    EXPECT_EQ(j["op"], "GREATER_THAN");
    EXPECT_EQ(j["value"].get<std::string>(), "1.2.3");
    EXPECT_EQ(j["value_type"], "VERSION");
}

TEST_F(FilterJsonTest, FilterConditionFromJson_Version) {
    nlohmann::json j = {
        {"field", "app_version"},
        {"op", "GREATER_THAN"},
        {"value", "1.2.3"},
        {"value_type", "VERSION"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(fc.customField.value(), "app_version");
    EXPECT_EQ(fc.op, FilterOperator::GREATER_THAN);
    ASSERT_TRUE(std::holds_alternative<std::string>(fc.value));
    EXPECT_EQ(std::get<std::string>(fc.value), "1.2.3");
    EXPECT_EQ(fc.valueType, FilterValueType::VERSION);
}

TEST_F(FilterJsonTest, FilterConditionToJson_StringEndsWith) {
    auto createResult = FilterCondition::createString(
        LogEntryField::SOURCE_FILE, FilterOperator::ENDS_WITH, ".cpp", true
    );
    ASSERT_TRUE(createResult) << createResult.error().message;
    const FilterCondition& fc = createResult.value();

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "SOURCE_FILE");
    EXPECT_EQ(j["op"], "ENDS_WITH");
    EXPECT_EQ(j["value"].get<std::string>(), ".cpp");
    EXPECT_EQ(j["value_type"], "STRING");
    EXPECT_EQ(j["caseSensitive"], true);
}

TEST_F(FilterJsonTest, FilterConditionFromJson_StringEndsWith) {
    nlohmann::json j = {
        {"field", "SOURCE_FILE"},
        {"op", "ENDS_WITH"},
        {"value", ".cpp"},
        {"value_type", "STRING"},
        {"caseSensitive", true}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::SOURCE_FILE);
    EXPECT_EQ(fc.op, FilterOperator::ENDS_WITH);
    ASSERT_TRUE(std::holds_alternative<std::string>(fc.value));
    EXPECT_EQ(std::get<std::string>(fc.value), ".cpp");
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
    EXPECT_EQ(fc.caseSensitive, true);
}

TEST_F(FilterJsonTest, FilterConditionToJson_StringNotContains) {
    auto createResult = FilterCondition::createString(
        LogEntryField::MESSAGE, FilterOperator::NOT_CONTAINS, "secret"
    );
    ASSERT_TRUE(createResult) << createResult.error().message;
    const FilterCondition& fc = createResult.value();

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "MESSAGE");
    EXPECT_EQ(j["op"], "NOT_CONTAINS");
    EXPECT_EQ(j["value"].get<std::string>(), "secret");
    EXPECT_EQ(j["value_type"], "STRING");
    EXPECT_FALSE(j.contains("caseSensitive"));
}

TEST_F(FilterJsonTest, FilterConditionFromJson_StringNotContains) {
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"op", "NOT_CONTAINS"},
        {"value", "secret"},
        {"value_type", "STRING"},
        {"caseSensitive", false}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::MESSAGE);
    EXPECT_EQ(fc.op, FilterOperator::NOT_CONTAINS);
    ASSERT_TRUE(std::holds_alternative<std::string>(fc.value));
    EXPECT_EQ(std::get<std::string>(fc.value), "secret");
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
    EXPECT_EQ(fc.caseSensitive, false);
}
