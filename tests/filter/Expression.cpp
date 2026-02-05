#include <gtest/gtest.h>
#include "filter/Core.h"
#include "core/LogTypes.h"
#include "filter/Expression.h" // For FilterValueType, FilterOperator, FilterCondition, FilterExpression
#include "utils/Version.h" // For Utils::parseSemanticVersion
#include "utils/IpAddress.h" // For Utils::parseIpAddress
#include <chrono>
#include <map>
#include <optional>
#include <regex> // For regex tests
#include <nlohmann/json.hpp> // For IN/NOT_IN tests

// Consolidated test fixture for FilterExpression tests
class FilterExpressionTest : public ::testing::Test {
protected:
    // Helper to create a LogEntry with common fields
    LogEntry createLogEntry(
        LogLevel level,
        const std::string& message,
        const std::string& sourceFile = "test.log",
        const std::map<std::string, std::string>& customFields = {},
        std::optional<size_t> id = std::nullopt,
        std::optional<std::chrono::system_clock::time_point> timestamp = std::nullopt,
        std::optional<unsigned int> sourceLineNumber = std::nullopt,
        std::optional<std::string> threadId = std::nullopt,
        std::optional<std::string> module = std::nullopt,
        std::optional<std::string> host = std::nullopt
    ) {
        LogEntry entry;
        entry.id = id.value_or(nextId++); // Use provided ID or generate new one
        entry.sourceFile = sourceFile;
        entry.timestamp = timestamp.value_or(std::chrono::system_clock::now());
        entry.level = level;
        entry.message = message;
        entry.customFields = customFields;
        entry.sourceLineNumber = sourceLineNumber;
        entry.threadId = threadId;
        entry.module = module;
        entry.host = host;
        return entry;
    }

    // Helper to create a FilterCondition
    FilterCondition createCondition(
        LogEntryField field,
        FilterOperator op,
        const std::string& value,
        FilterValueType valueType = FilterValueType::STRING,
        bool caseSensitive = true,
        std::optional<std::string> customField = std::nullopt
    ) {
        FilterCondition fc;
        fc.field = field;
        fc.op = op;
        fc.value = value;
        fc.valueType = valueType;
        fc.caseSensitive = caseSensitive;
        fc.customField = customField;
        return fc;
    }

    // Helper to create a FilterExpression from a Condition
    FilterExpression createExpr(
        LogEntryField field,
        FilterOperator op,
        const std::string& value,
        FilterValueType valueType = FilterValueType::STRING,
        bool caseSensitive = true,
        std::optional<std::string> customField = std::nullopt
    ) {
        return FilterExpression::create(createCondition(field, op, value, valueType, caseSensitive, customField));
    }

private:
    size_t nextId = 1; // Simple counter for unique IDs
};

// --- Legacy Filter Tests (maintained for backward compatibility) ---
// (Keeping these as they were, not modifying them yet)
class LegacyFilterTest : public ::testing::Test {
protected:
    LogEntry createLogEntry(
        size_t id,
        const std::string& sourceFile,
        std::chrono::system_clock::time_point timestamp,
        LogLevel level,
        const std::string& message,
        const std::map<std::string, std::string>& customFields = {}
    ) {
        LogEntry entry;
        entry.id = id;
        entry.sourceFile = sourceFile;
        entry.timestamp = timestamp;
        entry.level = level;
        entry.message = message;
        entry.customFields = customFields;
        return entry;
    }

    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
};


TEST_F(LegacyFilterTest, CompositeFilterEmptyListAND) {
    CompositeFilter filter(CompositeFilter::Logic::AND);
    auto entry = createLogEntry(1, "web.log", now, LogLevel::INFO, "Any message");
    EXPECT_TRUE(filter.matches(entry)); // Empty AND should always return true
}

TEST_F(LegacyFilterTest, CompositeFilterEmptyListOR) {
    CompositeFilter filter(CompositeFilter::Logic::OR);
    auto entry = createLogEntry(1, "web.log", now, LogLevel::INFO, "Any message");
    EXPECT_FALSE(filter.matches(entry)); // Empty OR should always return false
}

TEST_F(LegacyFilterTest, ExclusionFilter) {
    auto base_filter = std::make_shared<FieldExistsFilter>("error_code");
    ExclusionFilter filter(base_filter);

    auto entry1 = createLogEntry(1, "app.log", now, LogLevel::ERROR, "An error occurred", {{"error_code", "E101"}});
    auto entry2 = createLogEntry(2, "app.log", now, LogLevel::INFO, "Operation successful", {{"status", "OK"}});

    EXPECT_FALSE(filter.matches(entry1)); // error_code exists, so exclusion filter should return false
    EXPECT_TRUE(filter.matches(entry2));  // error_code does not exist, so exclusion filter should return true
}


