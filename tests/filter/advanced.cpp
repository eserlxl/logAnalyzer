// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "utils.h"

using namespace filter;

// --- FilterOperator::IN and FilterOperator::NOT_IN Edge Cases ---
TEST_F(FilterTestFixture, EvaluateInOperatorInvalidJson) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Message");
    FilterCondition cond = createCondition(LogEntryField::MESSAGE, FilterOperator::IN, "not-json", FilterValueType::STRING);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry).has_value());
}

TEST_F(FilterTestFixture, EvaluateInOperatorNonArrayJson) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Message");
    FilterCondition cond = createCondition(LogEntryField::MESSAGE, FilterOperator::IN, "\"single_string\"", FilterValueType::STRING);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry).has_value());
    
    cond = createCondition(LogEntryField::MESSAGE, FilterOperator::IN, "{\"key\": \"value\"}", FilterValueType::STRING);
    expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry).has_value());
}

TEST_F(FilterTestFixture, EvaluateInOperatorNonStringItems) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "test");
    // "test" is not in ["abc", 123, true]
    FilterCondition cond = createCondition(LogEntryField::MESSAGE, FilterOperator::IN, "[\"abc\", 123, true]", FilterValueType::STRING);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry).value_or(true));

    // "abc" is in ["abc", 123, true]
    LogEntry entry2 = createLogEntry(LogLevel::INFO, "abc");
    EXPECT_TRUE(expr.evaluate(entry2).value_or(false));

    // "123" (as string) is now in ["abc", 123, true] because integers are correctly converted to strings
    LogEntry entry3 = createLogEntry(LogLevel::INFO, "123");
    EXPECT_TRUE(expr.evaluate(entry3).value_or(false));

    // "true" (as string) is also in the set
    LogEntry entry4 = createLogEntry(LogLevel::INFO, "true");
    EXPECT_TRUE(expr.evaluate(entry4).value_or(false));
}

TEST_F(FilterTestFixture, EvaluateInOperatorEmptyArray) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "test");
    FilterCondition cond = createCondition(LogEntryField::MESSAGE, FilterOperator::IN, "[]", FilterValueType::STRING);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry).value_or(true));
}

TEST_F(FilterTestFixture, EvaluateNotInOperatorEmptyArray) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "test");
    FilterCondition cond = createCondition(LogEntryField::MESSAGE, FilterOperator::NOT_IN, "[]", FilterValueType::STRING);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry).value_or(false)); // Not in an empty set is always true
}

// --- Error Handling for Numeric and Regex Conversions ---
TEST_F(FilterTestFixture, EvaluateNumericInvalidInput) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Test", "test.log", {{"value", "abc"}});
    FilterCondition cond = createCondition(LogEntryField::CUSTOM, FilterOperator::EQUALS, "123", FilterValueType::INT, true, "value");
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry).has_value()); // "abc" cannot be converted to INT

    cond = createCondition(LogEntryField::CUSTOM, FilterOperator::EQUALS, "not-a-num", FilterValueType::INT, true, "value");
    expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry).has_value()); // "not-a-num" cannot be converted to INT
}

TEST_F(FilterTestFixture, EvaluateDoubleInvalidInput) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Test", "test.log", {{"value", "abc"}});
    FilterCondition cond = createCondition(LogEntryField::CUSTOM, FilterOperator::EQUALS, "3.14", FilterValueType::DOUBLE, true, "value");
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry).has_value()); // "abc" cannot be converted to DOUBLE
}

TEST_F(FilterTestFixture, EvaluateRegexInvalidPattern) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Some message");
    FilterCondition cond = createCondition(LogEntryField::MESSAGE, FilterOperator::REGEX, "[invalid regex", FilterValueType::STRING);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry).has_value()); // Invalid regex should return error
}

// --- LogEntryField::ID and LINE_NUMBER with Optional Values ---
TEST_F(FilterTestFixture, EvaluateIdOptionalPresent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg", "test.log", {}, 100);
    FilterCondition cond = createCondition(LogEntryField::ID, FilterOperator::EQUALS, "100", FilterValueType::INT);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry).value_or(false));
}

TEST_F(FilterTestFixture, EvaluateIdOptionalAbsent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg", "test.log", {}, std::nullopt);
    entry.id = std::nullopt; // Explicitly reset because createLogEntry assigns a default ID
    FilterCondition cond = createCondition(LogEntryField::ID, FilterOperator::IS_ABSENT, "", FilterValueType::STRING);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry).value_or(false));
    
    cond = createCondition(LogEntryField::ID, FilterOperator::IS_PRESENT, "", FilterValueType::STRING);
    expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry).value_or(true));
}

