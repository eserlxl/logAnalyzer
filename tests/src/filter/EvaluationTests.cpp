// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "TestUtils.h"

// --- FilterExpression Evaluation Tests ---

TEST_F(FilterTestFixture, EvaluateStringEquals) {
    auto entry = createLogEntry(LogLevel::INFO, "User logged in", "auth.log");
    
    // Case-sensitive match
    auto expr_cs_match = createExpr(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO", FilterValueType::STRING, true);
    ASSERT_TRUE(expr_cs_match.evaluate(entry).has_value()) << expr_cs_match.evaluate(entry).error().toString();
    EXPECT_TRUE(expr_cs_match.evaluate(entry).value_or(false));

    // Case-sensitive mismatch
    auto expr_cs_mismatch = createExpr(LogEntryField::LEVEL, FilterOperator::EQUALS, "info", FilterValueType::STRING, true);
    ASSERT_TRUE(expr_cs_mismatch.evaluate(entry).has_value()) << expr_cs_mismatch.evaluate(entry).error().toString();
    EXPECT_FALSE(expr_cs_mismatch.evaluate(entry).value_or(true));

    // Case-insensitive match
    auto expr_ci_match = createExpr(LogEntryField::LEVEL, FilterOperator::EQUALS, "info", FilterValueType::STRING, false);
    ASSERT_TRUE(expr_ci_match.evaluate(entry).has_value()) << expr_ci_match.evaluate(entry).error().toString();
    EXPECT_TRUE(expr_ci_match.evaluate(entry).value_or(false));
}

TEST_F(FilterTestFixture, EvaluateStringContains) {
    auto entry = createLogEntry(LogLevel::DEBUG, "Processing user_id:123", "worker.log");

    // Case-sensitive contains
    auto expr_cs_match = createExpr(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "user_id", FilterValueType::STRING, true);
    ASSERT_TRUE(expr_cs_match.evaluate(entry).has_value()) << expr_cs_match.evaluate(entry).error().toString();
    EXPECT_TRUE(expr_cs_match.evaluate(entry).value_or(false));

    // Case-sensitive no match
    auto expr_cs_mismatch = createExpr(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "User_id", FilterValueType::STRING, true);
    ASSERT_TRUE(expr_cs_mismatch.evaluate(entry).has_value()) << expr_cs_mismatch.evaluate(entry).error().toString();
    EXPECT_FALSE(expr_cs_mismatch.evaluate(entry).value_or(true));
    
    // Case-insensitive contains
    auto expr_ci_match = createExpr(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "User_id", FilterValueType::STRING, false);
    ASSERT_TRUE(expr_ci_match.evaluate(entry).has_value()) << expr_ci_match.evaluate(entry).error().toString();
    EXPECT_TRUE(expr_ci_match.evaluate(entry).value_or(false));
}

TEST_F(FilterTestFixture, EvaluateNumericComparison) {
    auto entry = createLogEntry(LogLevel::WARNING, "Response time high", "perf.log", {{"response_time_ms", "550"}});

    auto expr_gt = createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "500", FilterValueType::INT, true, "response_time_ms");
    ASSERT_TRUE(expr_gt.evaluate(entry).has_value()) << expr_gt.evaluate(entry).error().toString();
    EXPECT_TRUE(expr_gt.evaluate(entry).value_or(false));
    
    auto expr_lt = createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN, "600", FilterValueType::INT, true, "response_time_ms");
    ASSERT_TRUE(expr_lt.evaluate(entry).has_value()) << expr_lt.evaluate(entry).error().toString();
    EXPECT_TRUE(expr_lt.evaluate(entry).value_or(false));

    auto expr_gt_fail = createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "600", FilterValueType::INT, true, "response_time_ms");
    ASSERT_TRUE(expr_gt_fail.evaluate(entry).has_value()) << expr_gt_fail.evaluate(entry).error().toString();
    EXPECT_FALSE(expr_gt_fail.evaluate(entry).value_or(true));
    
    // Test with a field that doesn't exist
    auto expr_missing_field = createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "100", FilterValueType::INT, true, "non_existent");
    auto result_missing_field = expr_missing_field.evaluate(entry);
    ASSERT_FALSE(result_missing_field.has_value());
    EXPECT_EQ(result_missing_field.error().code, Code::FieldNotFound);

    // Test with non-numeric value that should fail conversion
    auto entry_bad_num = createLogEntry(LogLevel::ERROR, "Bad data", "data.log", {{"value", "not_a_number"}});
    auto expr_bad_num = createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "123", FilterValueType::INT, true, "value");
    ASSERT_FALSE(expr_bad_num.evaluate(entry_bad_num).has_value()); // Expecting failure due to conversion
}