// --- FilterExpression Fluent API & Optimization Tests ---

TEST_F(FilterExpressionTest, FluentApiAndOptimization) {
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

TEST_F(FilterExpressionTest, FluentApiOrOptimization) {
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

TEST_F(FilterExpressionTest, FluentApiNot) {
    FilterExpression cond = createExpr(LogEntryField::LEVEL, FilterOperator::EQUALS, "DEBUG");
    FilterExpression not_cond = cond.Not();

    // The 'not_cond' should now be a copy of 'cond' but with 'negated_' flag set to true.
    ASSERT_TRUE(not_cond.isCondition()); // It's still a condition, but negated
    EXPECT_TRUE(not_cond.isNegated());   // Check the new negated flag
    EXPECT_EQ(not_cond.getCondition()->field, LogEntryField::LEVEL); // Verify it's the original condition
    EXPECT_EQ(not_cond.getCondition()->value, "DEBUG");
    
    // NOT(NOT(cond)) should simplify back to the original cond (not negated)
    FilterExpression not_not_cond = not_cond.Not();
    ASSERT_TRUE(not_not_cond.isCondition());
    EXPECT_FALSE(not_not_cond.isNegated()); // Should no longer be negated
    EXPECT_EQ(not_not_cond.getCondition()->value, "DEBUG");
}

// --- FilterExpression Evaluation Tests ---

TEST_F(FilterExpressionTest, EvaluateStringEquals) {
    auto entry = createLogEntry(LogLevel::INFO, "User logged in", "auth.log");
    
    // Case-sensitive match
    auto expr_cs_match = createExpr(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO", FilterValueType::STRING, true);
    EXPECT_TRUE(expr_cs_match.evaluate(entry));

    // Case-sensitive mismatch
    auto expr_cs_mismatch = createExpr(LogEntryField::LEVEL, FilterOperator::EQUALS, "info", FilterValueType::STRING, true);
    EXPECT_FALSE(expr_cs_mismatch.evaluate(entry));

    // Case-insensitive match
    auto expr_ci_match = createExpr(LogEntryField::LEVEL, FilterOperator::EQUALS, "info", FilterValueType::STRING, false);
    EXPECT_TRUE(expr_ci_match.evaluate(entry));
}

TEST_F(FilterExpressionTest, EvaluateStringContains) {
    auto entry = createLogEntry(LogLevel::DEBUG, "Processing user_id:123", "worker.log");

    // Case-sensitive contains
    auto expr_cs_match = createExpr(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "user_id", FilterValueType::STRING, true);
    EXPECT_TRUE(expr_cs_match.evaluate(entry));

    // Case-sensitive no match
    auto expr_cs_mismatch = createExpr(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "User_id", FilterValueType::STRING, true);
    EXPECT_FALSE(expr_cs_mismatch.evaluate(entry));
    
    // Case-insensitive contains
    auto expr_ci_match = createExpr(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "User_id", FilterValueType::STRING, false);
    EXPECT_TRUE(expr_ci_match.evaluate(entry));
}

TEST_F(FilterExpressionTest, EvaluateNumericComparison) {
    auto entry = createLogEntry(LogLevel::WARNING, "Response time high", "perf.log", {{"response_time_ms", "550"}});

    auto expr_gt = createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "500", FilterValueType::INT, true, "response_time_ms");
    EXPECT_TRUE(expr_gt.evaluate(entry));
    
    auto expr_lt = createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN, "600", FilterValueType::INT, true, "response_time_ms");
    EXPECT_TRUE(expr_lt.evaluate(entry));

    auto expr_gt_fail = createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "600", FilterValueType::INT, true, "response_time_ms");
    EXPECT_FALSE(expr_gt_fail.evaluate(entry));
    
    // Test with a field that doesn't exist
    auto expr_missing_field = createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "100", FilterValueType::INT, true, "non_existent");
    EXPECT_FALSE(expr_missing_field.evaluate(entry));

    // Test with non-numeric value that should fail conversion
    auto entry_bad_num = createLogEntry(LogLevel::ERROR, "Bad data", "data.log", {{"value", "not_a_number"}});
    auto expr_bad_num = createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "123", FilterValueType::INT, true, "value");
    EXPECT_FALSE(expr_bad_num.evaluate(entry_bad_num)); // Expecting false due to conversion failure
}

