// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2024 Eser KUBALI

#include <gtest/gtest.h>
#include "core/log/types.h"
#include <nlohmann/json.hpp>
#include "filter_json_fixture.h"

using namespace filter;

// --- from_json Deserialization Failure Tests ---

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

// --- Additional from_json Deserialization Failure Tests ---

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