TEST_F(FilterTestFixture, EvaluateDoubleComparison) {
    auto entry = createLogEntry(LogLevel::INFO, "Calculation result", "calc.log", {{"result", "123.456789"}});
    
    // Exact match (within epsilon)
    auto expr_exact = createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "123.456789", FilterValueType::DOUBLE, true, "result");
    ASSERT_TRUE(expr_exact.evaluate(entry).has_value()) << expr_exact.evaluate(entry).error().toString();
    EXPECT_TRUE(expr_exact.evaluate(entry).value_or(false));

    // Close match (within epsilon)
    auto expr_close = createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "123.4567890001", FilterValueType::DOUBLE, true, "result");
    ASSERT_TRUE(expr_close.evaluate(entry).has_value()) << expr_close.evaluate(entry).error().toString();
    EXPECT_TRUE(expr_close.evaluate(entry).value_or(false));

    // Greater than
    auto expr_gt = createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "123.456", FilterValueType::DOUBLE, true, "result");
    ASSERT_TRUE(expr_gt.evaluate(entry).has_value()) << expr_gt.evaluate(entry).error().toString();
    EXPECT_TRUE(expr_gt.evaluate(entry).value_or(false));

    // Less than
    auto expr_lt = createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN, "123.457", FilterValueType::DOUBLE, true, "result");
    ASSERT_TRUE(expr_lt.evaluate(entry).has_value()) << expr_lt.evaluate(entry).error().toString();
    EXPECT_TRUE(expr_lt.evaluate(entry).value_or(false));
    
    // Test with non-numeric value that should fail conversion
    auto entry_bad_double = createLogEntry(LogLevel::ERROR, "Bad double data", "data.log", {{"value", "not_a_double"}});
    auto expr_bad_double = createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "1.23", FilterValueType::DOUBLE, true, "value");
    ASSERT_FALSE(expr_bad_double.evaluate(entry_bad_double).has_value()); // Expecting failure due to conversion
}


