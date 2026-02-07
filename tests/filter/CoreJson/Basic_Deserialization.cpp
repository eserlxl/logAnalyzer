// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2024 Eser KUBALI

#include <gtest/gtest.h>
#include "filter/Core.h"
#include "filter/Legacy.h"
#include "filter/Condition.h"
#include "filter/Types.h"
#include "filter/Expression.h"
#include "core/LogTypes.h"
#include <nlohmann/json.hpp>
#include <optional>
#include "FilterJsonTestFixture.h"

using namespace filter;

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