TEST_F(FilterTestFixture, EvaluateLineNumberOptionalPresent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg", "test.log", {}, std::nullopt, std::nullopt, 50);
    FilterCondition cond = createCondition(LogEntryField::LINE_NUMBER, FilterOperator::EQUALS, "50", FilterValueType::INT);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry).value_or(false));
}

TEST_F(FilterTestFixture, EvaluateLineNumberOptionalAbsent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg", "test.log", {}, std::nullopt, std::nullopt, std::nullopt);
    FilterCondition cond = createCondition(LogEntryField::LINE_NUMBER, FilterOperator::IS_ABSENT, "", FilterValueType::STRING);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry).value_or(false));
    
    cond = createCondition(LogEntryField::LINE_NUMBER, FilterOperator::IS_PRESENT, "", FilterValueType::STRING);
    expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry).value_or(true));
}

// --- LogEntryField::THREAD_ID, MODULE, HOST (now direct members) ---
TEST_F(FilterTestFixture, EvaluateThreadIdOptionalPresent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg", "test.log", {}, std::nullopt, std::nullopt, std::nullopt, "thread-123");
    FilterCondition cond = createCondition(LogEntryField::THREAD_ID, FilterOperator::EQUALS, "thread-123");
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry).value_or(false));
}

TEST_F(FilterTestFixture, EvaluateThreadIdOptionalAbsent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg");
    FilterCondition cond = createCondition(LogEntryField::THREAD_ID, FilterOperator::IS_ABSENT, "");
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry).value_or(false));
}

TEST_F(FilterTestFixture, EvaluateModuleOptionalPresent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg", "test.log", {}, std::nullopt, std::nullopt, std::nullopt, std::nullopt, "Analytics");
    FilterCondition cond = createCondition(LogEntryField::MODULE, FilterOperator::EQUALS, "Analytics");
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry).value_or(false));
}

TEST_F(FilterTestFixture, EvaluateModuleOptionalAbsent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg");
    FilterCondition cond = createCondition(LogEntryField::MODULE, FilterOperator::IS_ABSENT, "");
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry).value_or(false));
}

TEST_F(FilterTestFixture, EvaluateHostOptionalPresent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg", "test.log", {}, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, "server-prod-01");
    FilterCondition cond = createCondition(LogEntryField::HOST, FilterOperator::EQUALS, "server-prod-01");
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry).value_or(false));
}

TEST_F(FilterTestFixture, EvaluateHostOptionalAbsent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg");
    FilterCondition cond = createCondition(LogEntryField::HOST, FilterOperator::IS_ABSENT, "");
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry).value_or(false));
}

// --- FilterValueType::DATETIME Edge Cases ---
TEST_F(FilterTestFixture, EvaluateDateTimeInvalidInput) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg", "test.log", {}, std::nullopt, Utils::parseTime("2023-01-01 10:00:00").value());
    FilterCondition cond = createCondition(LogEntryField::TIMESTAMP, FilterOperator::EQUALS, "not-a-date", FilterValueType::DATETIME);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry).has_value());

    cond = createCondition(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-invalid-date", FilterValueType::DATETIME);
    expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry).has_value());
}

TEST_F(FilterTestFixture, EvaluateDateTimeDifferentFormats) {
    // This test ensures that different valid time formats can be compared if they represent the same time.
    // We'll use a specific time point and format it two different ways that parseTime should handle.
    auto t1_res = Utils::parseTime("2023-01-01 10:00:00"); // Local time
    ASSERT_TRUE(t1_res.has_value());
    auto t1 = t1_res.value();
    
    LogEntry entry1 = createLogEntry(LogLevel::INFO, "Msg", "test.log", {}, std::nullopt, t1);
    
    // Condition using ISO8601 format for the same local time (assuming system timezone offset is correctly handled)
    std::string iso_str = Utils::formatTimestamp(t1, "%Y-%m-%dT%H:%M:%S"); 
    FilterCondition cond = createCondition(LogEntryField::TIMESTAMP, FilterOperator::EQUALS, iso_str, FilterValueType::DATETIME);
    FilterExpression expr = FilterExpression::create(cond);
    
    // This will pass if both formats parse to the same time_point
    EXPECT_TRUE(expr.evaluate(entry1).value_or(false)); 
}