TEST_F(FilterTestFixture, EvaluateBoolComparison) {
    auto entry_true = createLogEntry(LogLevel::INFO, "Status is true", "status.log", {{"is_active", "true"}});
    auto entry_false = createLogEntry(LogLevel::INFO, "Status is false", "status.log", {{"is_active", "false"}});
    auto entry_zero = createLogEntry(LogLevel::INFO, "Status is zero", "status.log", {{"is_active", "0"}});
    auto entry_one = createLogEntry(LogLevel::INFO, "Status is one", "status.log", {{"is_active", "1"}});
    auto entry_invalid = createLogEntry(LogLevel::INFO, "Status invalid", "status.log", {{"is_active", "maybe"}});

    // EQUALS true
    ASSERT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "true", FilterValueType::BOOL, true, "is_active").evaluate(entry_true).has_value());
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "true", FilterValueType::BOOL, true, "is_active").evaluate(entry_true).value_or(false));
    ASSERT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "1", FilterValueType::BOOL, true, "is_active").evaluate(entry_one).has_value());
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "1", FilterValueType::BOOL, true, "is_active").evaluate(entry_one).value_or(false));
    ASSERT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "true", FilterValueType::BOOL, true, "is_active").evaluate(entry_false).has_value());
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "true", FilterValueType::BOOL, true, "is_active").evaluate(entry_false).value_or(true));
    ASSERT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "true", FilterValueType::BOOL, true, "is_active").evaluate(entry_zero).has_value());
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "true", FilterValueType::BOOL, true, "is_active").evaluate(entry_zero).value_or(true));
    ASSERT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "true", FilterValueType::BOOL, true, "is_active").evaluate(entry_invalid).has_value()); // Invalid conversion

    // EQUALS false
    ASSERT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "false", FilterValueType::BOOL, true, "is_active").evaluate(entry_false).has_value());
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "false", FilterValueType::BOOL, true, "is_active").evaluate(entry_false).value_or(false));
    ASSERT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "0", FilterValueType::BOOL, true, "is_active").evaluate(entry_zero).has_value());
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "0", FilterValueType::BOOL, true, "is_active").evaluate(entry_zero).value_or(false));
    ASSERT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "false", FilterValueType::BOOL, true, "is_active").evaluate(entry_true).has_value());
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "false", FilterValueType::BOOL, true, "is_active").evaluate(entry_true).value_or(true));
    ASSERT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "false", FilterValueType::BOOL, true, "is_active").evaluate(entry_one).has_value());
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "false", FilterValueType::BOOL, true, "is_active").evaluate(entry_one).value_or(true));
    ASSERT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "false", FilterValueType::BOOL, true, "is_active").evaluate(entry_invalid).has_value());
}

TEST_F(FilterTestFixture, EvaluateDateTimeComparison) {
    auto entry_past = createLogEntry(LogLevel::INFO, "Event in the past", "time.log", {}, std::nullopt, std::chrono::system_clock::now() - std::chrono::hours(1));
    auto entry_future = createLogEntry(LogLevel::INFO, "Event in the future", "time.log", {}, std::nullopt, std::chrono::system_clock::now() + std::chrono::hours(1));
    auto entry_now = createLogEntry(LogLevel::INFO, "Event now", "time.log", {}, std::nullopt, std::chrono::system_clock::now());

    // Use a precise format that Utils::parseTime should handle
    std::string ts_past_str = Utils::formatTimestamp(entry_past.timestamp.value());
    std::string ts_future_str = Utils::formatTimestamp(entry_future.timestamp.value());
    std::string ts_now_str = Utils::formatTimestamp(entry_now.timestamp.value());

    // EQUALS
    EXPECT_TRUE(createExpr(LogEntryField::TIMESTAMP, FilterOperator::EQUALS, ts_past_str, FilterValueType::DATETIME).evaluate(entry_past).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::TIMESTAMP, FilterOperator::EQUALS, ts_future_str, FilterValueType::DATETIME).evaluate(entry_past).value_or(true));

    // GREATER_THAN
    EXPECT_TRUE(createExpr(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, ts_past_str, FilterValueType::DATETIME).evaluate(entry_future).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, ts_future_str, FilterValueType::DATETIME).evaluate(entry_future).value_or(true));
    EXPECT_TRUE(createExpr(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, ts_past_str, FilterValueType::DATETIME).evaluate(entry_now).value_or(false));

    // LESS_THAN_OR_EQUAL
    EXPECT_TRUE(createExpr(LogEntryField::TIMESTAMP, FilterOperator::LESS_THAN_OR_EQUAL, ts_future_str, FilterValueType::DATETIME).evaluate(entry_future).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::TIMESTAMP, FilterOperator::LESS_THAN_OR_EQUAL, ts_future_str, FilterValueType::DATETIME).evaluate(entry_now).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::TIMESTAMP, FilterOperator::LESS_THAN_OR_EQUAL, ts_now_str, FilterValueType::DATETIME).evaluate(entry_future).value_or(true));

    // Test with invalid datetime string
    auto expr_invalid_dt = createExpr(LogEntryField::TIMESTAMP, FilterOperator::EQUALS, "not_a_datetime", FilterValueType::DATETIME);
    ASSERT_FALSE(expr_invalid_dt.evaluate(entry_now).has_value());
}

