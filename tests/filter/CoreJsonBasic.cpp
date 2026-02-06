// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2024 Eser KUBALI

#include <gtest/gtest.h>
#include "filter/Core.h"
#include "filter/Legacy.h" // Add this include
#include "filter/Condition.h" // Added
#include "filter/Types.h" // Added
#include "filter/Expression.h" // Added
#include "core/LogTypes.h"
#include <nlohmann/json.hpp>
#include <optional>

using namespace filter;

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

TEST_F(FilterJsonTest, FilterConditionFromJsonCustomField) {
    nlohmann::json j = {
        {"field", "non_existent_field"},
        {"op", "EQUALS"},
        {"value", "INFO"},
        {"value_type", "STRING"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value()); // Custom fields are now valid
    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "non_existent_field");
    EXPECT_EQ(fc.op, FilterOperator::EQUALS);
    EXPECT_EQ(std::get<std::string>(fc.value), "INFO");
}



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
    EXPECT_EQ(j["value"].get<std::string>(), "2023-01-01T00:00:00Z");
    EXPECT_EQ(j["value_type"], "DATETIME");
    EXPECT_EQ(j["caseSensitive"], false);
    EXPECT_EQ(j["datetimeFormat"], "%Y-%m-%dT%H:%M:%SZ");
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
    // Modified call to pass current path for error reporting
    auto result = from_json(j, fc, "/"); // Start path at root
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(fc.field, LogEntryField::MESSAGE);
    EXPECT_EQ(fc.op, FilterOperator::CONTAINS);
    EXPECT_EQ(std::get<std::string>(fc.value), "warning");
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
    EXPECT_EQ(fc.caseSensitive, true);
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
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(fc.field, LogEntryField::TIMESTAMP);
    EXPECT_EQ(fc.op, FilterOperator::LESS_THAN);
    EXPECT_EQ(std::get<std::string>(fc.value), "2023-12-31 23:59:59");
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
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "bad_field");
    EXPECT_EQ(fc.op, FilterOperator::EQUALS);
    EXPECT_EQ(std::get<std::string>(fc.value), "value");
}

TEST_F(FilterJsonTest, FilterConditionFromJsonMissingOp) {
    nlohmann::json j = {
        {"field", "message"},
        {"value", "value"},
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Missing required key"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/op"); // Verify jsonPath
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
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Unrecognized value_type"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/value_type"); // Verify jsonPath
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
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(fc.field, LogEntryField::THREAD_ID);
    EXPECT_EQ(fc.op, FilterOperator::GREATER_THAN);
    EXPECT_EQ(std::get<std::string>(fc.value), "100");
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
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("DATETIME value_type requires a non-empty 'datetimeFormat'"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/datetimeFormat"); // Verify jsonPath
}

// FilterCondition datetime constructor validation
TEST_F(FilterJsonTest, FilterConditionDatetimeConstructorValidation) {
    // Should return an error if DATETIME type is used without format via factory
    auto result = FilterCondition::createTyped(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-01-01", FilterValueType::DATETIME);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);

    // Should not return an error if DATETIME type is used with format
    EXPECT_NO_THROW({
        auto fc = FilterCondition::createDatetime(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-01-01", "%Y-%m-%d");
    });

    // Should not return an error for other value types
    EXPECT_NO_THROW({
        auto result_ok = FilterCondition::createTyped(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "error", FilterValueType::STRING);
        EXPECT_TRUE(result_ok.has_value());
    });
}



// Test for FilterOperator::IS_NULL (no value field required)
TEST_F(FilterJsonTest, FilterConditionFromJsonIsNull) {
    nlohmann::json j = {
        {"field", "thread_name"},
        {"op", "IS_NULL"},
        {"value_type", "STRING"} // value_type is still required
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(fc.field, LogEntryField::CUSTOM); // Assuming thread_name is custom
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "thread_name");
    EXPECT_EQ(fc.op, FilterOperator::IS_NULL);
    EXPECT_TRUE(std::get<std::string>(fc.value).empty()); // Value should be empty for IS_NULL
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
    EXPECT_FALSE(fc.datetimeFormat.has_value());
}

// Test for FilterOperator::IS_NOT_NULL (no value field required)
TEST_F(FilterJsonTest, FilterConditionFromJsonIsNotNull) {
    nlohmann::json j = {
        {"field", "thread_name"},
        {"op", "IS_NOT_NULL"},
        {"value_type", "STRING"} // value_type is still required
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "thread_name");
    EXPECT_EQ(fc.op, FilterOperator::IS_NOT_NULL);
    EXPECT_TRUE(std::get<std::string>(fc.value).empty()); // Value should be empty for IS_NOT_NULL
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
    EXPECT_FALSE(fc.datetimeFormat.has_value());
}

// Test for DATETIME value_type with empty datetimeFormat string
TEST_F(FilterJsonTest, FilterConditionFromJsonDatetimeEmptyFormat) {
    nlohmann::json j = {
        {"field", "timestamp"},
        {"op", "LESS_THAN"},
        {"value", "2023-12-31"},
        {"value_type", "DATETIME"},
        {"datetimeFormat", ""} // Empty string
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("DATETIME value_type requires a non-empty 'datetimeFormat'"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/datetimeFormat");
}

// Test for invalid caseSensitive type (e.g., integer)
TEST_F(FilterJsonTest, FilterConditionFromJsonInvalidCaseSensitiveTypeInt) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "test"},
        {"value_type", "STRING"},
        {"caseSensitive", 123} // Invalid type: integer
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Invalid type for key 'caseSensitive'"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/caseSensitive");
}

// Test for invalid caseSensitive type (e.g., string)
TEST_F(FilterJsonTest, FilterConditionFromJsonInvalidCaseSensitiveTypeString) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "test"},
        {"value_type", "STRING"},
        {"caseSensitive", "true"} // Invalid type: string instead of boolean
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Invalid type for key 'caseSensitive'"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/caseSensitive");
}