// --- stringToBool Helper Function ---
TEST_F(FilterTestFixture, StringToBoolValidCases) {
    LogEntry entry_true = createLogEntry(LogLevel::INFO, "true", "test.log", {{"bool_val", "true"}});
    LogEntry entry_TRUE = createLogEntry(LogLevel::INFO, "TRUE", "test.log", {{"bool_val", "TRUE"}});
    LogEntry entry_1 = createLogEntry(LogLevel::INFO, "1", "test.log", {{"bool_val", "1"}});
    LogEntry entry_false = createLogEntry(LogLevel::INFO, "false", "test.log", {{"bool_val", "false"}});
    LogEntry entry_FALSE = createLogEntry(LogLevel::INFO, "FALSE", "test.log", {{"bool_val", "FALSE"}});
    LogEntry entry_0 = createLogEntry(LogLevel::INFO, "0", "test.log", {{"bool_val", "0"}});

    FilterCondition cond_true = createCondition(LogEntryField::CUSTOM, FilterOperator::EQUALS, "true", FilterValueType::BOOL, true, "bool_val");
    FilterExpression expr_true = FilterExpression::create(cond_true);
    EXPECT_TRUE(expr_true.evaluate(entry_true).value_or(false));
    EXPECT_TRUE(expr_true.evaluate(entry_TRUE).value_or(false));
    EXPECT_TRUE(expr_true.evaluate(entry_1).value_or(false));
    EXPECT_FALSE(expr_true.evaluate(entry_false).value_or(true));

    FilterCondition cond_false = createCondition(LogEntryField::CUSTOM, FilterOperator::EQUALS, "false", FilterValueType::BOOL, true, "bool_val");
    FilterExpression expr_false = FilterExpression::create(cond_false);
    EXPECT_TRUE(expr_false.evaluate(entry_false).value_or(false));
    EXPECT_TRUE(expr_false.evaluate(entry_FALSE).value_or(false));
    EXPECT_TRUE(expr_false.evaluate(entry_0).value_or(false));
    EXPECT_FALSE(expr_false.evaluate(entry_true).value_or(true));
}

TEST_F(FilterTestFixture, StringToBoolInvalidCases) {
    LogEntry entry_invalid = createLogEntry(LogLevel::INFO, "maybe", "test.log", {{"bool_val", "maybe"}});
    FilterCondition cond_true = createCondition(LogEntryField::CUSTOM, FilterOperator::EQUALS, "true", FilterValueType::BOOL, true, "bool_val");
    FilterExpression expr_true = FilterExpression::create(cond_true);
    EXPECT_FALSE(expr_true.evaluate(entry_invalid).has_value()); // Invalid bool string should return error
}

// --- Fluent API - Complex Combinations and Negation ---
TEST_F(FilterTestFixture, EvaluateComplexLogicalCombination) {
    // (LEVEL == INFO AND MESSAGE CONTAINS "user") OR (HOST == "backend" AND NOT (MODULE == "auth"))
    LogEntry entry1 = createLogEntry(LogLevel::INFO, "User 'john' logged in", "test.log", {}, std::nullopt, std::nullopt, std::nullopt, std::nullopt, "web", "frontend"); // Matches only 1st part
    LogEntry entry2 = createLogEntry(LogLevel::DEBUG, "Request to backend", "test.log", {}, std::nullopt, std::nullopt, std::nullopt, std::nullopt, "api", "backend"); // Matches 2nd part (HOST=backend AND NOT MODULE=auth)
    LogEntry entry3 = createLogEntry(LogLevel::ERROR, "Auth failed", "test.log", {}, std::nullopt, std::nullopt, std::nullopt, std::nullopt, "auth", "backend"); // Doesn't match 2nd part (HOST=backend but MODULE=auth)
    LogEntry entry4 = createLogEntry(LogLevel::WARNING, "Disk full", "test.log", {}, std::nullopt, std::nullopt, std::nullopt, std::nullopt, "monitor", "local"); // No match

    auto cond_info = createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO");
    auto cond_user_msg = createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "user", FilterValueType::STRING, false); // Make case-insensitive
    auto cond_host_backend = createCondition(LogEntryField::HOST, FilterOperator::EQUALS, "backend");
    auto cond_module_auth = createCondition(LogEntryField::MODULE, FilterOperator::EQUALS, "auth");

    FilterExpression expr = (FilterExpression::create(cond_info).And(FilterExpression::create(cond_user_msg)))
                            .Or(FilterExpression::create(cond_host_backend).And(FilterExpression::create(cond_module_auth).Not()));

    EXPECT_TRUE(expr.evaluate(entry1).value_or(false));
    EXPECT_TRUE(expr.evaluate(entry2).value_or(false));
    EXPECT_FALSE(expr.evaluate(entry3).value_or(true));
    EXPECT_FALSE(expr.evaluate(entry4).value_or(true));
}