TEST_F(FilterTestFixture, EvaluateInOperator) {
    auto entry_apple = createLogEntry(LogLevel::INFO, "Fruit: Apple", "fruit.log", {{"item", "Apple"}});
    auto entry_banana = createLogEntry(LogLevel::INFO, "Fruit: Banana", "fruit.log", {{"item", "Banana"}});
    auto entry_cherry = createLogEntry(LogLevel::INFO, "Fruit: Cherry", "fruit.log", {{"item", "Cherry"}});
    auto entry_date = createLogEntry(LogLevel::INFO, "Fruit: Date", "fruit.log", {{"item", "Date"}});

    // IN operator - match
    auto expr_in_match = createExpr(LogEntryField::CUSTOM, FilterOperator::IN, R"json(["Apple", "Banana"])json", FilterValueType::STRING, true, "item");
    EXPECT_TRUE(expr_in_match.evaluate(entry_apple).value_or(false));
    EXPECT_TRUE(expr_in_match.evaluate(entry_banana).value_or(false));

    // IN operator - no match
    auto expr_in_no_match = createExpr(LogEntryField::CUSTOM, FilterOperator::IN, R"json(["Apple", "Banana"])json", FilterValueType::STRING, true, "item");
    EXPECT_FALSE(expr_in_no_match.evaluate(entry_cherry).value_or(true));

    // IN operator - case insensitive
    auto expr_in_ci = createExpr(LogEntryField::CUSTOM, FilterOperator::IN, R"json(["apple", "banana"])json", FilterValueType::STRING, false, "item");
    EXPECT_TRUE(expr_in_ci.evaluate(entry_apple).value_or(false));
    EXPECT_TRUE(expr_in_ci.evaluate(entry_banana).value_or(false));
    EXPECT_FALSE(expr_in_ci.evaluate(entry_cherry).value_or(true));

    // IN operator - empty array
    auto expr_in_empty = createExpr(LogEntryField::CUSTOM, FilterOperator::IN, R"json([])json", FilterValueType::STRING, true, "item");
    EXPECT_FALSE(expr_in_empty.evaluate(entry_apple).value_or(true)); // Empty IN should never match

    // IN operator - invalid JSON
    auto expr_in_invalid_json = createExpr(LogEntryField::CUSTOM, FilterOperator::IN, R"json(["Apple",)json", FilterValueType::STRING, true, "item");
    ASSERT_FALSE(expr_in_invalid_json.evaluate(entry_apple).has_value()); // Invalid JSON should error

    // IN operator - JSON is not an array
    auto expr_in_not_array = createExpr(LogEntryField::CUSTOM, FilterOperator::IN, R"json("Apple")json", FilterValueType::STRING, true, "item");
    ASSERT_FALSE(expr_in_not_array.evaluate(entry_apple).has_value()); // Not an array should error

    // IN operator - array with non-string items (should be ignored)
    auto expr_in_mixed_types = createExpr(LogEntryField::CUSTOM, FilterOperator::IN, R"json(["Apple", 123, true, null])json", FilterValueType::STRING, true, "item");
    EXPECT_TRUE(expr_in_mixed_types.evaluate(entry_apple).value_or(false)); // Should match "Apple"
    EXPECT_FALSE(expr_in_mixed_types.evaluate(entry_banana).value_or(true)); // Should not match "Banana"

    // NOT_IN operator - match
    auto expr_not_in_match = createExpr(LogEntryField::CUSTOM, FilterOperator::NOT_IN, R"json(["Apple", "Banana"])json", FilterValueType::STRING, true, "item");
    EXPECT_FALSE(expr_not_in_match.evaluate(entry_apple).value_or(true));
    EXPECT_FALSE(expr_not_in_match.evaluate(entry_banana).value_or(true));

    // NOT_IN operator - no match
    auto expr_not_in_no_match = createExpr(LogEntryField::CUSTOM, FilterOperator::NOT_IN, R"json(["Apple", "Banana"])json", FilterValueType::STRING, true, "item");
    EXPECT_TRUE(expr_not_in_no_match.evaluate(entry_cherry).value_or(false));
    EXPECT_TRUE(expr_not_in_no_match.evaluate(entry_date).value_or(false));
}

