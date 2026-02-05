// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 eserlxl

#include <gtest/gtest.h>
#include "filter/Core.h"
#include "core/LogTypes.h"
#include <nlohmann/json.hpp>
#include <optional>

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

TEST_F(FilterJsonTest, FilterRuleFromJsonInvalidField) {
    nlohmann::json j = {
        {"field", "non_existent_field"},
        {"op", "EQUALS"},
        {"value", "INFO"}
    };

    FilterRule fr;
    auto result = from_json(j, fr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Unrecognized field"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/field"); // Verify jsonPath
}

TEST_F(FilterJsonTest, FilterRuleFromJsonMissingField) {
    nlohmann::json j = {
        {"op", "EQUALS"},
        {"value", "INFO"}
    };

    FilterRule fr;
    auto result = from_json(j, fr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Missing required key"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/field"); // Verify jsonPath
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
    EXPECT_EQ(j["value"], "2023-01-01T00:00:00Z");
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
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    EXPECT_EQ(fc.field, LogEntryField::MESSAGE);
    EXPECT_EQ(fc.op, FilterOperator::CONTAINS);
    EXPECT_EQ(fc.value, "warning");
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
    ASSERT_TRUE(result.has_value()) << result.error().toString();

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
    auto result = from_json(j, fc, "/");
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
    ASSERT_TRUE(result.has_value()) << result.error().toString();

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
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("DATETIME value_type requires 'datetimeFormat'"), std::string::npos);
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