TEST_F(FilterExpressionTest, EvaluateDoubleComparison) {
    auto entry = createLogEntry(LogLevel::INFO, "Calculation result", "calc.log", {{"result", "123.456789"}});
    
    // Exact match (within epsilon)
    auto expr_exact = createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "123.456789", FilterValueType::DOUBLE, true, "result");
    EXPECT_TRUE(expr_exact.evaluate(entry));

    // Close match (within epsilon)
    auto expr_close = createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "123.4567890001", FilterValueType::DOUBLE, true, "result");
    EXPECT_TRUE(expr_close.evaluate(entry));

    // Greater than
    auto expr_gt = createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "123.456", FilterValueType::DOUBLE, true, "result");
    EXPECT_TRUE(expr_gt.evaluate(entry));

    // Less than
    auto expr_lt = createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN, "123.457", FilterValueType::DOUBLE, true, "result");
    EXPECT_TRUE(expr_lt.evaluate(entry));
    
    // Test with non-numeric value that should fail conversion
    auto entry_bad_double = createLogEntry(LogLevel::ERROR, "Bad double data", "data.log", {{"value", "not_a_double"}});
    auto expr_bad_double = createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "1.23", FilterValueType::DOUBLE, true, "value");
    EXPECT_FALSE(expr_bad_double.evaluate(entry_bad_double)); // Expecting false due to conversion failure
}


TEST_F(FilterExpressionTest, EvaluateBoolComparison) {
    auto entry_true = createLogEntry(LogLevel::INFO, "Status is true", "status.log", {{"is_active", "true"}});
    auto entry_false = createLogEntry(LogLevel::INFO, "Status is false", "status.log", {{"is_active", "false"}});
    auto entry_zero = createLogEntry(LogLevel::INFO, "Status is zero", "status.log", {{"is_active", "0"}});
    auto entry_one = createLogEntry(LogLevel::INFO, "Status is one", "status.log", {{"is_active", "1"}});
    auto entry_invalid = createLogEntry(LogLevel::INFO, "Status invalid", "status.log", {{"is_active", "yes"}});

    // EQUALS true
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "true", FilterValueType::BOOL, true, "is_active").evaluate(entry_true));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "1", FilterValueType::BOOL, true, "is_active").evaluate(entry_one));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "true", FilterValueType::BOOL, true, "is_active").evaluate(entry_false));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "true", FilterValueType::BOOL, true, "is_active").evaluate(entry_zero));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "true", FilterValueType::BOOL, true, "is_active").evaluate(entry_invalid)); // Invalid conversion

    // EQUALS false
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "false", FilterValueType::BOOL, true, "is_active").evaluate(entry_false));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "0", FilterValueType::BOOL, true, "is_active").evaluate(entry_zero));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "false", FilterValueType::BOOL, true, "is_active").evaluate(entry_true));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "false", FilterValueType::BOOL, true, "is_active").evaluate(entry_one));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "false", FilterValueType::BOOL, true, "is_active").evaluate(entry_invalid));
}

TEST_F(FilterExpressionTest, EvaluateDateTimeComparison) {
    auto entry_past = createLogEntry(LogLevel::INFO, "Event in the past", "time.log", {}, std::nullopt, std::chrono::system_clock::now() - std::chrono::hours(1));
    auto entry_future = createLogEntry(LogLevel::INFO, "Event in the future", "time.log", {}, std::nullopt, std::chrono::system_clock::now() + std::chrono::hours(1));
    auto entry_now = createLogEntry(LogLevel::INFO, "Event now", "time.log", {}, std::nullopt, std::chrono::system_clock::now());

    // Use a precise format that Utils::parseTime should handle
    std::string ts_past_str = Utils::formatTimestamp(entry_past.timestamp.value());
    std::string ts_future_str = Utils::formatTimestamp(entry_future.timestamp.value());
    std::string ts_now_str = Utils::formatTimestamp(entry_now.timestamp.value());

    // EQUALS
    EXPECT_TRUE(createExpr(LogEntryField::TIMESTAMP, FilterOperator::EQUALS, ts_past_str, FilterValueType::DATETIME).evaluate(entry_past));
    EXPECT_FALSE(createExpr(LogEntryField::TIMESTAMP, FilterOperator::EQUALS, ts_future_str, FilterValueType::DATETIME).evaluate(entry_past));

    // GREATER_THAN
    EXPECT_TRUE(createExpr(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, ts_past_str, FilterValueType::DATETIME).evaluate(entry_future));
    EXPECT_FALSE(createExpr(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, ts_future_str, FilterValueType::DATETIME).evaluate(entry_future));
    EXPECT_TRUE(createExpr(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, ts_past_str, FilterValueType::DATETIME).evaluate(entry_now));

    // LESS_THAN_OR_EQUAL
    EXPECT_TRUE(createExpr(LogEntryField::TIMESTAMP, FilterOperator::LESS_THAN_OR_EQUAL, ts_future_str, FilterValueType::DATETIME).evaluate(entry_future));
    EXPECT_TRUE(createExpr(LogEntryField::TIMESTAMP, FilterOperator::LESS_THAN_OR_EQUAL, ts_future_str, FilterValueType::DATETIME).evaluate(entry_now));
    EXPECT_FALSE(createExpr(LogEntryField::TIMESTAMP, FilterOperator::LESS_THAN_OR_EQUAL, ts_now_str, FilterValueType::DATETIME).evaluate(entry_future));

    // Test with invalid datetime string
    auto expr_invalid_dt = createExpr(LogEntryField::TIMESTAMP, FilterOperator::EQUALS, "not_a_datetime", FilterValueType::DATETIME);
    EXPECT_FALSE(expr_invalid_dt.evaluate(entry_now));
}

