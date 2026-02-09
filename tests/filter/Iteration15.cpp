// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "filter/ConcreteFilters.h"
#include "filter/Expression.h"
#include "filter/Condition.h"
#include "filter/Parser.h"
#include "filter/EnumStringConversions.h"
#include "core/Log/Types.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>

using namespace filter;

class FilterIteration15Test : public ::testing::Test {
protected:
    LogEntry createEntry(LogLevel level, const std::string& message, const std::map<std::string, std::string>& customFields = {}) {
        LogEntry entry;
        entry.level = level;
        entry.message = message;
        entry.customFields = customFields;
        return entry;
    }
};

// Test for FilterCondition with std::vector<std::string>
TEST_F(FilterIteration15Test, ConditionWithVectorValue) {
    auto condResult = FilterCondition::createSet(LogEntryField::MESSAGE, FilterOperator::IN, {"hello", "world"});
    ASSERT_TRUE(condResult.has_value());
    auto cond = condResult.value();

    ASSERT_TRUE(std::holds_alternative<std::vector<std::string>>(cond.value));
    auto& vec = std::get<std::vector<std::string>>(cond.value);
    ASSERT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0], "hello");
    EXPECT_EQ(vec[1], "world");

    LogEntry entry1 = createEntry(LogLevel::INFO, "world");
    LogEntry entry2 = createEntry(LogLevel::INFO, "goodbye");

    auto expr = FilterExpression(cond);
    EXPECT_TRUE(expr.evaluate(entry1).value());
    EXPECT_FALSE(expr.evaluate(entry2).value());
}

// Test JSON serialization/deserialization for vector value
TEST_F(FilterIteration15Test, ConditionVectorJsonRoundtrip) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "IN"},
        {"value", {"val1", "val2", "val3"}},
        {"value_type", "STRING"}
    };

    FilterCondition cond;
    auto fromJsonResult = from_json(j, cond, "/");
    ASSERT_TRUE(fromJsonResult.has_value());
    
    ASSERT_TRUE(std::holds_alternative<std::vector<std::string>>(cond.value));
    auto& vec = std::get<std::vector<std::string>>(cond.value);
    ASSERT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[1], "val2");

    nlohmann::json j_out;
    to_json(j_out, cond);
    EXPECT_TRUE(j_out["value"].is_array());
    EXPECT_EQ(j_out["value"].size(), 3);
}

TEST_F(FilterIteration15Test, ConditionVectorJsonFailsWithWrongOperator) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "EQUALS"},
        {"value", {"val1", "val2"}},
        {"value_type", "STRING"}
    };

    FilterCondition cond;
    auto fromJsonResult = from_json(j, cond, "/");
    ASSERT_FALSE(fromJsonResult.has_value());
    EXPECT_EQ(fromJsonResult.error().code, Code::InvalidArgument);
}

// Test ExpressionFilter bridge class
TEST_F(FilterIteration15Test, ExpressionFilterBridge) {
    nlohmann::json exprJson = {
        {"condition", {
            {"field", "level"},
            {"op", "GREATER_THAN_OR_EQUAL"},
            {"value", "WARNING"},
            {"value_type", "LOG_LEVEL"}
        }}
    };

    auto filterResult = ExpressionFilter::create(exprJson);
    ASSERT_TRUE(filterResult.has_value());
    auto filter = filterResult.value();

    LogEntry entryInfo = createEntry(LogLevel::INFO, "info message");
    LogEntry entryWarn = createEntry(LogLevel::WARNING, "warn message");

    EXPECT_FALSE(filter->matches(entryInfo));
    EXPECT_TRUE(filter->matches(entryWarn));
}

