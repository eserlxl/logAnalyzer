// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "filter/ConcreteFilters.h"
#include "filter/Expression.h"
#include "filter/Condition.h"
#include "filter/FilterParser.h"
#include "core/LogTypes.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>

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

// Test parseQuery placeholder
TEST_F(FilterIteration15Test, ParseQueryNotImplemented) {
    auto result = parseQuery("level = INFO");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::NotImplemented);
}