// --- Tests for VERSION and IP_ADDRESS types ---

TEST_F(FilterExpressionTest, EvaluateVersionComparison) {
    auto entry_v100 = createLogEntry(LogLevel::INFO, "Version 1.0.0", "ver.log", {{"app_version", "1.0.0"}});
    auto entry_v110 = createLogEntry(LogLevel::INFO, "Version 1.1.0", "ver.log", {{"app_version", "1.1.0"}});
    auto entry_v110_patch = createLogEntry(LogLevel::INFO, "Version 1.1.0-patch", "ver.log", {{"app_version", "1.1.0-patch"}});
    auto entry_v200 = createLogEntry(LogLevel::INFO, "Version 2.0.0", "ver.log", {{"app_version", "2.0.0"}});
    auto entry_v_invalid = createLogEntry(LogLevel::INFO, "Invalid version", "ver.log", {{"app_version", "invalid-version"}});

    // EQUALS
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "1.0.0", FilterValueType::VERSION, true, "app_version").evaluate(entry_v100));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "1.1.0", FilterValueType::VERSION, true, "app_version").evaluate(entry_v100));

    // GREATER_THAN
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "1.0.0", FilterValueType::VERSION, true, "app_version").evaluate(entry_v110));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "1.1.0-patch", FilterValueType::VERSION, true, "app_version").evaluate(entry_v110)); // 1.1.0 is greater than 1.1.0-patch
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "1.1.0", FilterValueType::VERSION, true, "app_version").evaluate(entry_v200));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "2.0.0", FilterValueType::VERSION, true, "app_version").evaluate(entry_v200));

    // LESS_THAN_OR_EQUAL
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "1.1.0", FilterValueType::VERSION, true, "app_version").evaluate(entry_v100));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "1.1.0-patch", FilterValueType::VERSION, true, "app_version").evaluate(entry_v110_patch));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "2.0.0", FilterValueType::VERSION, true, "app_version").evaluate(entry_v200));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "1.0.0", FilterValueType::VERSION, true, "app_version").evaluate(entry_v110));

    // Test with invalid version strings (should fail comparison)
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "1.0.0", FilterValueType::VERSION, true, "app_version").evaluate(entry_v_invalid));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "invalid-version", FilterValueType::VERSION, true, "app_version").evaluate(entry_v100));
}

TEST_F(FilterExpressionTest, EvaluateIpAddressComparison) {
    auto entry_ip1 = createLogEntry(LogLevel::INFO, "IP address 1", "ip.log", {{"client_ip", "192.168.1.100"}});
    auto entry_ip2 = createLogEntry(LogLevel::INFO, "IP address 2", "ip.log", {{"client_ip", "192.168.1.200"}});
    auto entry_ip_ipv6 = createLogEntry(LogLevel::INFO, "IP address IPv6", "ip.log", {{"client_ip", "2001:0db8:85a3:0000:0000:8a2e:0370:7334"}});
    auto entry_ip_invalid = createLogEntry(LogLevel::INFO, "Invalid IP", "ip.log", {{"client_ip", "invalid-ip"}});

    // EQUALS
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "192.168.1.100", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip1));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "192.168.1.200", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip1));

    // GREATER_THAN
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "192.168.1.100", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip2));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "192.168.1.200", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip2));

    // LESS_THAN_OR_EQUAL
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "192.168.1.200", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip2));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "192.168.1.200", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip1));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "192.168.1.100", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip2));

    // Test with IPv6 address
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "2001:0db8:85a3:0000:0000:8a2e:0370:7334", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip_ipv6));

    // Test with invalid IP strings (should fail comparison)
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "192.168.1.100", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip_invalid));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "invalid-ip", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip1));
}