// Test FilterExpression::visit
TEST_F(FilterIteration15Test, ExpressionVisit) {
    auto cond1 = FilterCondition::createString(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "error").value();
    auto cond2 = FilterCondition::createString(LogEntryField::SOURCE_FILE, FilterOperator::EQUALS, "main.cpp").value();
    
    FilterExpression expr = FilterExpression::makeAnd({FilterExpression(cond1), FilterExpression(cond2)});

    int visitCount = 0;
    std::vector<std::string> visitedFields;
    expr.visit([&](const FilterCondition& c) {
        visitCount++;
        if (c.customField) {
            visitedFields.push_back(*c.customField);
        } else {
            visitedFields.push_back(Utils::logEntryFieldToString(c.field));
        }
    });

    EXPECT_EQ(visitCount, 2);
    EXPECT_NE(std::find(visitedFields.begin(), visitedFields.end(), "MESSAGE"), visitedFields.end());
    EXPECT_NE(std::find(visitedFields.begin(), visitedFields.end(), "SOURCE_FILE"), visitedFields.end());
}

// Test FilterExpression::simplify (basic test)
TEST_F(FilterIteration15Test, ExpressionSimplify) {
    auto cond = FilterCondition::createString(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "error").value();
    FilterExpression expr(cond);
    FilterExpression simplified = expr.simplify();

    // Basic test: simplifying a single condition should not change it.
    EXPECT_EQ(simplified.getType(), FilterExpression::ExpressionType::CONDITION);
    EXPECT_EQ(simplified.getCondition()->op, FilterOperator::CONTAINS);
}

TEST_F(FilterIteration15Test, ParseQuerySimpleCondition) {
    auto result = parseQuery("level = INFO");
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    LogEntry infoEntry = createEntry(LogLevel::INFO, "ok");
    LogEntry errorEntry = createEntry(LogLevel::ERROR, "fail");
    EXPECT_TRUE(result->evaluate(infoEntry).value_or(false));
    EXPECT_FALSE(result->evaluate(errorEntry).value_or(true));
}

TEST_F(FilterIteration15Test, ParseQueryLogicalExpressionWithSet) {
    auto result = parseQuery("NOT (level = DEBUG) AND message IN ('hello', 'world')");
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    LogEntry matchEntry = createEntry(LogLevel::INFO, "world");
    LogEntry blockedByNot = createEntry(LogLevel::DEBUG, "world");
    LogEntry blockedBySet = createEntry(LogLevel::INFO, "other");

    EXPECT_TRUE(result->evaluate(matchEntry).value_or(false));
    EXPECT_FALSE(result->evaluate(blockedByNot).value_or(true));
    EXPECT_FALSE(result->evaluate(blockedBySet).value_or(true));
}

TEST_F(FilterIteration15Test, ParseQueryLogicalOperatorAliases) {
    auto result = parseQuery("!(level = DEBUG) && (message CONTAINS 'hello' || message CONTAINS 'world')");
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    LogEntry matchHello = createEntry(LogLevel::INFO, "say hello");
    LogEntry matchWorld = createEntry(LogLevel::WARNING, "world event");
    LogEntry blockedByNot = createEntry(LogLevel::DEBUG, "hello");
    LogEntry blockedByContent = createEntry(LogLevel::INFO, "other");

    EXPECT_TRUE(result->evaluate(matchHello).value_or(false));
    EXPECT_TRUE(result->evaluate(matchWorld).value_or(false));
    EXPECT_FALSE(result->evaluate(blockedByNot).value_or(true));
    EXPECT_FALSE(result->evaluate(blockedByContent).value_or(true));
}

TEST_F(FilterIteration15Test, ParseQueryMalformedDiagnostics) {
    auto missingValue = parseQuery("level = ");
    ASSERT_FALSE(missingValue.has_value());
    EXPECT_THAT(missingValue.error().message, testing::HasSubstr("Expected value in condition."));

    auto missingRhs = parseQuery("level = INFO AND");
    ASSERT_FALSE(missingRhs.has_value());
    EXPECT_THAT(missingRhs.error().message, testing::HasSubstr("Expected field name in condition."));
}

