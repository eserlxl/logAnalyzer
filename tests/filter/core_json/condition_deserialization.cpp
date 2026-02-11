// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2024 Eser KUBALI

#include <gtest/gtest.h>
#include "core/log/types.h"
#include <nlohmann/json.hpp>
#include "filter_json_fixture.h"

using namespace filter;

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
    EXPECT_EQ(std::get<std::string>(fc.value), "warning");
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
    EXPECT_EQ(std::get<std::string>(fc.value), "specific_value");
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

TEST_F(FilterJsonTest, FromJsonSuccessLegacyValueTypeIntLogLevel) {
    nlohmann::json j = {
        {"field", "level"},
        {"op", "EQUALS"},
        {"value", "ERROR"},
        {"value_type", 11}, // Legacy integer for LOG_LEVEL
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(fc.valueType, FilterValueType::LOG_LEVEL);
}

TEST_F(FilterJsonTest, FromJsonFailureFieldIsCustomLiteral) {
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
    EXPECT_EQ(*fc.customField, "CUSTOM");
}

TEST_F(FilterJsonTest, FromJsonIgnoresCustomFieldJsonKey) {
    nlohmann::json j = {
        {"field", "myCustomFieldKey"},
        {"customField", "thisKeyShouldBeIgnored"},
        {"op", "EQUALS"},
        {"value", "specific_value"},
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(fc.field, LogEntryField::CUSTOM);
    EXPECT_TRUE(fc.customField.has_value());
    EXPECT_EQ(*fc.customField, "myCustomFieldKey");
}

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
    EXPECT_EQ(result.error().message, "DATETIME value_type requires a non-empty 'datetimeFormat'.");
}

TEST_F(FilterJsonTest, FromJsonFailureNonFiniteFloatValue) {
    nlohmann::json j_nan = {
        {"field", "message"},
        {"op", "EQUALS"},
        {"value", "nan"},
        {"value_type", "FLOAT"},
    };
    FilterCondition fc_nan;
    auto result_nan = from_json(j_nan, fc_nan);
    ASSERT_FALSE(result_nan.has_value());
    EXPECT_EQ(result_nan.error().code, Code::InvalidArgument);
    EXPECT_EQ(result_nan.error().message, "Invalid float value: nan");

    nlohmann::json j_inf = {
        {"field", "message"},
        {"op", "EQUALS"},
        {"value", "inf"},
        {"value_type", "DOUBLE"},
    };
    FilterCondition fc_inf;
    auto result_inf = from_json(j_inf, fc_inf);
    ASSERT_FALSE(result_inf.has_value());
    EXPECT_EQ(result_inf.error().code, Code::InvalidArgument);
    EXPECT_EQ(result_inf.error().message, "Invalid float value: inf");
}

TEST_F(FilterJsonTest, FromJsonFailureWhitespacePaddedNumericValue) {
    nlohmann::json j_int = {
        {"field", "line_number"},
        {"op", "EQUALS"},
        {"value", " 42"},
        {"value_type", "INT"},
    };
    FilterCondition fc_int;
    auto result_int = from_json(j_int, fc_int);
    ASSERT_FALSE(result_int.has_value());
    EXPECT_EQ(result_int.error().code, Code::InvalidArgument);
    EXPECT_EQ(result_int.error().message, "Invalid integer value:  42");

    nlohmann::json j_float = {
        {"field", "message"},
        {"op", "EQUALS"},
        {"value", "1.23 "},
        {"value_type", "FLOAT"},
    };
    FilterCondition fc_float;
    auto result_float = from_json(j_float, fc_float);
    ASSERT_FALSE(result_float.has_value());
    EXPECT_EQ(result_float.error().code, Code::InvalidArgument);
    EXPECT_EQ(result_float.error().message, "Invalid float value: 1.23 ");
}

TEST_F(FilterJsonTest, FromJsonFailurePlusPrefixedIntValue) {
    nlohmann::json j = {
        {"field", "line_number"},
        {"op", "EQUALS"},
        {"value", "+42"},
        {"value_type", "INT"},
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "Invalid integer value: +42");
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

// --- Additional from_json Deserialization Tests ---

TEST_F(FilterJsonTest, FromJsonFailureDatetimeNonParsableValue) {
    nlohmann::json j = {
        {"field", "timestamp"},
        {"op", "GREATER_THAN"},
        {"value", "not-a-datetime"},
        {"value_type", "DATETIME"},
        {"datetimeFormat", "%Y-%m-%d %H:%M:%S"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_TRUE(result.error().message.find("Failed to parse datetime value") != std::string::npos ||
                result.error().message.find("Invalid datetime string format") != std::string::npos);
}

TEST_F(FilterJsonTest, FromJsonFailureIntNonNumericValue) {
    nlohmann::json j = {
        {"field", "process_id"},
        {"op", "GREATER_THAN"},
        {"value", "not_an_int"},
        {"value_type", "INT"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "Invalid integer value: not_an_int");
}

TEST_F(FilterJsonTest, FromJsonFailureBoolNonBooleanValue) {
    nlohmann::json j = {
        {"field", "is_error"},
        {"op", "EQUALS"},
        {"value", "not_a_bool"},
        {"value_type", "BOOL"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "Invalid boolean value: not_a_bool");
}

TEST_F(FilterJsonTest, FromJsonSuccessBoolExtendedLiterals) {
    for (const std::string& value : {"t", "f", "yes", "no", "TRUE", "FALSE"}) {
        nlohmann::json j = {
            {"field", "is_error"},
            {"op", "EQUALS"},
            {"value", value},
            {"value_type", "BOOL"}
        };
        FilterCondition fc;
        auto result = from_json(j, fc);
        ASSERT_TRUE(result.has_value()) << "value=" << value << " error=" << result.error().message;
    }
}

TEST_F(FilterJsonTest, FromJsonFailureDoubleNonNumericValue) {
    nlohmann::json j = {
        {"field", "latency"},
        {"op", "GREATER_THAN"},
        {"value", "not_a_double"},
        {"value_type", "DOUBLE"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "Invalid float value: not_a_double");
}

TEST_F(FilterJsonTest, DebugGetOptionalDatetimeFormat) {
    nlohmann::json j = {
        {"datetimeFormat", "%Y-%m-%d %H:%M:%S"}
    };
    auto optionalFormat = FilterJsonUtils::getOptional<std::string>(j, "datetimeFormat");
    ASSERT_TRUE(optionalFormat.has_value());
    EXPECT_EQ(*optionalFormat, "%Y-%m-%d %H:%M:%S");

    nlohmann::json j_null = {
        {"datetimeFormat", nullptr}
    };
    auto optionalFormatNull = FilterJsonUtils::getOptional<std::string>(j_null, "datetimeFormat");
    ASSERT_FALSE(optionalFormatNull.has_value());

    nlohmann::json j_missing = {};
    auto optionalFormatMissing = FilterJsonUtils::getOptional<std::string>(j_missing, "datetimeFormat");
    ASSERT_FALSE(optionalFormatMissing.has_value());
}

TEST_F(FilterJsonTest, FromJsonFailureDatetimeEmptyFormat2) {
    nlohmann::json j = {
        {"field", "timestamp"},
        {"op", "LESS_THAN"},
        {"value", "2023-12-31 23:59:59"},
        {"value_type", "DATETIME"},
        {"datetimeFormat", ""} // Empty format string
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "DATETIME value_type requires a non-empty 'datetimeFormat'.");
}

TEST_F(FilterJsonTest, FromJsonSuccessOmittedCaseSensitiveDefaultsToFalse) {
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"op", "CONTAINS"},
        {"value", "warning"},
        {"value_type", "STRING"}
        // caseSensitive is omitted
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::MESSAGE);
    EXPECT_EQ(fc.op, FilterOperator::CONTAINS);
    EXPECT_EQ(std::get<std::string>(fc.value), "warning");
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
    EXPECT_FALSE(fc.caseSensitive); // Should default to false
    EXPECT_FALSE(fc.customField.has_value());
}

TEST_F(FilterJsonTest, FromJsonFailureDatetimeNullFormat) {
    nlohmann::json j = {
        {"field", "timestamp"},
        {"op", "LESS_THAN"},
        {"value", "2023-12-31 23:59:59"},
        {"value_type", "DATETIME"},
        {"datetimeFormat", nullptr} // Explicitly null datetimeFormat
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().message, "DATETIME value_type requires a non-empty 'datetimeFormat'.");
}

TEST_F(FilterJsonTest, FromJsonSuccessCaseSensitiveWithNonStringField) {
    // caseSensitive is primarily for string comparisons, but should deserialize correctly for other types.
    nlohmann::json j = {
        {"field", "level"},
        {"op", "EQUALS"},
        {"value", "5"},
        {"value_type", "INT"},
        {"caseSensitive", true}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::LEVEL);
    EXPECT_EQ(fc.op, FilterOperator::EQUALS);
    EXPECT_EQ(std::get<std::string>(fc.value), "5");
    EXPECT_EQ(fc.valueType, FilterValueType::INT);
    EXPECT_TRUE(fc.caseSensitive);
}
