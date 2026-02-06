// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "TestUtils.h"

// --- Test for expression negation on logical operators ---
TEST_F(FilterTestFixture, NegationOfLogicalExpressions) {
    // Log entries with distinct messages to represent different logical states for conditions A, B, and C.
    auto entry_a_true_b_true = createLogEntry(LogLevel::INFO, "Contains A and B", "logic.log");   // A=T, B=T
    auto entry_a_true_b_false = createLogEntry(LogLevel::INFO, "Contains A only", "logic.log");    // A=T, B=F
    auto entry_a_false_b_true = createLogEntry(LogLevel::INFO, "Contains B only", "logic.log");    // A=F, B=T
    auto entry_a_false_b_false = createLogEntry(LogLevel::INFO, "Contains neither", "logic.log"); // A=F, B=F

    // Create conditions based on message content.
    auto cond_a = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "A"));
    auto cond_b = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "B"));

    // --- Test De Morgan's Law for AND: NOT (A AND B) == (NOT A) OR (NOT B) ---
    FilterExpression original_and = cond_a.And(cond_b);
    FilterExpression negated_and = original_and.Not();

    // Case 1: A=T, B=T. (A AND B) is TRUE. NOT(A AND B) should be FALSE.
    auto res_and_t_t = original_and.evaluate(entry_a_true_b_true);
    ASSERT_TRUE(res_and_t_t.has_value());
    EXPECT_TRUE(res_and_t_t.value());
    auto res_neg_and_t_t = negated_and.evaluate(entry_a_true_b_true);
    ASSERT_TRUE(res_neg_and_t_t.has_value());
    EXPECT_FALSE(res_neg_and_t_t.value());

    // Case 2: A=T, B=F. (A AND B) is FALSE. NOT(A AND B) should be TRUE.
    auto res_and_t_f = original_and.evaluate(entry_a_true_b_false);
    ASSERT_TRUE(res_and_t_f.has_value());
    EXPECT_FALSE(res_and_t_f.value());
    auto res_neg_and_t_f = negated_and.evaluate(entry_a_true_b_false);
    ASSERT_TRUE(res_neg_and_t_f.has_value());
    EXPECT_TRUE(res_neg_and_t_f.value());

    // Case 3: A=F, B=T. (A AND B) is FALSE. NOT(A AND B) should be TRUE.
    auto res_and_f_t = original_and.evaluate(entry_a_false_b_true);
    ASSERT_TRUE(res_and_f_t.has_value());
    EXPECT_FALSE(res_and_f_t.value());
    auto res_neg_and_f_t = negated_and.evaluate(entry_a_false_b_true);
    ASSERT_TRUE(res_neg_and_f_t.has_value());
    EXPECT_TRUE(res_neg_and_f_t.value());

    // --- Test De Morgan's Law for OR: NOT (A OR B) == (NOT A) AND (NOT B) ---
    FilterExpression original_or = cond_a.Or(cond_b);
    FilterExpression negated_or = original_or.Not();

    // Case 1: A=T, B=T. (A OR B) is TRUE. NOT(A OR B) should be FALSE.
    auto res_or_t_t = original_or.evaluate(entry_a_true_b_true);
    ASSERT_TRUE(res_or_t_t.has_value());
    EXPECT_TRUE(res_or_t_t.value());
    auto res_neg_or_t_t = negated_or.evaluate(entry_a_true_b_true);
    ASSERT_TRUE(res_neg_or_t_t.has_value());
    EXPECT_FALSE(res_neg_or_t_t.value());

    // Case 2: A=T, B=F. (A OR B) is TRUE. NOT(A OR B) should be FALSE.
    auto res_or_t_f = original_or.evaluate(entry_a_true_b_false);
    ASSERT_TRUE(res_or_t_f.has_value());
    EXPECT_TRUE(res_or_t_f.value());
    auto res_neg_or_t_f = negated_or.evaluate(entry_a_true_b_false);
    ASSERT_TRUE(res_neg_or_t_f.has_value());
    EXPECT_FALSE(res_neg_or_t_f.value());

    // Case 3: A=F, B=F. (A OR B) is FALSE. NOT(A OR B) should be TRUE.
    auto res_or_f_f = original_or.evaluate(entry_a_false_b_false);
    ASSERT_TRUE(res_or_f_f.has_value());
    EXPECT_FALSE(res_or_f_f.value());
    auto res_neg_or_f_f = negated_or.evaluate(entry_a_false_b_false);
    ASSERT_TRUE(res_neg_or_f_f.has_value());
    EXPECT_TRUE(res_neg_or_f_f.value());
}