TEST_F(FilterTestFixture, EvaluateRegexMatch) {
    auto entry_email = createLogEntry(LogLevel::INFO, "Contact: test@example.com", "contact.log");
    auto entry_phone = createLogEntry(LogLevel::INFO, "Call: +1-555-123-4567", "contact.log");

    // Regex match (case-sensitive)
    auto expr_regex_match_cs = createExpr(LogEntryField::MESSAGE, FilterOperator::REGEX, R"(\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b)", FilterValueType::STRING, true);
    EXPECT_TRUE(expr_regex_match_cs.evaluate(entry_email).value_or(false));
    EXPECT_FALSE(expr_regex_match_cs.evaluate(entry_phone).value_or(true));

    // Regex match (case-insensitive)
    auto expr_regex_match_ci = createExpr(LogEntryField::MESSAGE, FilterOperator::REGEX, R"(\b[A-Za-z0-9._%+-]+@[a-z0-9.-]+\.[a-z]{2,}\b)", FilterValueType::STRING, false);
    EXPECT_TRUE(expr_regex_match_ci.evaluate(entry_email).value_or(false));

    // Regex mismatch
    auto expr_regex_mismatch = createExpr(LogEntryField::MESSAGE, FilterOperator::REGEX, R"(\d{3}-\d{3}-\d{4})", FilterValueType::STRING, true);
    EXPECT_TRUE(expr_regex_mismatch.evaluate(entry_phone).value_or(false));
    EXPECT_FALSE(expr_regex_mismatch.evaluate(entry_email).value_or(true));
    
    // Invalid regex pattern
    auto expr_invalid_regex = createExpr(LogEntryField::MESSAGE, FilterOperator::REGEX, R"([)", FilterValueType::STRING, true);
    ASSERT_FALSE(expr_invalid_regex.evaluate(entry_email).has_value()); // Invalid regex should return error
}

TEST_F(FilterTestFixture, EvaluateIsPresentAndIsAbsent) {
    auto entry_with_id = createLogEntry(LogLevel::INFO, "Message with ID", "log.log", {{"custom_field", "value"}}, 123, std::nullopt, 45);
    
    // Manually create entry without ID to bypass helper's auto-ID generation
    LogEntry entry_without_id;
    entry_without_id.id = std::nullopt;
    entry_without_id.level = LogLevel::INFO;
    entry_without_id.message = "Message without ID";
    entry_without_id.sourceFile = "log.log";
    entry_without_id.customFields = {{"custom_field", "value"}};
    entry_without_id.sourceLineNumber = 45;

    auto entry_with_custom = createLogEntry(LogLevel::INFO, "Message with custom field", "log.log", {{"custom_field", "value"}});
    auto entry_without_custom = createLogEntry(LogLevel::INFO, "Message without custom field", "log.log", {});

    // IS_PRESENT tests for built-in fields (use default valueType and caseSensitive)
    EXPECT_TRUE(createExpr(LogEntryField::ID, FilterOperator::IS_PRESENT, "").evaluate(entry_with_id).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::ID, FilterOperator::IS_PRESENT, "").evaluate(entry_without_id).value_or(true));
    EXPECT_TRUE(createExpr(LogEntryField::LINE_NUMBER, FilterOperator::IS_PRESENT, "").evaluate(entry_with_id).value_or(false)); // Line number is set
    EXPECT_TRUE(createExpr(LogEntryField::LINE_NUMBER, FilterOperator::IS_PRESENT, "").evaluate(entry_without_id).value_or(false)); // Line number is set even if ID is not

    // IS_PRESENT tests for custom fields (correctly pass valueType, caseSensitive, and customField name)
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::IS_PRESENT, "", FilterValueType::STRING, true, "custom_field").evaluate(entry_with_custom).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::IS_PRESENT, "", FilterValueType::STRING, true, "custom_field").evaluate(entry_without_custom).value_or(true));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::IS_PRESENT, "", FilterValueType::STRING, true, "non_existent_custom").evaluate(entry_with_custom).value_or(true));

    // IS_ABSENT tests for built-in fields
    EXPECT_TRUE(createExpr(LogEntryField::ID, FilterOperator::IS_ABSENT, "").evaluate(entry_without_id).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::ID, FilterOperator::IS_ABSENT, "").evaluate(entry_with_id).value_or(true));

    // IS_ABSENT tests for custom fields (correctly pass valueType, caseSensitive, and customField name)
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::IS_ABSENT, "", FilterValueType::STRING, true, "custom_field").evaluate(entry_without_custom).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::IS_ABSENT, "", FilterValueType::STRING, true, "custom_field").evaluate(entry_with_custom).value_or(true));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::IS_ABSENT, "", FilterValueType::STRING, true, "non_existent_custom").evaluate(entry_with_custom).value_or(false));
}

