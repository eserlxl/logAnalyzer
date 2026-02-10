// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "filter/Expression.h"
#include "filter/Condition.h"
#include "filter/Types.h" // Added
#include "core/Log/Types.h"
#include <chrono>
#include <map>
#include <vector> // Added for std::vector

using namespace filter;

// Test suite for new features in Iteration 13 for FilterExpression
class Iteration13ExpressionTest : public ::testing::Test {
protected:
    FilterCondition cond1;
    FilterCondition cond2;
    FilterCondition cond3;

    Iteration13ExpressionTest()
        : cond1(FilterCondition::createString(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO").value()),
          cond2(FilterCondition::createString(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "error").value()),
          cond3(FilterCondition::createString(LogEntryField::SOURCE_FILE, FilterOperator::EQUALS, "main.cpp").value())
    {}
};

TEST_F(Iteration13ExpressionTest, ToStringBasicCondition) {
    FilterExpression expr(cond1);
    EXPECT_EQ(expr.toString(), "LEVEL EQUALS \"INFO\"");
}

TEST_F(Iteration13ExpressionTest, ToStringWithNegation) {
    FilterExpression expr = FilterExpression(cond1).Not();
    EXPECT_EQ(expr.toString(), "NOT (LEVEL EQUALS \"INFO\")");
}

TEST_F(Iteration13ExpressionTest, ToStringComplexExpression) {
    FilterExpression expr = FilterExpression(cond1).And(FilterExpression(cond2).Not());
    EXPECT_EQ(expr.toString(), "(LEVEL EQUALS \"INFO\" AND NOT (MESSAGE CONTAINS \"error\"))");
}

TEST_F(Iteration13ExpressionTest, ToStringDoubleNegation) {
    FilterExpression expr = FilterExpression(cond1).Not().Not();
    // A double NOT should simplify back to the original expression string.
    // The implementation of toString handles this by creating a temporary non-negated expression.
    // Let's verify NOT(NOT(X)) becomes X, not NOT(NOT(X)) literally.
    FilterExpression notExpr = FilterExpression(cond1).Not();
    FilterExpression doubleNotExpr = notExpr.Not();
    EXPECT_EQ(doubleNotExpr.toString(), "LEVEL EQUALS \"INFO\"");
}

TEST_F(Iteration13ExpressionTest, CloneIsDeepCopy) {
    FilterExpression original = FilterExpression(cond1).Or(FilterExpression(cond2));
    FilterExpression cloned = original.clone();

    // Ensure they are identical at first
    EXPECT_EQ(original.toString(), cloned.toString());

    // Modify the original
    original = original.And(FilterExpression(cond3));

    // Ensure the clone is unaffected
    EXPECT_NE(original.toString(), cloned.toString());
    EXPECT_EQ(cloned.toString(), "(LEVEL EQUALS \"INFO\" OR MESSAGE CONTAINS \"error\")");
}

TEST_F(Iteration13ExpressionTest, MakeEmpty) {
    FilterExpression emptyExpr = FilterExpression::makeEmpty();
    EXPECT_EQ(emptyExpr.toString(), "EMPTY");
    EXPECT_TRUE(emptyExpr.evaluate(LogEntry{}).value());

    FilterExpression negatedEmpty = FilterExpression::makeEmpty(true);
    EXPECT_EQ(negatedEmpty.toString(), "NOT (EMPTY)");
    EXPECT_FALSE(negatedEmpty.evaluate(LogEntry{}).value());
}

TEST_F(Iteration13ExpressionTest, MakeCondition) {
    FilterExpression expr = FilterExpression::makeCondition(cond1);
    EXPECT_EQ(expr.toString(), "LEVEL EQUALS \"INFO\"");

    FilterExpression negatedExpr = FilterExpression::makeCondition(cond1, true);
    EXPECT_EQ(negatedExpr.toString(), "NOT (LEVEL EQUALS \"INFO\")");
}

TEST_F(Iteration13ExpressionTest, MakeAnd) {
    std::vector<FilterExpression> expressions = {
        FilterExpression(cond1),
        FilterExpression(cond2)
    };
    FilterExpression andExpr = FilterExpression::makeAnd(expressions);
    EXPECT_EQ(andExpr.toString(), "(LEVEL EQUALS \"INFO\" AND MESSAGE CONTAINS \"error\")");
}

TEST_F(Iteration13ExpressionTest, MakeOr) {
    std::vector<FilterExpression> expressions = {
        FilterExpression(cond1),
        FilterExpression(cond2)
    };
    FilterExpression orExpr = FilterExpression::makeOr(expressions);
    EXPECT_EQ(orExpr.toString(), "(LEVEL EQUALS \"INFO\" OR MESSAGE CONTAINS \"error\")");
}

TEST_F(Iteration13ExpressionTest, MakeNot) {
    FilterExpression expr(cond1);
    FilterExpression notExpr = FilterExpression::makeNot(expr);
    EXPECT_EQ(notExpr.toString(), "NOT (LEVEL EQUALS \"INFO\")");

    // Test double negation
    FilterExpression doubleNotExpr = FilterExpression::makeNot(notExpr);
    EXPECT_EQ(doubleNotExpr.toString(), "LEVEL EQUALS \"INFO\"");
}

TEST_F(Iteration13ExpressionTest, MakeAndWithFlattening) {
    FilterExpression innerAnd = FilterExpression::makeAnd({FilterExpression(cond2), FilterExpression(cond3)});
    FilterExpression outerAnd = FilterExpression::makeAnd({FilterExpression(cond1), innerAnd});

    // Expecting (cond1 AND cond2 AND cond3) without nested parentheses for the same operator
    EXPECT_EQ(outerAnd.toString(), "(LEVEL EQUALS \"INFO\" AND MESSAGE CONTAINS \"error\" AND SOURCE_FILE EQUALS \"main.cpp\")");
}

TEST_F(Iteration13ExpressionTest, MakeOrWithFlattening) {
    FilterExpression innerOr = FilterExpression::makeOr({FilterExpression(cond2), FilterExpression(cond3)});
    FilterExpression outerOr = FilterExpression::makeOr({FilterExpression(cond1), innerOr});

    EXPECT_EQ(outerOr.toString(), "(LEVEL EQUALS \"INFO\" OR MESSAGE CONTAINS \"error\" OR SOURCE_FILE EQUALS \"main.cpp\")");
}
