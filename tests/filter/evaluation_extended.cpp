// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "tests/utils/test_utils.h"

namespace {
    // Helper to create a FilterExpression from a JSON object
    // This is the preferred way to test conditions, as it exercises the full
    // from_json -> pre-computation -> evaluation pipeline.
    filter::FilterExpression createExprFromJson(const nlohmann::json& j) {
        filter::FilterCondition cond;
        auto result = filter::from_json(j, cond);
        if (!result) {
            // Fail the test if JSON parsing fails, to see the error.
            ADD_FAILURE() << "Failed to parse FilterCondition from JSON: " << result.error().toString();
        }
        return filter::FilterExpression::create(cond);
    }
}

// --- Extended Evaluation Tests for Iteration 2 ---

TEST_F(FilterTestFixture, EvaluateLogLevelComparison) {
    auto entryInfo = createLogEntry(LogLevel::INFO, "Info message");
    auto entryWarn = createLogEntry(LogLevel::WARNING, "Warning message");
    auto entryError = createLogEntry(LogLevel::ERROR, "Error message");

    // EQUALS
    auto exprEq = createExpr(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO", filter::FilterValueType::LOG_LEVEL);
    EXPECT_TRUE(exprEq.evaluate(entryInfo).value_or(false));
    EXPECT_FALSE(exprEq.evaluate(entryWarn).value_or(true));

    // GREATER_THAN (ERROR > WARNING)
    auto exprGt = createExpr(LogEntryField::LEVEL, FilterOperator::GREATER_THAN, "WARNING", filter::FilterValueType::LOG_LEVEL);
    EXPECT_TRUE(exprGt.evaluate(entryError).value_or(false));
    EXPECT_FALSE(exprGt.evaluate(entryWarn).value_or(true));
    EXPECT_FALSE(exprGt.evaluate(entryInfo).value_or(true));

    // LESS_THAN (INFO < WARNING)
    auto exprLt = createExpr(LogEntryField::LEVEL, FilterOperator::LESS_THAN, "WARNING", filter::FilterValueType::LOG_LEVEL);
    EXPECT_TRUE(exprLt.evaluate(entryInfo).value_or(false));
    EXPECT_FALSE(exprLt.evaluate(entryWarn).value_or(true));
    EXPECT_FALSE(exprLt.evaluate(entryError).value_or(true));

    // GREATER_THAN_OR_EQUAL
    auto exprGte = createExpr(LogEntryField::LEVEL, FilterOperator::GREATER_THAN_OR_EQUAL, "WARNING", filter::FilterValueType::LOG_LEVEL);
    EXPECT_TRUE(exprGte.evaluate(entryError).value_or(false));
    EXPECT_TRUE(exprGte.evaluate(entryWarn).value_or(false));
    EXPECT_FALSE(exprGte.evaluate(entryInfo).value_or(true));
}

TEST_F(FilterTestFixture, EvaluateAutoTypeInferenceFromJson) {
    auto entryInt = createLogEntry(LogLevel::INFO, "message", "test.log", {{"value", "123"}});
    auto entryDbl = createLogEntry(LogLevel::INFO, "message", "test.log", {{"value", "123.45"}});
    auto entryBool = createLogEntry(LogLevel::INFO, "message", "test.log", {{"value", "true"}});
    auto entryStr = createLogEntry(LogLevel::INFO, "message", "test.log", {{"value", "hello"}});

    // AUTO INT
    auto exprInt = createExprFromJson({{"field", "value"}, {"op", "EQUALS"}, {"value", 123}, {"value_type", "AUTO"}});
    EXPECT_TRUE(exprInt.evaluate(entryInt).value_or(false));
    EXPECT_FALSE(exprInt.evaluate(entryDbl).value_or(true)); // Type mismatch should be false, not error

    // AUTO DOUBLE
    auto exprDbl = createExprFromJson({{"field", "value"}, {"op", "EQUALS"}, {"value", 123.45}, {"value_type", "AUTO"}});
    EXPECT_TRUE(exprDbl.evaluate(entryDbl).value_or(false));
    EXPECT_FALSE(exprDbl.evaluate(entryInt).value_or(true));

    // AUTO BOOL
    auto exprBool = createExprFromJson({{"field", "value"}, {"op", "EQUALS"}, {"value", true}, {"value_type", "AUTO"}});
    EXPECT_TRUE(exprBool.evaluate(entryBool).value_or(false));
    EXPECT_FALSE(exprBool.evaluate(entryStr).value_or(true));

    // AUTO STRING
    auto exprStr = createExprFromJson({{"field", "value"}, {"op", "EQUALS"}, {"value", "hello"}, {"value_type", "AUTO"}});
    EXPECT_TRUE(exprStr.evaluate(entryStr).value_or(false));
    EXPECT_FALSE(exprStr.evaluate(entryInt).value_or(true));
    
    // AUTO without value_type key (should default to AUTO)
    auto exprAutoDefault = createExprFromJson({{"field", "value"}, {"op", "EQUALS"}, {"value", 123}});
    EXPECT_TRUE(exprAutoDefault.evaluate(entryInt).value_or(false));
}


TEST_F(FilterTestFixture, EvaluateInOperatorWithMixedTypesFromJson) {
    auto entryStr = createLogEntry(LogLevel::INFO, "message", "test.log", {{"value", "hello"}});
    auto entryInt = createLogEntry(LogLevel::INFO, "message", "test.log", {{"value", "123"}});
    auto entryDbl = createLogEntry(LogLevel::INFO, "message", "test.log", {{"value", "45.6"}});
    auto entryBool = createLogEntry(LogLevel::INFO, "message", "test.log", {{"value", "true"}});

    auto expr = createExprFromJson({
        {"field", "value"}, 
        {"op", "IN"}, 
        {"value", {"hello", 123, 45.6, true}}, 
        {"value_type", "AUTO"}
    });

    EXPECT_TRUE(expr.evaluate(entryStr).value_or(false));
    EXPECT_TRUE(expr.evaluate(entryInt).value_or(false));
    EXPECT_TRUE(expr.evaluate(entryDbl).value_or(false));
    EXPECT_TRUE(expr.evaluate(entryBool).value_or(false));
    
    // Test a value not in the list
    auto entryNotIn = createLogEntry(LogLevel::INFO, "message", "test.log", {{"value", "not_in_list"}});
    EXPECT_FALSE(expr.evaluate(entryNotIn).value_or(true));
}

TEST_F(FilterTestFixture, EvaluateFloatPrecisionEdgeCases) {
    // Test with very small numbers
    auto entrySmall = createLogEntry(LogLevel::INFO, "message", "test.log", {{"value", "0.000000001"}});
    auto exprSmall = createExprFromJson({{"field", "value"}, {"op", "EQUALS"}, {"value", 0.0000000011}, {"value_type", "DOUBLE"}});
    EXPECT_TRUE(exprSmall.evaluate(entrySmall).value_or(false)); // Should be almost equal

    // Test NOT_EQUALS for almost-equal numbers
    auto exprNotEqSmall = createExprFromJson({{"field", "value"}, {"op", "NOT_EQUALS"}, {"value", 0.0000000011}, {"value_type", "DOUBLE"}});
    EXPECT_FALSE(exprNotEqSmall.evaluate(entrySmall).value_or(true));

    // Test with very large numbers
    auto entryLarge = createLogEntry(LogLevel::INFO, "message", "test.log", {{"value", "1234567890.123456"}});
    auto exprLarge = createExprFromJson({{"field", "value"}, {"op", "EQUALS"}, {"value", 1234567890.123457}, {"value_type", "DOUBLE"}});
    EXPECT_TRUE(exprLarge.evaluate(entryLarge).value_or(false)); // Should be almost equal

    // Test relational operators for almost-equal numbers
    auto entryAlmost = createLogEntry(LogLevel::INFO, "message", "test.log", {{"value", "1.0000000001"}});
    
    auto exprGtAlmost = createExprFromJson({{"field", "value"}, {"op", "GREATER_THAN"}, {"value", 1.0}, {"value_type", "DOUBLE"}});
    EXPECT_FALSE(exprGtAlmost.evaluate(entryAlmost).value_or(true)); // 1.000...1 is not strictly > 1.0 if they are almost equal
    
    auto exprLtAlmost = createExprFromJson({{"field", "value"}, {"op", "LESS_THAN"}, {"value", 1.0000000002}, {"value_type", "DOUBLE"}});
    EXPECT_FALSE(exprLtAlmost.evaluate(entryAlmost).value_or(true)); // not strictly < if almost equal
    
    auto exprGteAlmost = createExprFromJson({{"field", "value"}, {"op", "GREATER_THAN_OR_EQUAL"}, {"value", 1.0}, {"value_type", "DOUBLE"}});
    EXPECT_TRUE(exprGteAlmost.evaluate(entryAlmost).value_or(false));
}

TEST_F(FilterTestFixture, EvaluateExplicitCaseInsensitiveOperators) {
    auto entry = createLogEntry(LogLevel::INFO, "Hello World", "test.log");

    // EQUALS_I
    auto exprEqI = createExprFromJson({{"field", "message"}, {"op", "EQUALS_I"}, {"value", "hello world"}});
    EXPECT_TRUE(exprEqI.evaluate(entry).value_or(false));
    
    // NOT_EQUALS_I
    auto exprNotEqI = createExprFromJson({{"field", "message"}, {"op", "NOT_EQUALS_I"}, {"value", "hello world"}});
    EXPECT_FALSE(exprNotEqI.evaluate(entry).value_or(true));

    // CONTAINS_I
    auto exprContainsI = createExprFromJson({{"field", "message"}, {"op", "CONTAINS_I"}, {"value", "WORLD"}});
    EXPECT_TRUE(exprContainsI.evaluate(entry).value_or(false));

    // NOT_CONTAINS_I
    auto exprNotContainsI = createExprFromJson({{"field", "message"}, {"op", "NOT_CONTAINS_I"}, {"value", "WORLD"}});
    EXPECT_FALSE(exprNotContainsI.evaluate(entry).value_or(true));

    // STARTS_WITH_I
    auto exprStartsI = createExprFromJson({{"field", "message"}, {"op", "STARTS_WITH_I"}, {"value", "HeLLo"}});
    EXPECT_TRUE(exprStartsI.evaluate(entry).value_or(false));
    
    // ENDS_WITH_I
    auto exprEndsI = createExprFromJson({{"field", "message"}, {"op", "ENDS_WITH_I"}, {"value", "WoRlD"}});
    EXPECT_TRUE(exprEndsI.evaluate(entry).value_or(false));
}


TEST_F(FilterTestFixture, EvaluateNullAliases) {
    LogEntry entryWithHost;
    entryWithHost.host = "server-1";
    LogEntry entryWithoutHost;
    entryWithoutHost.host = std::nullopt;

    // IS_NULL (alias for IS_ABSENT)
    auto exprIsNull = createExprFromJson({{"field", "host"}, {"op", "IS_NULL"}});
    EXPECT_TRUE(exprIsNull.evaluate(entryWithoutHost).value_or(false));
    EXPECT_FALSE(exprIsNull.evaluate(entryWithHost).value_or(true));

    // IS_NOT_NULL (alias for IS_PRESENT)
    auto exprIsNotNull = createExprFromJson({{"field", "host"}, {"op", "IS_NOT_NULL"}});
    EXPECT_TRUE(exprIsNotNull.evaluate(entryWithHost).value_or(false));
    EXPECT_FALSE(exprIsNotNull.evaluate(entryWithoutHost).value_or(true));
}

TEST_F(FilterTestFixture, EvaluateInvalidRegexError) {
    auto entry = createLogEntry(LogLevel::INFO, "any message");
    
    nlohmann::json j = {
        {"field", "message"},
        {"op", "REGEX"},
        {"value", "["} // Invalid regex pattern
    };

    filter::FilterCondition cond;
    auto result = filter::from_json(j, cond);

    // The error should be caught during parsing/pre-computation
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidRegex);
    EXPECT_THAT(result.error().message, ::testing::HasSubstr("Invalid regex pattern"));
}