TEST_F(FilterIteration15Test, ParseQuerySupportsOperatorTokenAliases) {
    LogEntry entry = createEntry(LogLevel::WARNING, "alpha world", {
        {"scope", "backend"},
        {"tag", "World"},
        {"opt", "yes"},
    });

    auto notInRes = parseQuery("message NOT_IN ('x', 'y')");
    ASSERT_TRUE(notInRes.has_value()) << notInRes.error().toString();
    EXPECT_TRUE(notInRes->evaluate(entry).value_or(false));

    auto regexAliasRes = parseQuery("message REGEX_MATCH 'alpha.*'");
    ASSERT_TRUE(regexAliasRes.has_value()) << regexAliasRes.error().toString();
    EXPECT_TRUE(regexAliasRes->evaluate(entry).value_or(false));

    auto containsIRes = parseQuery("custom.tag CONTAINS_I 'world'");
    ASSERT_TRUE(containsIRes.has_value()) << containsIRes.error().toString();
    EXPECT_TRUE(containsIRes->evaluate(entry).value_or(false));

    auto equalsIRes = parseQuery("custom.tag EQUALS_I 'world'");
    ASSERT_TRUE(equalsIRes.has_value()) << equalsIRes.error().toString();
    EXPECT_TRUE(equalsIRes->evaluate(entry).value_or(false));

    auto notEqualsIRes = parseQuery("custom.tag NOT_EQUALS_I 'w0rld'");
    ASSERT_TRUE(notEqualsIRes.has_value()) << notEqualsIRes.error().toString();
    EXPECT_TRUE(notEqualsIRes->evaluate(entry).value_or(false));

    auto startsWithIRes = parseQuery("custom.tag STARTS_WITH_I 'wo'");
    ASSERT_TRUE(startsWithIRes.has_value()) << startsWithIRes.error().toString();
    EXPECT_TRUE(startsWithIRes->evaluate(entry).value_or(false));

    auto endsWithIRes = parseQuery("custom.tag ENDS_WITH_I 'LD'");
    ASSERT_TRUE(endsWithIRes.has_value()) << endsWithIRes.error().toString();
    EXPECT_TRUE(endsWithIRes->evaluate(entry).value_or(false));

    auto isPresentRes = parseQuery("custom.scope IS_PRESENT");
    ASSERT_TRUE(isPresentRes.has_value()) << isPresentRes.error().toString();
    EXPECT_TRUE(isPresentRes->evaluate(entry).value_or(false));

    auto isAbsentRes = parseQuery("custom.missing IS_ABSENT");
    ASSERT_TRUE(isAbsentRes.has_value()) << isAbsentRes.error().toString();
    EXPECT_TRUE(isAbsentRes->evaluate(entry).value_or(false));

    auto isNotNullRes = parseQuery("custom.scope IS_NOT_NULL");
    ASSERT_TRUE(isNotNullRes.has_value()) << isNotNullRes.error().toString();
    EXPECT_TRUE(isNotNullRes->evaluate(entry).value_or(false));

    auto isNullRes = parseQuery("custom.missing IS_NULL");
    ASSERT_TRUE(isNullRes.has_value()) << isNullRes.error().toString();
    EXPECT_TRUE(isNullRes->evaluate(entry).value_or(false));
}

TEST_F(FilterIteration15Test, FilterOperatorToStringCoversAllIteration15Operators) {
    EXPECT_EQ(toString(FilterOperator::EQUALS_I), "EQUALS_I");
    EXPECT_EQ(toString(FilterOperator::NOT_EQUALS_I), "NOT_EQUALS_I");
    EXPECT_EQ(toString(FilterOperator::CONTAINS_I), "CONTAINS_I");
    EXPECT_EQ(toString(FilterOperator::NOT_CONTAINS_I), "NOT_CONTAINS_I");
    EXPECT_EQ(toString(FilterOperator::STARTS_WITH_I), "STARTS_WITH_I");
    EXPECT_EQ(toString(FilterOperator::ENDS_WITH_I), "ENDS_WITH_I");
    EXPECT_EQ(toString(FilterOperator::IN), "IN");
    EXPECT_EQ(toString(FilterOperator::NOT_IN), "NOT_IN");
}