TEST_F(FilterExpressionTest, EvaluateInOperator) {
    auto entry_apple = createLogEntry(LogLevel::INFO, "Fruit: Apple", "fruit.log", {{"item", "Apple"}});
    auto entry_banana = createLogEntry(LogLevel::INFO, "Fruit: Banana", "fruit.log", {{"item", "Banana"}});
    auto entry_cherry = createLogEntry(LogLevel::INFO, "Fruit: Cherry", "fruit.log", {{"item", "Cherry"}});
    auto entry_date = createLogEntry(LogLevel::INFO, "Fruit: Date", "fruit.log", {{"item", "Date"}});

    // IN operator - match
    auto expr_in_match = createExpr(LogEntryField::CUSTOM, FilterOperator::IN, R"json(["Apple", "Banana"])json", FilterValueType::STRING, true, "item");
    EXPECT_TRUE(expr_in_match.evaluate(entry_apple));
    EXPECT_TRUE(expr_in_match.evaluate(entry_banana));

    // IN operator - no match
    auto expr_in_no_match = createExpr(LogEntryField::CUSTOM, FilterOperator::IN, R"json(["Apple", "Banana"])json", FilterValueType::STRING, true, "item");
    EXPECT_FALSE(expr_in_no_match.evaluate(entry_cherry));

    // IN operator - case insensitive
    auto expr_in_ci = createExpr(LogEntryField::CUSTOM, FilterOperator::IN, R"json(["apple", "banana"])json", FilterValueType::STRING, false, "item");
    EXPECT_TRUE(expr_in_ci.evaluate(entry_apple));
    EXPECT_TRUE(expr_in_ci.evaluate(entry_banana));
    EXPECT_FALSE(expr_in_ci.evaluate(entry_cherry));

    // IN operator - empty array
    auto expr_in_empty = createExpr(LogEntryField::CUSTOM, FilterOperator::IN, R"json([])json", FilterValueType::STRING, true, "item");
    EXPECT_FALSE(expr_in_empty.evaluate(entry_apple)); // Empty IN should never match

    // IN operator - invalid JSON
    auto expr_in_invalid_json = createExpr(LogEntryField::CUSTOM, FilterOperator::IN, R"json(["Apple",)json", FilterValueType::STRING, true, "item");
    EXPECT_FALSE(expr_in_invalid_json.evaluate(entry_apple)); // Invalid JSON should not match

    // IN operator - JSON is not an array
    auto expr_in_not_array = createExpr(LogEntryField::CUSTOM, FilterOperator::IN, R"json("Apple")json", FilterValueType::STRING, true, "item");
    EXPECT_FALSE(expr_in_not_array.evaluate(entry_apple)); // Not an array should not match

    // IN operator - array with non-string items (should be ignored)
    auto expr_in_mixed_types = createExpr(LogEntryField::CUSTOM, FilterOperator::IN, R"json(["Apple", 123, true, null])json", FilterValueType::STRING, true, "item");
    EXPECT_TRUE(expr_in_mixed_types.evaluate(entry_apple)); // Should match "Apple"
    EXPECT_FALSE(expr_in_mixed_types.evaluate(entry_banana)); // Should not match "Banana"

    // NOT_IN operator - match
    auto expr_not_in_match = createExpr(LogEntryField::CUSTOM, FilterOperator::NOT_IN, R"json(["Apple", "Banana"])json", FilterValueType::STRING, true, "item");
    EXPECT_FALSE(expr_not_in_match.evaluate(entry_apple));
    EXPECT_FALSE(expr_not_in_match.evaluate(entry_banana));

    // NOT_IN operator - no match
    auto expr_not_in_no_match = createExpr(LogEntryField::CUSTOM, FilterOperator::NOT_IN, R"json(["Apple", "Banana"])json", FilterValueType::STRING, true, "item");
    EXPECT_TRUE(expr_not_in_no_match.evaluate(entry_cherry));
    EXPECT_TRUE(expr_not_in_no_match.evaluate(entry_date));
}