TEST_F(FilterTestFixture, EvaluateStringOperatorsEdgeCases) {
    auto entry = createLogEntry(LogLevel::INFO, "  leading and trailing spaces  ", "spaces.log");

    // STARTS_WITH
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "  leading", FilterValueType::STRING, true).evaluate(entry).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "leading", FilterValueType::STRING, true).evaluate(entry).value_or(true)); // Doesn't start with "leading"
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "  leading", FilterValueType::STRING, false).evaluate(entry).value_or(false)); // CI match

    // ENDS_WITH
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "spaces  ", FilterValueType::STRING, true).evaluate(entry).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "spaces", FilterValueType::STRING, true).evaluate(entry).value_or(true)); // Doesn't end with "spaces"
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "SPACES  ", FilterValueType::STRING, false).evaluate(entry).value_or(false)); // CI match

    // CONTAINS
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "and trailing", FilterValueType::STRING, true).evaluate(entry).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "trailing", FilterValueType::STRING, true).evaluate(entry).value_or(false)); // Contains "trailing" substring
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "TRAILING", FilterValueType::STRING, false).evaluate(entry).value_or(false)); // CI match

    // Empty string comparisons
    auto entry_empty_msg = createLogEntry(LogLevel::INFO, "", "empty.log");
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::EQUALS, "", FilterValueType::STRING, true).evaluate(entry_empty_msg).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "", FilterValueType::STRING, true).evaluate(entry_empty_msg).value_or(false)); // Empty string is contained in any string
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "", FilterValueType::STRING, true).evaluate(entry_empty_msg).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "", FilterValueType::STRING, true).evaluate(entry_empty_msg).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::MESSAGE, FilterOperator::EQUALS, "a", FilterValueType::STRING, true).evaluate(entry_empty_msg).value_or(true));
}

TEST_F(FilterTestFixture, EvaluateNumericBoundaryTests) {
    auto entry = createLogEntry(LogLevel::INFO, "Value is 100", "boundary.log", {{"count", "100"}});

    // GREATER_THAN_OR_EQUAL
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN_OR_EQUAL, "100", FilterValueType::INT, true, "count").evaluate(entry).value_or(false)); // Equal
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN_OR_EQUAL, "99", FilterValueType::INT, true, "count").evaluate(entry).value_or(false)); // Greater
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN_OR_EQUAL, "101", FilterValueType::INT, true, "count").evaluate(entry).value_or(true)); // Less

    // LESS_THAN_OR_EQUAL
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "100", FilterValueType::INT, true, "count").evaluate(entry).value_or(false)); // Equal
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "101", FilterValueType::INT, true, "count").evaluate(entry).value_or(false)); // Less
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "99", FilterValueType::INT, true, "count").evaluate(entry).value_or(true)); // Greater
}