TEST_F(FilterTestFixture, EvaluateNegationOfLogicalExpression) {
    // NOT(LEVEL == INFO OR LEVEL == DEBUG)
    LogEntry entry_info = createLogEntry(LogLevel::INFO, "Log");
    LogEntry entry_debug = createLogEntry(LogLevel::DEBUG, "Log");
    LogEntry entry_warn = createLogEntry(LogLevel::WARNING, "Log");

    auto cond_info = createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO");
    auto cond_debug = createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "DEBUG");

    FilterExpression expr = (FilterExpression::create(cond_info).Or(FilterExpression::create(cond_debug))).Not();

    EXPECT_FALSE(expr.evaluate(entry_info).value_or(true));
    EXPECT_FALSE(expr.evaluate(entry_debug).value_or(true));
    EXPECT_TRUE(expr.evaluate(entry_warn).value_or(false));
}

// --- FilterExpression::EMPTY Type ---
TEST_F(FilterTestFixture, EvaluateEmptyExpression_Advanced) { // Renamed to avoid duplicate test name
    FilterExpression empty_expr; // Default constructor creates an EMPTY expression
    LogEntry any_entry = createLogEntry(LogLevel::INFO, "Any message");
    EXPECT_TRUE(empty_expr.evaluate(any_entry));
}

// --- Comprehensive String Operator Tests ---
TEST_F(FilterTestFixture, EvaluateStringStartsEndsWithEdgeCases) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "HelloWorld");

    // STARTS_WITH
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "Hello")).evaluate(entry).value_or(false));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "hello", FilterValueType::STRING, true)).evaluate(entry).value_or(true));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "hello", FilterValueType::STRING, false)).evaluate(entry).value_or(false));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH_I, "hello")).evaluate(entry).value_or(false));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "HelloWorld")).evaluate(entry).value_or(false));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "HellWorld")).evaluate(entry).value_or(true)); // Longer prefix
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "")).evaluate(entry).value_or(false)); // Empty prefix
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH_I, "")).evaluate(entry).value_or(false)); // Empty prefix
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "LongerThanMessage")).evaluate(entry).value_or(true));

    // ENDS_WITH
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "World")).evaluate(entry).value_or(false));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "world", FilterValueType::STRING, true)).evaluate(entry).value_or(true));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "world", FilterValueType::STRING, false)).evaluate(entry).value_or(false));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH_I, "world")).evaluate(entry).value_or(false));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "HelloWorld")).evaluate(entry).value_or(false));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "HelloWord")).evaluate(entry).value_or(true)); // Longer suffix
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "")).evaluate(entry).value_or(false)); // Empty suffix
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH_I, "")).evaluate(entry).value_or(false)); // Empty suffix
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "LongerThanMessage")).evaluate(entry).value_or(true));
}

TEST_F(FilterTestFixture, EvaluateStringContainsNotContainsEdgeCases) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Hello World Hello");

    // CONTAINS
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "World")).evaluate(entry).value_or(false));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "world", FilterValueType::STRING, true)).evaluate(entry).value_or(true));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "world", FilterValueType::STRING, false)).evaluate(entry).value_or(false));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS_I, "world")).evaluate(entry).value_or(false));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "Hello")).evaluate(entry).value_or(false));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "")).evaluate(entry).value_or(false)); // Empty pattern matches
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS_I, "")).evaluate(entry).value_or(false)); // Empty pattern matches

    // NOT_CONTAINS
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::NOT_CONTAINS, "World")).evaluate(entry).value_or(true));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::NOT_CONTAINS, "world", FilterValueType::STRING, true)).evaluate(entry).value_or(false));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::NOT_CONTAINS, "world", FilterValueType::STRING, false)).evaluate(entry).value_or(true));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::NOT_CONTAINS_I, "world")).evaluate(entry).value_or(true));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::NOT_CONTAINS, "")).evaluate(entry).value_or(true)); // Empty pattern does not match (is contained)
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::NOT_CONTAINS_I, "")).evaluate(entry).value_or(true)); // Empty pattern does not match (is contained)
}