TEST_F(FilterExpressionTest, EvaluateRegexMatch) {
    auto entry_email = createLogEntry(LogLevel::INFO, "Contact: test@example.com", "contact.log");
    auto entry_phone = createLogEntry(LogLevel::INFO, "Call: +1-555-123-4567", "contact.log");

    // Regex match (case-sensitive)
    auto expr_regex_match_cs = createExpr(LogEntryField::MESSAGE, FilterOperator::REGEX_MATCH, R"(\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b)", FilterValueType::STRING, true);
    EXPECT_TRUE(expr_regex_match_cs.evaluate(entry_email));
    EXPECT_FALSE(expr_regex_match_cs.evaluate(entry_phone));

    // Regex match (case-insensitive)
    auto expr_regex_match_ci = createExpr(LogEntryField::MESSAGE, FilterOperator::REGEX_MATCH, R"(\b[A-Za-z0-9._%+-]+@[a-z0-9.-]+\.[a-z]{2,}\b)", FilterValueType::STRING, false);
    EXPECT_TRUE(expr_regex_match_ci.evaluate(entry_email));

    // Regex mismatch
    auto expr_regex_mismatch = createExpr(LogEntryField::MESSAGE, FilterOperator::REGEX_MATCH, R"(\d{3}-\d{3}-\d{4})", FilterValueType::STRING, true);
    EXPECT_TRUE(expr_regex_mismatch.evaluate(entry_phone));
    EXPECT_FALSE(expr_regex_mismatch.evaluate(entry_email));
    
    // Invalid regex pattern
    auto expr_invalid_regex = createExpr(LogEntryField::MESSAGE, FilterOperator::REGEX_MATCH, R"([)", FilterValueType::STRING, true);
    EXPECT_FALSE(expr_invalid_regex.evaluate(entry_email)); // Invalid regex should not match
}

TEST_F(FilterExpressionTest, EvaluateIsPresentAndIsAbsent) {
    auto entry_with_id = createLogEntry(LogLevel::INFO, "Message with ID", "log.log", {{"custom_field", "value"}}, 123, std::nullopt, 45);
    auto entry_without_id = createLogEntry(LogLevel::INFO, "Message without ID", "log.log", {{"custom_field", "value"}}, std::nullopt, std::nullopt, 45);
    auto entry_with_custom = createLogEntry(LogLevel::INFO, "Message with custom field", "log.log", {{"custom_field", "value"}});
    auto entry_without_custom = createLogEntry(LogLevel::INFO, "Message without custom field", "log.log", {});

    // IS_PRESENT tests for built-in fields (use default valueType and caseSensitive)
    EXPECT_TRUE(createExpr(LogEntryField::ID, FilterOperator::IS_PRESENT, "").evaluate(entry_with_id));
    EXPECT_FALSE(createExpr(LogEntryField::ID, FilterOperator::IS_PRESENT, "").evaluate(entry_without_id));
    EXPECT_TRUE(createExpr(LogEntryField::LINE_NUMBER, FilterOperator::IS_PRESENT, "").evaluate(entry_with_id)); // Line number is set
    EXPECT_TRUE(createExpr(LogEntryField::LINE_NUMBER, FilterOperator::IS_PRESENT, "").evaluate(entry_without_id)); // Line number is set even if ID is not

    // IS_PRESENT tests for custom fields (correctly pass valueType, caseSensitive, and customField name)
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::IS_PRESENT, "", FilterValueType::STRING, true, "custom_field").evaluate(entry_with_custom));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::IS_PRESENT, "", FilterValueType::STRING, true, "custom_field").evaluate(entry_without_custom));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::IS_PRESENT, "", FilterValueType::STRING, true, "non_existent_custom").evaluate(entry_with_custom));

    // IS_ABSENT tests for built-in fields
    EXPECT_TRUE(createExpr(LogEntryField::ID, FilterOperator::IS_ABSENT, "").evaluate(entry_without_id));
    EXPECT_FALSE(createExpr(LogEntryField::ID, FilterOperator::IS_ABSENT, "").evaluate(entry_with_id));

    // IS_ABSENT tests for custom fields (correctly pass valueType, caseSensitive, and customField name)
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::IS_ABSENT, "", FilterValueType::STRING, true, "custom_field").evaluate(entry_without_custom));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::IS_ABSENT, "", FilterValueType::STRING, true, "custom_field").evaluate(entry_with_custom));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::IS_ABSENT, "", FilterValueType::STRING, true, "non_existent_custom").evaluate(entry_with_custom));
}