TEST_F(FilterIteration15Test, FilterValueTypeUnknownRoundTripAliases) {
    EXPECT_EQ(toString(FilterValueType::UNKNOWN), "UNKNOWN");
    EXPECT_EQ(fromStringToFilterValueType("UNKNOWN"), FilterValueType::UNKNOWN);
    EXPECT_EQ(fromStringToFilterValueType("UNKNOWN_VALUE_TYPE"), FilterValueType::UNKNOWN);
}

TEST_F(FilterIteration15Test, ParseQueryFieldAliasesMapToStandardFields) {
    LogEntry entry{};
    entry.sourceFile = "app.log";
    entry.sourceLineNumber = 42;
    entry.threadId = "worker-1";
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = LogLevel::INFO;
    entry.message = "ok";

    auto sourceRes = parseQuery("source = 'app.log'");
    ASSERT_TRUE(sourceRes.has_value()) << sourceRes.error().toString();
    EXPECT_TRUE(sourceRes->evaluate(entry).value_or(false));

    auto lineRes = parseQuery("line = 42");
    ASSERT_TRUE(lineRes.has_value()) << lineRes.error().toString();
    EXPECT_TRUE(lineRes->evaluate(entry).value_or(false));

    auto threadRes = parseQuery("thread = 'worker-1'");
    ASSERT_TRUE(threadRes.has_value()) << threadRes.error().toString();
    EXPECT_TRUE(threadRes->evaluate(entry).value_or(false));
}

TEST_F(FilterIteration15Test, ParseQuerySupportsRelationalShorthandTokens) {
    LogEntry entry{};
    entry.level = LogLevel::INFO;
    entry.message = "ok";
    entry.sourceLineNumber = 42;

    auto eqRes = parseQuery("line EQ 42");
    ASSERT_TRUE(eqRes.has_value()) << eqRes.error().toString();
    EXPECT_TRUE(eqRes->evaluate(entry).value_or(false));

    auto neRes = parseQuery("line NE 43");
    ASSERT_TRUE(neRes.has_value()) << neRes.error().toString();
    EXPECT_TRUE(neRes->evaluate(entry).value_or(false));

    auto gtRes = parseQuery("line GT 41");
    ASSERT_TRUE(gtRes.has_value()) << gtRes.error().toString();
    EXPECT_TRUE(gtRes->evaluate(entry).value_or(false));

    auto ltRes = parseQuery("line LT 43");
    ASSERT_TRUE(ltRes.has_value()) << ltRes.error().toString();
    EXPECT_TRUE(ltRes->evaluate(entry).value_or(false));

    auto gteRes = parseQuery("line GTE 42");
    ASSERT_TRUE(gteRes.has_value()) << gteRes.error().toString();
    EXPECT_TRUE(gteRes->evaluate(entry).value_or(false));

    auto lteRes = parseQuery("line LTE 42");
    ASSERT_TRUE(lteRes.has_value()) << lteRes.error().toString();
    EXPECT_TRUE(lteRes->evaluate(entry).value_or(false));

    auto gteEqualsRes = parseQuery("line GREATER_THAN_OR_EQUALS 42");
    ASSERT_TRUE(gteEqualsRes.has_value()) << gteEqualsRes.error().toString();
    EXPECT_TRUE(gteEqualsRes->evaluate(entry).value_or(false));

    auto lteEqualsRes = parseQuery("line LESS_THAN_OR_EQUALS 42");
    ASSERT_TRUE(lteEqualsRes.has_value()) << lteEqualsRes.error().toString();
    EXPECT_TRUE(lteEqualsRes->evaluate(entry).value_or(false));
}