// --- Numeric Operator Boundary Tests ---
TEST_F(FilterTestFixture, EvaluateNumericEqualityBoundary) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg", "test.log", {{"num_val", "10"}});

    FilterCondition cond_eq = createCondition(LogEntryField::CUSTOM, FilterOperator::EQUALS, "10", FilterValueType::INT, true, "num_val");
    EXPECT_TRUE(FilterExpression::create(cond_eq).evaluate(entry).value_or(false));

    FilterCondition cond_ge = createCondition(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN_OR_EQUAL, "10", FilterValueType::INT, true, "num_val");
    EXPECT_TRUE(FilterExpression::create(cond_ge).evaluate(entry).value_or(false));

    FilterCondition cond_le = createCondition(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "10", FilterValueType::INT, true, "num_val");
    EXPECT_TRUE(FilterExpression::create(cond_le).evaluate(entry).value_or(false));

    FilterCondition cond_gt = createCondition(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "10", FilterValueType::INT, true, "num_val");
    EXPECT_FALSE(FilterExpression::create(cond_gt).evaluate(entry).value_or(true));

    FilterCondition cond_lt = createCondition(LogEntryField::CUSTOM, FilterOperator::LESS_THAN, "10", FilterValueType::INT, true, "num_val");
    EXPECT_FALSE(FilterExpression::create(cond_lt).evaluate(entry).value_or(true));
}

// --- IS_PRESENT and IS_ABSENT ---
TEST_F(FilterTestFixture, EvaluateIsPresentAbsentAllFieldTypes) {
    // LogEntry with some fields present, some absent
    LogEntry entry_partial = createLogEntry(
        LogLevel::INFO, "Partial log", 
        "test.log",
        {{"custom_field_present", "value"}, {"custom_field_empty", ""}}, // Custom fields
        123,                // ID present
        Utils::parseTime("2023-01-01 12:00:00").has_value() ? std::make_optional(Utils::parseTime("2023-01-01 12:00:00").value()) : std::nullopt,
        std::nullopt,       // Line Number absent
        "thread_A",         // Thread ID present
        std::nullopt,       // Module absent
        "host_X"            // Host present
    );

    // ID
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::ID, FilterOperator::IS_PRESENT, "")).evaluate(entry_partial).value_or(false));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::ID, FilterOperator::IS_ABSENT, "")).evaluate(entry_partial).value_or(true));

    // LINE_NUMBER
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::LINE_NUMBER, FilterOperator::IS_PRESENT, "")).evaluate(entry_partial).value_or(true));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::LINE_NUMBER, FilterOperator::IS_ABSENT, "")).evaluate(entry_partial).value_or(false));

    // THREAD_ID
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::THREAD_ID, FilterOperator::IS_PRESENT, "")).evaluate(entry_partial).value_or(false));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::THREAD_ID, FilterOperator::IS_ABSENT, "")).evaluate(entry_partial).value_or(true));

    // MODULE
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MODULE, FilterOperator::IS_PRESENT, "")).evaluate(entry_partial).value_or(true));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MODULE, FilterOperator::IS_ABSENT, "")).evaluate(entry_partial).value_or(false));

    // HOST
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::HOST, FilterOperator::IS_PRESENT, "")).evaluate(entry_partial).value_or(false));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::HOST, FilterOperator::IS_ABSENT, "")).evaluate(entry_partial).value_or(true));

    // TIMESTAMP
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::TIMESTAMP, FilterOperator::IS_PRESENT, "")).evaluate(entry_partial).value_or(false));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::TIMESTAMP, FilterOperator::IS_ABSENT, "")).evaluate(entry_partial).value_or(true));
    
    // LEVEL (always present)
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::LEVEL, FilterOperator::IS_PRESENT, "")).evaluate(entry_partial).value_or(false));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::LEVEL, FilterOperator::IS_ABSENT, "")).evaluate(entry_partial).value_or(true));

    // MESSAGE (always present)
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::IS_PRESENT, "")).evaluate(entry_partial).value_or(false));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::IS_ABSENT, "")).evaluate(entry_partial).value_or(true));

    // CUSTOM field (present with value)
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::CUSTOM, FilterOperator::IS_PRESENT, "", FilterValueType::STRING, true, "custom_field_present")).evaluate(entry_partial).value_or(false));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::CUSTOM, FilterOperator::IS_ABSENT, "", FilterValueType::STRING, true, "custom_field_present")).evaluate(entry_partial).value_or(true));

    // CUSTOM field (present with empty string value, but still considered 'present')
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::CUSTOM, FilterOperator::IS_PRESENT, "", FilterValueType::STRING, true, "custom_field_empty")).evaluate(entry_partial).value_or(false));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::CUSTOM, FilterOperator::IS_ABSENT, "", FilterValueType::STRING, true, "custom_field_empty")).evaluate(entry_partial).value_or(true));

    // CUSTOM field (not present at all)
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::CUSTOM, FilterOperator::IS_PRESENT, "", FilterValueType::STRING, true, "custom_field_absent")).evaluate(entry_partial).value_or(true));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::CUSTOM, FilterOperator::IS_ABSENT, "", FilterValueType::STRING, true, "custom_field_absent")).evaluate(entry_partial).value_or(false));
}