TEST_F(FilterTestFixture, DoubleNegation) {
    auto entry_true = createLogEntry(LogLevel::INFO, "true_message", "test.log");
    auto entry_false = createLogEntry(LogLevel::INFO, "false_message", "test.log");

    auto cond_true = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::EQUALS, "true_message"));
    auto cond_false = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::EQUALS, "false_message"));

    // Test double negation on a simple condition
    FilterExpression double_negated_true = cond_true.Not().Not();
    FilterExpression double_negated_false = cond_false.Not().Not();

    auto result_double_negated_true_on_true = double_negated_true.evaluate(entry_true);
    ASSERT_TRUE(result_double_negated_true_on_true.has_value());
    EXPECT_TRUE(result_double_negated_true_on_true.value());

    auto result_double_negated_true_on_false = double_negated_true.evaluate(entry_false);
    ASSERT_TRUE(result_double_negated_true_on_false.has_value());
    EXPECT_FALSE(result_double_negated_true_on_false.value());

    auto result_double_negated_false_on_true = double_negated_false.evaluate(entry_true);
    ASSERT_TRUE(result_double_negated_false_on_true.has_value());
    EXPECT_FALSE(result_double_negated_false_on_true.value());

    auto result_double_negated_false_on_false = double_negated_false.evaluate(entry_false);
    ASSERT_TRUE(result_double_negated_false_on_false.has_value());
    EXPECT_TRUE(result_double_negated_false_on_false.value());
}

TEST_F(FilterTestFixture, ComplexNegatedExpression) {
    auto entry_1 = createLogEntry(LogLevel::INFO, "Message A, C", "source1.log"); // A=T, B=F, C=T
    auto entry_2 = createLogEntry(LogLevel::INFO, "Message B", "source2.log");     // A=F, B=T, C=F
    auto entry_3 = createLogEntry(LogLevel::INFO, "Message A", "source3.log");     // A=T, B=F, C=F
    auto entry_4 = createLogEntry(LogLevel::INFO, "Message D", "source4.log");     // A=F, B=F, C=F

    auto cond_a = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "A"));
    auto cond_b = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "B"));
    auto cond_c = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "C"));

    // Expression: NOT (A AND (B OR C))
    // Truth table:
    // Entry | A | B | C | B OR C | A AND (B OR C) | NOT (A AND (B OR C))
    // ------|---|---|---|--------|----------------|----------------------
    // entry_1 | T | F | T | T      | T              | F
    // entry_2 | F | T | F | T      | F              | T
    // entry_3 | T | F | F | F      | F              | T
    // entry_4 | F | F | F | F      | F              | T
    
    FilterExpression complex_expr = cond_a.And(cond_b.Or(cond_c)).Not();

    auto result_complex_expr_entry_1 = complex_expr.evaluate(entry_1);
    ASSERT_TRUE(result_complex_expr_entry_1.has_value());
    EXPECT_FALSE(result_complex_expr_entry_1.value());

    auto result_complex_expr_entry_2 = complex_expr.evaluate(entry_2);
    ASSERT_TRUE(result_complex_expr_entry_2.has_value());
    EXPECT_TRUE(result_complex_expr_entry_2.value());

    auto result_complex_expr_entry_3 = complex_expr.evaluate(entry_3);
    ASSERT_TRUE(result_complex_expr_entry_3.has_value());
    EXPECT_TRUE(result_complex_expr_entry_3.value());

    auto result_complex_expr_entry_4 = complex_expr.evaluate(entry_4);
    ASSERT_TRUE(result_complex_expr_entry_4.has_value());
    EXPECT_TRUE(result_complex_expr_entry_4.value());
}

TEST_F(FilterTestFixture, EvaluateErrorOnMissingField) {
    // Create a log entry that specifically does NOT have a 'customField'
    LogEntry entry;
    entry.message = "Test message";
    entry.level = LogLevel::INFO;
    entry.sourceFile = "test.log";

    // Create a condition that tries to evaluate a non-existent custom field
    auto condResult = FilterCondition::createCustomString("nonExistentField", FilterOperator::EQUALS, "value");
    ASSERT_TRUE(condResult.has_value());
    FilterExpression expr_missing_field = FilterExpression::create(condResult.value());

    // --- Test direct evaluation ---
    auto evaluation_result = expr_missing_field.evaluate(entry);
    // It should fail because the field does not exist.
    ASSERT_FALSE(evaluation_result.has_value()) << "Evaluation should have failed for a missing field, but it succeeded.";
    // Check that the error is the one we expect.
    EXPECT_EQ(evaluation_result.error().code, Code::FieldNotFound);

    // --- Test negated evaluation ---
    FilterExpression negated_expr_missing_field = expr_missing_field.Not();
    auto negated_evaluation_result = negated_expr_missing_field.evaluate(entry);
    // The error should propagate through the negation.
    ASSERT_FALSE(negated_evaluation_result.has_value()) << "Negated evaluation should have failed for a missing field, but it succeeded.";
    // Check that the propagated error is the correct one.
    EXPECT_EQ(negated_evaluation_result.error().code, Code::FieldNotFound);
}