TEST_F(FilterExpressionTest, EvaluateStringOperatorsEdgeCases) {
    auto entry = createLogEntry(LogLevel::INFO, "  leading and trailing spaces  ", "spaces.log");

    // STARTS_WITH
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "  leading", FilterValueType::STRING, true).evaluate(entry));
    EXPECT_FALSE(createExpr(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "leading", FilterValueType::STRING, true).evaluate(entry)); // Doesn't start with "leading"
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "  leading", FilterValueType::STRING, false).evaluate(entry)); // CI match

    // ENDS_WITH
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "spaces  ", FilterValueType::STRING, true).evaluate(entry));
    EXPECT_FALSE(createExpr(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "spaces", FilterValueType::STRING, true).evaluate(entry)); // Doesn't end with "spaces"
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "SPACES  ", FilterValueType::STRING, false).evaluate(entry)); // CI match

    // CONTAINS
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "and trailing", FilterValueType::STRING, true).evaluate(entry));
    EXPECT_FALSE(createExpr(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "trailing", FilterValueType::STRING, true).evaluate(entry)); // Doesn't contain "trailing" without surrounding spaces
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "TRAILING", FilterValueType::STRING, false).evaluate(entry)); // CI match

    // Empty string comparisons
    auto entry_empty_msg = createLogEntry(LogLevel::INFO, "", "empty.log");
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::EQUALS, "", FilterValueType::STRING, true).evaluate(entry_empty_msg));
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "", FilterValueType::STRING, true).evaluate(entry_empty_msg)); // Empty string is contained in any string
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "", FilterValueType::STRING, true).evaluate(entry_empty_msg));
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "", FilterValueType::STRING, true).evaluate(entry_empty_msg));
    EXPECT_FALSE(createExpr(LogEntryField::MESSAGE, FilterOperator::EQUALS, "a", FilterValueType::STRING, true).evaluate(entry_empty_msg));
}

TEST_F(FilterExpressionTest, EvaluateNumericBoundaryTests) {
    auto entry = createLogEntry(LogLevel::INFO, "Value is 100", "boundary.log", {{"count", "100"}});

    // GREATER_THAN_OR_EQUAL
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN_OR_EQUAL, "100", FilterValueType::INT, true, "count").evaluate(entry)); // Equal
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN_OR_EQUAL, "99", FilterValueType::INT, true, "count").evaluate(entry)); // Greater
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN_OR_EQUAL, "101", FilterValueType::INT, true, "count").evaluate(entry)); // Less

    // LESS_THAN_OR_EQUAL
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "100", FilterValueType::INT, true, "count").evaluate(entry)); // Equal
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "101", FilterValueType::INT, true, "count").evaluate(entry)); // Less
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "99", FilterValueType::INT, true, "count").evaluate(entry)); // Greater
}

TEST_F(FilterExpressionTest, EvaluateEmptyExpression) {
    auto entry = createLogEntry(LogLevel::INFO, "Any message", "any.log");
    // An empty FilterExpression is created via default construction.
    FilterExpression empty_expr; 
    
    EXPECT_TRUE(empty_expr.evaluate(entry)); // Empty expression should always match

    // Check if it's neither a condition nor a logical operator, implying an empty state.
    EXPECT_FALSE(empty_expr.isCondition()); 
    EXPECT_FALSE(empty_expr.isLogical());   
}

TEST_F(FilterExpressionTest, EvaluateStringConversionsForNonStringTypes) {
    // Ensure that string comparison logic in the default/auto/unknown handler is robust
    // when passed to types that are not STRING, AUTO, or UNKNOWN.
    // The refactored code should ensure these are handled by their specific types (INT, DOUBLE, etc.)
    // or throw if the type is truly unhandled.

    // Test INT comparison using string logic (should not happen due to correct type dispatch)
    auto entry_int = createLogEntry(LogLevel::INFO, "Int value", "int.log", {{"count", "100"}});
    // This test case is more about ensuring the type dispatch works correctly.
    // If it falls into string logic by mistake, it might yield unexpected results.
    // The correct behavior is to use INT logic.
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "100", FilterValueType::INT, true, "count").evaluate(entry_int));
    // This condition would be evaluated by the INT logic, not string logic.

    // The refactoring of the default case ensures that if a type is not INT, DOUBLE, etc.,
    // AND it's not STRING, AUTO, UNKNOWN, then it will throw.
    // This test indirectly verifies that INT, DOUBLE, etc. are handled before the default.
}

// --- Tests for helper functions (stringToBool, etc.) ---
// These are implicitly tested by EvaluateBoolComparison and other tests,
// but explicit tests can be added if needed for deeper coverage.

// TEST_F(FilterExpressionTest, StringToBoolHelper) { ... }


// --- Test for custom fields and specific LogEntryFields ---

TEST_F(FilterExpressionTest, CustomFieldAccess) {
    auto entry = createLogEntry(LogLevel::INFO, "Log entry", "custom.log", {{"user_id", "abc123"}, {"tenant_id", "xyz789"}});

    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "abc123", FilterValueType::STRING, true, "user_id").evaluate(entry));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "XYZ789", FilterValueType::STRING, false, "tenant_id").evaluate(entry)); // Case-insensitive
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "def456", FilterValueType::STRING, true, "user_id").evaluate(entry));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "xyz789", FilterValueType::STRING, true, "tenant_id").evaluate(entry)); // Case-sensitive mismatch
}