// Test for FilterOperator::STARTS_WITH
TEST_F(FilterJsonTest, FilterConditionFromJsonStartsWith) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "STARTS_WITH"},
        {"value", "Error"},
        {"value_type", "STRING"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(fc.op, FilterOperator::STARTS_WITH);
    EXPECT_EQ(std::get<std::string>(fc.value), "Error");
}

// Test for FilterOperator::ENDS_WITH
TEST_F(FilterJsonTest, FilterConditionFromJsonEndsWith) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "ENDS_WITH"},
        {"value", ".log"},
        {"value_type", "STRING"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(fc.op, FilterOperator::ENDS_WITH);
    EXPECT_EQ(std::get<std::string>(fc.value), ".log");
}

// Test for FilterOperator::REGEX
TEST_F(FilterJsonTest, FilterConditionFromJsonRegex) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "REGEX"},
        {"value", ".*(error|fail).*"},
        {"value_type", "STRING"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(fc.op, FilterOperator::REGEX);
    EXPECT_EQ(std::get<std::string>(fc.value), ".*(error|fail).*");
}

// Test for FilterOperator::IS_NULL with an unexpected value field
TEST_F(FilterJsonTest, FilterConditionFromJsonIsNullWithValue) {
    nlohmann::json j = {
        {"field", "thread_name"},
        {"op", "IS_NULL"},
        {"value", "some_value"}, // Value should be ignored or cause error
        {"value_type", "STRING"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value()); // It should parse successfully and ignore the value.
    EXPECT_EQ(fc.op, FilterOperator::IS_NULL);
    EXPECT_TRUE(std::get<std::string>(fc.value).empty()); // Value should be empty if ignored.
}

// Test for FilterOperator::IS_NOT_NULL with an unexpected value field
TEST_F(FilterJsonTest, FilterConditionFromJsonIsNotNullWithValue) {
    nlohmann::json j = {
        {"field", "thread_name"},
        {"op", "IS_NOT_NULL"},
        {"value", "some_value"}, // Value should be ignored or cause error
        {"value_type", "STRING"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value()); // It should parse successfully and ignore the value.
    EXPECT_EQ(fc.op, FilterOperator::IS_NOT_NULL);
    EXPECT_TRUE(std::get<std::string>(fc.value).empty()); // Value should be empty if ignored.
}

// Test for FilterValueType::BOOL with true
TEST_F(FilterJsonTest, FilterConditionFromJsonBoolTrue) {
    nlohmann::json j = {
        {"field", "flag"},
        {"op", "EQUALS"},
        {"value", true},
        {"value_type", "BOOL"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(fc.field, LogEntryField::CUSTOM); // Assuming 'flag' is a custom field
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "flag");
    EXPECT_EQ(fc.op, FilterOperator::EQUALS);
    EXPECT_EQ(std::get<std::string>(fc.value), "true");
    EXPECT_EQ(fc.valueType, FilterValueType::BOOL);
}

// Test for FilterValueType::BOOL with false
TEST_F(FilterJsonTest, FilterConditionFromJsonBoolFalse) {
    nlohmann::json j = {
        {"field", "active"},
        {"op", "EQUALS"},
        {"value", false},
        {"value_type", "BOOL"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "active");
    EXPECT_EQ(fc.op, FilterOperator::EQUALS);
    EXPECT_EQ(std::get<std::string>(fc.value), "false"); // Value stored as string "false"
    EXPECT_EQ(fc.valueType, FilterValueType::BOOL);
}

// Test for FilterValueType::BOOL with string "true"
TEST_F(FilterJsonTest, FilterConditionFromJsonBoolStringTrue) {
    nlohmann::json j = {
        {"field", "enabled"},
        {"op", "EQUALS"},
        {"value", "true"},
        {"value_type", "BOOL"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "enabled");
    EXPECT_EQ(fc.op, FilterOperator::EQUALS);
    EXPECT_EQ(std::get<std::string>(fc.value), "true"); // Value stored as string "true"
    EXPECT_EQ(fc.valueType, FilterValueType::BOOL);
}

// Test for FilterValueType::BOOL with string "false"
TEST_F(FilterJsonTest, FilterConditionFromJsonBoolStringFalse) {
    nlohmann::json j = {
        {"field", "disabled"},
        {"op", "EQUALS"},
        {"value", "false"},
        {"value_type", "BOOL"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "disabled");
    EXPECT_EQ(fc.op, FilterOperator::EQUALS);
    EXPECT_EQ(std::get<std::string>(fc.value), "false");
    EXPECT_EQ(fc.valueType, FilterValueType::BOOL);
}

// Test for FilterValueType::BOOL with invalid string value
TEST_F(FilterJsonTest, FilterConditionFromJsonBoolInvalidString) {
    nlohmann::json j = {
        {"field", "invalid_bool"},
        {"op", "EQUALS"},
        {"value", "not_a_boolean"},
        {"value_type", "BOOL"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Invalid boolean value"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/value");
}

// Test for FilterValueType::IP_ADDRESS with valid value
TEST_F(FilterJsonTest, FilterConditionFromJsonIpAddressValid) {
    nlohmann::json j = {
        {"field", "source_ip"},
        {"op", "EQUALS"},
        {"value", "192.168.1.1"},
        {"value_type", "IP_ADDRESS"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "source_ip");
    EXPECT_EQ(fc.op, FilterOperator::EQUALS);
    EXPECT_EQ(std::get<std::string>(fc.value), "192.168.1.1");
    EXPECT_EQ(fc.valueType, FilterValueType::IP_ADDRESS);
}

// Test for FilterValueType::IP_ADDRESS with invalid value
TEST_F(FilterJsonTest, FilterConditionFromJsonIpAddressInvalid) {
    nlohmann::json j = {
        {"field", "dest_ip"},
        {"op", "EQUALS"},
        {"value", "999.999.999.999"},
        {"value_type", "IP_ADDRESS"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Invalid IP address"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/value");
}

// Test for FilterValueType::VERSION with valid value
TEST_F(FilterJsonTest, FilterConditionFromJsonVersionValid) {
    nlohmann::json j = {
        {"field", "app_version"},
        {"op", "GREATER_THAN"},
        {"value", "1.2.3"},
        {"value_type", "VERSION"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "app_version");
    EXPECT_EQ(fc.op, FilterOperator::GREATER_THAN);
    EXPECT_EQ(std::get<std::string>(fc.value), "1.2.3");
    EXPECT_EQ(fc.valueType, FilterValueType::VERSION);
}

// Test for FilterValueType::VERSION with invalid value
TEST_F(FilterJsonTest, FilterConditionFromJsonVersionInvalid) {
    nlohmann::json j = {
        {"field", "os_version"},
        {"op", "LESS_THAN"},
        {"value", "invalid-version"},
        {"value_type", "VERSION"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Invalid version string"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/value");
}

// Test for FilterValueType::FLOAT with valid float string value
TEST_F(FilterJsonTest, FilterConditionFromJsonFloatStringValid) {
    nlohmann::json j = {
        {"field", "duration"},
        {"op", "GREATER_THAN"},
        {"value", "123.45"},
        {"value_type", "FLOAT"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "duration");
    EXPECT_EQ(fc.op, FilterOperator::GREATER_THAN);
    EXPECT_EQ(std::get<std::string>(fc.value), "123.45");
    EXPECT_EQ(fc.valueType, FilterValueType::FLOAT);
}

// Test for FilterValueType::FLOAT with valid integer string value
TEST_F(FilterJsonTest, FilterConditionFromJsonFloatStringIntegerValid) {
    nlohmann::json j = {
        {"field", "percentage"},
        {"op", "LESS_THAN_OR_EQUALS"},
        {"value", "99"},
        {"value_type", "FLOAT"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "percentage");
    EXPECT_EQ(fc.op, FilterOperator::LESS_THAN_OR_EQUAL);
    EXPECT_EQ(std::get<std::string>(fc.value), "99");
    EXPECT_EQ(fc.valueType, FilterValueType::FLOAT);
}

// Test for FilterValueType::FLOAT with invalid string value
TEST_F(FilterJsonTest, FilterConditionFromJsonFloatStringInvalid) {
    nlohmann::json j = {
        {"field", "invalid_float"},
        {"op", "EQUALS"},
        {"value", "not_a_float"},
        {"value_type", "FLOAT"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Invalid float value"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/value");
}

// Test for missing value but with an operator that might allow it (e.g., IS_NULL)
// This test depends on how IS_NULL is handled. Assuming value is required for now.
TEST_F(FilterJsonTest, FilterConditionFromJsonMissingValue) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Missing required key"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/value"); // Verify jsonPath
}

TEST_F(FilterJsonTest, FilterConditionFromJsonMissingValueType) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "some_value"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Missing required key"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/value_type"); // Verify jsonPath
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
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    // The error is now about the type of datetimeFormat itself.
    EXPECT_NE(result.error().message.find("Invalid type for key: 'datetimeFormat'"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/datetimeFormat"); // Verify jsonPath
}

TEST_F(FilterJsonTest, FilterConditionFromJsonInvalidValueTypeInt) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "warning"},
        {"value_type", 99}, // Invalid integer value
    };
    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Invalid integer for 'value_type'"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/value_type"); // Verify jsonPath
}

TEST_F(FilterJsonTest, FilterConditionFromJsonLegacyValueTypeInt) {
    nlohmann::json j = {
        {"field", "thread_id"},
        {"op", "EQUALS"},
        {"value", "42"},
        {"value_type", 2}, // Legacy integer for NUMERIC (INT)
    };
    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(fc.valueType, FilterValueType::INT);
}

TEST_F(FilterJsonTest, FilterConditionFromJsonCaseSensitiveDefault) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "default_case"},
        {"value_type", "STRING"}
        // caseSensitive is omitted
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());
    
    EXPECT_FALSE(fc.caseSensitive); // Should default to false
}

TEST_F(FilterJsonTest, FilterConditionFromJsonCaseSensitiveExplicitTrue) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "explicit_case"},
        {"value_type", "STRING"},
        {"caseSensitive", true} // Explicitly true
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());
    
    EXPECT_TRUE(fc.caseSensitive);
}

TEST_F(FilterJsonTest, FilterConditionFromJsonCaseSensitiveExplicitFalse) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "explicit_case"},
        {"value_type", "STRING"},
        {"caseSensitive", false} // Explicitly false
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());
    
    EXPECT_FALSE(fc.caseSensitive);
}

