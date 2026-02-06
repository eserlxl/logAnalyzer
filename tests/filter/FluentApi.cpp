// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "TestUtils.h"

using namespace filter;

// --- FilterExpression Fluent API & Optimization Tests ---

TEST_F(FilterTestFixture, FluentApiAndOptimization) {
    // cond1 AND cond2
    FilterExpression cond1 = createExpr(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO");
    FilterExpression cond2 = createExpr(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "user");
    FilterExpression expr1 = cond1.And(cond2);

    ASSERT_TRUE(expr1.isLogical());
    EXPECT_EQ(*expr1.getLogicalOperator(), FilterLogicalOperator::AND);
    ASSERT_EQ(expr1.getExpressions().size(), 2);

    // (cond1 AND cond2) AND cond3 -> should be flattened to AND [cond1, cond2, cond3]
    FilterExpression cond3 = createExpr(LogEntryField::SOURCE_FILE, FilterOperator::ENDS_WITH, ".log");
    FilterExpression expr2 = expr1.And(cond3);

    ASSERT_TRUE(expr2.isLogical());
    EXPECT_EQ(*expr2.getLogicalOperator(), FilterLogicalOperator::AND);
    ASSERT_EQ(expr2.getExpressions().size(), 3);
}

TEST_F(FilterTestFixture, FluentApiOrOptimization) {
    // cond1 OR cond2
    FilterExpression cond1 = createExpr(LogEntryField::LEVEL, FilterOperator::EQUALS, "ERROR");
    FilterExpression cond2 = createExpr(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "database");
    FilterExpression expr1 = cond1.Or(cond2);

    ASSERT_TRUE(expr1.isLogical());
    EXPECT_EQ(*expr1.getLogicalOperator(), FilterLogicalOperator::OR);
    ASSERT_EQ(expr1.getExpressions().size(), 2);

    // (cond1 OR cond2) OR cond3 -> should be flattened to OR [cond1, cond2, cond3]
    FilterExpression cond3 = createExpr(LogEntryField::SOURCE_FILE, FilterOperator::STARTS_WITH, "app");
    FilterExpression expr2 = expr1.Or(cond3);

    ASSERT_TRUE(expr2.isLogical());
    EXPECT_EQ(*expr2.getLogicalOperator(), FilterLogicalOperator::OR);
    ASSERT_EQ(expr2.getExpressions().size(), 3);
}

TEST_F(FilterTestFixture, FluentApiNot) {
    FilterExpression cond = createExpr(LogEntryField::LEVEL, FilterOperator::EQUALS, "DEBUG");
    FilterExpression not_cond = cond.Not();

    // The 'not_cond' should now be a copy of 'cond' but with 'negated_' flag set to true.
    ASSERT_TRUE(not_cond.isCondition()); // It's still a condition, but negated
    EXPECT_TRUE(not_cond.isNegated());   // Check the new negated flag
    EXPECT_EQ(not_cond.getCondition()->field, LogEntryField::LEVEL); // Verify it's the original condition
    EXPECT_EQ(std::get<std::string>(not_cond.getCondition()->value), "DEBUG");
    
    // NOT(NOT(cond)) should simplify back to the original cond (not negated)
    FilterExpression not_not_cond = not_cond.Not();
    ASSERT_TRUE(not_not_cond.isCondition());
    EXPECT_FALSE(not_not_cond.isNegated()); // Should no longer be negated
    EXPECT_EQ(std::get<std::string>(not_not_cond.getCondition()->value), "DEBUG");
}