TEST_F(FilterExpressionTest, BuiltInFieldsAccess) {
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
    EXPECT_TRUE(createExpr(LogEntryField::LEVEL, FilterOperator::EQUALS, "DEBUG", FilterValueType::STRING, true).evaluate(entry));
    EXPECT_TRUE(createExpr(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "heartbeat", FilterValueType::STRING, true).evaluate(entry));
    EXPECT_TRUE(createExpr(LogEntryField::SOURCE_FILE, FilterOperator::EQUALS, "main.cpp", FilterValueType::STRING, true).evaluate(entry));
    EXPECT_TRUE(createExpr(LogEntryField::ID, FilterOperator::EQUALS, "101", FilterValueType::INT, true).evaluate(entry));
    EXPECT_TRUE(createExpr(LogEntryField::LINE_NUMBER, FilterOperator::EQUALS, "55", FilterValueType::INT, true).evaluate(entry));
    EXPECT_TRUE(createExpr(LogEntryField::THREAD_ID, FilterOperator::EQUALS, "main_thread", FilterValueType::STRING, true).evaluate(entry));
    EXPECT_TRUE(createExpr(LogEntryField::MODULE, FilterOperator::EQUALS, "AppModule", FilterValueType::STRING, true).evaluate(entry));
    EXPECT_TRUE(createExpr(LogEntryField::HOST, FilterOperator::EQUALS, "server-123", FilterValueType::STRING, true).evaluate(entry));
    
    // Test custom field when it also exists
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "custom_value", FilterValueType::STRING, true, "custom_field").evaluate(entry));
}

// --- Test for expression negation on logical operators ---
TEST_F(FilterExpressionTest, NegationOfLogicalExpressions) {
    auto entry_a_and_b = createLogEntry(LogLevel::INFO, "A and B", "logic.log"); // Assume A and B are true
    auto entry_a_not_b = createLogEntry(LogLevel::INFO, "A not B", "logic.log"); // Assume A is true, B is false
    auto entry_not_a_b = createLogEntry(LogLevel::INFO, "Not A and B", "logic.log"); // Assume A is false, B is true

    // Create conditions that will be true for specific entries (simplified for this test)
    auto cond_a = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::EQUALS, "A and B")); // True for entry_a_and_b
    auto cond_b = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::EQUALS, "A and B")); // True for entry_a_and_b
    auto cond_c = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::EQUALS, "A not B")); // True for entry_a_not_b
    
    // De Morgan's Law: NOT (A AND B) == (NOT A) OR (NOT B)
    FilterExpression original_and = cond_a.And(cond_b);
    FilterExpression negated_and = original_and.Not();

    // With entry_a_and_b: original_and is TRUE. negated_and should be FALSE.
    EXPECT_TRUE(original_and.evaluate(entry_a_and_b));
    EXPECT_FALSE(negated_and.evaluate(entry_a_and_b));

    // With entry_a_not_b (A true, B false): original_and is FALSE. negated_and should be TRUE.
    EXPECT_FALSE(original_and.evaluate(entry_a_not_b));
    EXPECT_TRUE(negated_and.evaluate(entry_a_not_b));
    
    // With entry_not_a_b (A false, B true): original_and is FALSE. negated_and should be TRUE.
    EXPECT_FALSE(original_and.evaluate(entry_not_a_b));
    EXPECT_TRUE(negated_and.evaluate(entry_not_a_b));


    // De Morgan's Law: NOT (A OR B) == (NOT A) AND (NOT B)
    FilterExpression original_or = cond_a.Or(cond_c); // Using cond_c for "A not B" scenario
    FilterExpression negated_or = original_or.Not();

    // With entry_a_and_b (A true, B false): original_or is TRUE. negated_or should be FALSE.
    EXPECT_TRUE(original_or.evaluate(entry_a_and_b));
    EXPECT_FALSE(negated_or.evaluate(entry_a_and_b));
    
    // With entry_a_not_b (A true, B false): original_or is TRUE. negated_or should be FALSE.
    EXPECT_TRUE(original_or.evaluate(entry_a_not_b));
    EXPECT_FALSE(negated_or.evaluate(entry_a_not_b));

    // With entry_not_a_b (A false, B true): original_or is TRUE. negated_or should be FALSE.
    EXPECT_TRUE(original_or.evaluate(entry_not_a_b));
    EXPECT_FALSE(negated_or.evaluate(entry_not_a_b));
}