TEST_F(FilterJsonTest, FilterConditionFromJsonMissingField) {
    nlohmann::json j = {
        {"op", "EQUALS"},
        {"value", "INFO"},
        {"value_type", "STRING"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Missing required key"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/field"); // Verify jsonPath
}

TEST_F(FilterJsonTest, FilterConditionFromJsonFieldNotCustomButCustomFieldProvided) {
    nlohmann::json j = {
        {"field", "MESSAGE"}, // Standard field
        {"op", "CONTAINS"},
        {"value", "some text"},
        {"value_type", "STRING"},
        {"customField", "unexpected_key"} // customField provided but field is not CUSTOM
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(fc.field, LogEntryField::MESSAGE);
    // The new logic ignores customField if the main field is not CUSTOM or a custom string
    EXPECT_FALSE(fc.customField.has_value());
}


// Test for caseSensitive being null
TEST_F(FilterJsonTest, FilterConditionFromJsonCaseSensitiveNull) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "warning"},
        {"value_type", "STRING"},
        {"caseSensitive", nullptr} // caseSensitive is null
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    // Should be handled gracefully, defaulting to false.
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(fc.caseSensitive);
}

// Test for datetimeFormat being null
TEST_F(FilterJsonTest, FilterConditionFromJsonDatetimeFormatNull) {
    nlohmann::json j = {
        {"field", "timestamp"},
        {"op", "LESS_THAN"},
        {"value", "2023-12-31 23:59:59"},
        {"value_type", "DATETIME"},
        {"datetimeFormat", nullptr} // datetimeFormat is null
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("DATETIME value_type requires a non-empty 'datetimeFormat'"), std::string::npos);
}
