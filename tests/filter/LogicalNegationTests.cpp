// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "tests/filter/TestUtils.h"

// --- Test for expression negation on logical operators ---
TEST_F(FilterTestFixture, NegationOfLogicalExpressions) {
    auto entry_a_and_b = createLogEntry(LogLevel::INFO, "A and B", "logic.log"); // Assume A and B are true
    auto entry_a_not_b = createLogEntry(LogLevel::INFO, "A not B", "logic.log"); // Assume A is true, B is false
    auto entry_not_a_b = createLogEntry(LogLevel::INFO, "A not B", "logic.log"); // Assume A is false, B is true (matched by cond_c)

    // Create conditions that will be true for specific entries (simplified for this test)
    auto cond_a = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::EQUALS, "A and B")); // True for entry_a_and_b
    auto cond_b = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::EQUALS, "A and B")); // True for entry_a_and_b
    auto cond_c = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::EQUALS, "A not B")); // True for entry_a_not_b
    
    // De Morgan's Law: NOT (A AND B) == (NOT A) OR (NOT B)
    FilterExpression original_and = cond_a.And(cond_b);
    FilterExpression negated_and = original_and.Not();

    // With entry_a_and_b: original_and is TRUE. negated_and should be FALSE.
    EXPECT_TRUE(original_and.evaluate(entry_a_and_b).value_or(false));
    EXPECT_FALSE(negated_and.evaluate(entry_a_and_b).value_or(true));

    // With entry_a_not_b (A true, B false): original_and is FALSE. negated_and should be TRUE.
    EXPECT_FALSE(original_and.evaluate(entry_a_not_b).value_or(true));
    EXPECT_TRUE(negated_and.evaluate(entry_a_not_b).value_or(false));
    
    // With entry_not_a_b (A false, B true): original_and is FALSE. negated_and should be TRUE.
    EXPECT_FALSE(original_and.evaluate(entry_not_a_b).value_or(true));
    EXPECT_TRUE(negated_and.evaluate(entry_not_a_b).value_or(false));


    // De Morgan's Law: NOT (A OR B) == (NOT A) AND (NOT B)
    FilterExpression original_or = cond_a.Or(cond_c); // Using cond_c for "A not B" scenario
    FilterExpression negated_or = original_or.Not();

    // With entry_a_and_b (A true, B false): original_or is TRUE. negated_or should be FALSE.
    EXPECT_TRUE(original_or.evaluate(entry_a_and_b).value_or(false));
    EXPECT_FALSE(negated_or.evaluate(entry_a_and_b).value_or(true));
    
    // With entry_a_not_b (A true, B false): original_or is TRUE. negated_or should be FALSE.
    EXPECT_TRUE(original_or.evaluate(entry_a_not_b).value_or(false));
    EXPECT_FALSE(negated_or.evaluate(entry_a_not_b).value_or(true));

    // With entry_not_a_b (A false, B true): original_or is TRUE. negated_or should be FALSE.
    EXPECT_TRUE(original_or.evaluate(entry_not_a_b).value_or(false));
    EXPECT_FALSE(negated_or.evaluate(entry_not_a_b).value_or(true));
}