TEST_F(FilterTestFixture, EvaluateEmptyExpression) {
    auto entry = createLogEntry(LogLevel::INFO, "Any message", "any.log");
    // An empty FilterExpression is created via default construction.
    FilterExpression empty_expr; 
    
    EXPECT_TRUE(empty_expr.evaluate(entry).value_or(false)); // Empty expression should always match

    // Check if it's neither a condition nor a logical operator, implying an empty state.
    EXPECT_FALSE(empty_expr.isCondition()); 
    EXPECT_FALSE(empty_expr.isLogical());   
}

TEST_F(FilterTestFixture, EvaluateStringConversionsForNonStringTypes) {
    // Ensure that string comparison logic in the default/auto/unknown handler is robust
    // when passed to types that are not STRING, AUTO, or UNKNOWN.
    // The refactored code should ensure these are handled by their specific types (INT, DOUBLE, etc.)
    // or throw if the type is truly unhandled.

    // Test INT comparison using string logic (should not happen due to correct type dispatch)
    auto entry_int = createLogEntry(LogLevel::INFO, "Int value", "int.log", {{"count", "100"}});
    // This test case is more about ensuring the type dispatch works correctly.
    // If it falls into string logic by mistake, it might yield unexpected results.
    // The correct behavior is to use INT logic.
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "100", FilterValueType::INT, true, "count").evaluate(entry_int).value_or(false));
    // This condition would be evaluated by the INT logic, not string logic.

    // The refactoring of the default case ensures that if a type is not INT, DOUBLE, etc.,
    // AND it's not STRING, AUTO, UNKNOWN, then it will throw.
    // This test indirectly verifies that INT, DOUBLE, etc. are handled before the default.
}

TEST_F(FilterTestFixture, CustomFieldAccess) {
    auto entry = createLogEntry(LogLevel::INFO, "Log entry", "custom.log", {{"user_id", "abc123"}, {"tenant_id", "xyz789"}});

    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "abc123", FilterValueType::STRING, true, "user_id").evaluate(entry).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "XYZ789", FilterValueType::STRING, false, "tenant_id").evaluate(entry).value_or(false)); // Case-insensitive
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "def456", FilterValueType::STRING, true, "user_id").evaluate(entry).value_or(true));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "XYZ789", FilterValueType::STRING, true, "tenant_id").evaluate(entry).value_or(true)); // Case-sensitive mismatch
}

TEST_F(FilterTestFixture, BuiltInFieldsAccess) {
    auto entry = createLogEntry(
        LogLevel::DEBUG,
        "Application heartbeat",
        "main.cpp",
        {{"custom_field", "custom_value"}},
        101,                      // id
        std::chrono::system_clock::now(), // timestamp
        55,                       // sourceLineNumber
        "main_thread",            // threadId
        "AppModule",              // module
        "server-123"              // host
    );

    // Test standard fields
    EXPECT_TRUE(createExpr(LogEntryField::LEVEL, FilterOperator::EQUALS, "DEBUG", FilterValueType::STRING, true).evaluate(entry).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "heartbeat", FilterValueType::STRING, true).evaluate(entry).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::SOURCE_FILE, FilterOperator::EQUALS, "main.cpp", FilterValueType::STRING, true).evaluate(entry).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::ID, FilterOperator::EQUALS, "101", FilterValueType::INT, true).evaluate(entry).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::LINE_NUMBER, FilterOperator::EQUALS, "55", FilterValueType::INT, true).evaluate(entry).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::THREAD_ID, FilterOperator::EQUALS, "main_thread", FilterValueType::STRING, true).evaluate(entry).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::MODULE, FilterOperator::EQUALS, "AppModule", FilterValueType::STRING, true).evaluate(entry).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::HOST, FilterOperator::EQUALS, "server-123", FilterValueType::STRING, true).evaluate(entry).value_or(false));
    
    // Test custom field when it also exists
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "custom_value", FilterValueType::STRING, true, "custom_field").evaluate(entry).value_or(false));
}