TEST_F(FilterJsonTest, FilterRuleFromJsonMissingOp) {
    nlohmann::json j = {
        {"field", "level"},
        {"value", "INFO"}
    };

    FilterRule fr;
    auto result = from_json(j, fr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Missing required key"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterRuleFromJsonMissingValue) {
    nlohmann::json j = {
        {"field", "level"},
        {"op", "EQUALS"}
    };

    FilterRule fr;
    auto result = from_json(j, fr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Missing required key"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterRuleFromJsonInvalidOp) {
    nlohmann::json j = {
        {"field", "level"},
        {"op", "INVALID_OP"},
        {"value", "INFO"}
    };

    FilterRule fr;
    auto result = from_json(j, fr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Unrecognized operator"), std::string::npos);
}

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
    // getOptional returns nullopt on type mismatch, triggering the missing format check
    EXPECT_NE(result.error().message.find("DATETIME value_type requires 'datetimeFormat'"), std::string::npos);
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
    ASSERT_TRUE(result.has_value()) << result.error().toString();
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
    ASSERT_TRUE(result.has_value()) << result.error().toString();
    
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
    ASSERT_TRUE(result.has_value()) << result.error().toString();
    
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
    ASSERT_TRUE(result.has_value()) << result.error().toString();
    
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
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    EXPECT_EQ(fc.field, LogEntryField::MESSAGE);
    // The new logic ignores customField if the main field is not CUSTOM or a custom string
    EXPECT_FALSE(fc.customField.has_value());
}

// Test for custom field parsing when field IS "CUSTOM" and customField is a string
TEST_F(FilterJsonTest, FilterConditionFromJsonFieldCustomAndCustomFieldString) {
    nlohmann::json j = {
        {"field", "CUSTOM"}, // Explicitly CUSTOM
        {"op", "EQUALS"},
        {"value", "custom_val"},
        {"value_type", "STRING"},
        {"customField", "my_dynamic_key"} // customField provided
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    EXPECT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "my_dynamic_key");
}

// Test for custom field parsing when field IS "CUSTOM" and customField is null
TEST_F(FilterJsonTest, FilterConditionFromJsonFieldCustomAndCustomFieldNull) {
    nlohmann::json j = {
        {"field", "CUSTOM"}, // Explicitly CUSTOM
        {"op", "EQUALS"},
        {"value", "custom_val"},
        {"value_type", "STRING"},
        {"customField", nullptr} // customField is null
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    // This is now an error case. If field is "CUSTOM", a customField must be specified.
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("requires a non-empty 'customField' key"), std::string::npos);
}

// Test for custom field parsing when field IS CUSTOM, but customField is missing entirely
TEST_F(FilterJsonTest, FilterConditionFromJsonFieldCustomAndCustomFieldMissing) {
    nlohmann::json j = {
        {"field", "CUSTOM"}, // Explicitly CUSTOM
        {"op", "EQUALS"},
        {"value", "custom_val"},
        {"value_type", "STRING"}
        // customField is missing
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value()); // Should be an error because CUSTOM requires customField
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("requires a non-empty 'customField' key"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/field");
}

// Test case where field is inferred as CUSTOM and customField key is missing
TEST_F(FilterJsonTest, FilterConditionFromJsonFieldInferredCustomAndCustomFieldMissing) {
    nlohmann::json j = {
        {"field", "myDynamicKey"}, // Inferred as CUSTOM
        {"op", "EQUALS"},
        {"value", "custom_val"},
        {"value_type", "STRING"}
        // customField is missing, but it is inferred from "field"
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    ASSERT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "myDynamicKey");
}

// Test with a complex structure including custom field and datetime
TEST_F(FilterJsonTest, FilterConditionFromJsonComplex) {
    nlohmann::json j = {
        {"field", "myDynamicKey"}, // inferred custom
        {"op", "EQUALS"},
        {"value", "complex_value"},
        {"value_type", "STRING"},
        {"caseSensitive", true},
        {"customField", "myDynamicKey"} // Explicit custom field
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    EXPECT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "myDynamicKey");
    EXPECT_EQ(fc.op, FilterOperator::EQUALS);
    EXPECT_EQ(fc.value, "complex_value");
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
    EXPECT_TRUE(fc.caseSensitive);
    EXPECT_FALSE(fc.datetimeFormat.has_value());
}

TEST_F(FilterJsonTest, FilterConditionFromJsonComplexDatetime) {
    nlohmann::json j = {
        {"field", "timestamp"},
        {"op", "GREATER_THAN"},
        {"value", "2026-01-01 00:00:00"},
        {"value_type", "DATETIME"},
        {"datetimeFormat", "%Y-%m-%d %H:%M:%S"},
        {"caseSensitive", false}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    EXPECT_EQ(fc.field, LogEntryField::TIMESTAMP);
    EXPECT_EQ(fc.op, FilterOperator::GREATER_THAN);
    EXPECT_EQ(fc.value, "2026-01-01 00:00:00");
    EXPECT_EQ(fc.valueType, FilterValueType::DATETIME);
    EXPECT_TRUE(fc.datetimeFormat.has_value());
    EXPECT_EQ(*fc.datetimeFormat, "%Y-%m-%d %H:%M:%S");
    EXPECT_FALSE(fc.caseSensitive);
}

// Test for missing field with custom inferred key
TEST_F(FilterJsonTest, FilterConditionFromJsonMissingFieldWhenFieldInferredCustom) {
    nlohmann::json j = {
        {"op", "EQUALS"},
        {"value", "some_value"},
        {"value_type", "STRING"},
        {"customField", "my_key"}
        // 'field' is missing
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Missing required key"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/field"); // Verify jsonPath
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
    ASSERT_TRUE(result.has_value()) << result.error().toString();
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
    EXPECT_NE(result.error().message.find("DATETIME value_type requires 'datetimeFormat'"), std::string::npos);
}

// Test for a missing value but with an operator that might allow it (e.g., IS_NULL)
// This test depends on how IS_NULL is handled. Assuming value is required for now.
TEST_F(FilterJsonTest, FilterConditionFromJsonMissingValueForNonNullOp) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "EQUALS"}, // Assuming EQUALS requires a value
        {"value_type", "STRING"}
        // 'value' is missing
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Missing required key"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/value");
}

// Test for value being null when it shouldn't be
TEST_F(FilterJsonTest, FilterConditionFromJsonValueIsNull) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"}, // CONTAINS typically requires a non-null string value
        {"value", nullptr},
        {"value_type", "STRING"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc, "/");
    ASSERT_FALSE(result.has_value()); // value cannot be null for CONTAINS
    EXPECT_NE(result.error().message.find("Invalid type for key"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/value");
}
