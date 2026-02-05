#include <gtest/gtest.h>
#include "filter/Core.h"
#include "core/LogTypes.h"
#include <chrono>
#include <map>
#include <optional>

// Consolidated test fixture for FilterExpression tests
class FilterExpressionTest : public ::testing::Test {
protected:
    LogEntry createLogEntry(
        LogLevel level,
        const std::string& message,
        const std::string& sourceFile = "test.log",
        const std::map<std::string, std::string>& customFields = {}
    ) {
        LogEntry entry;
        entry.id = nextId++;
        entry.sourceFile = sourceFile;
        entry.timestamp = std::chrono::system_clock::now();
        entry.level = level;
        entry.message = message;
        entry.customFields = customFields;
        return entry;
    }

    FilterCondition createCondition(
        LogEntryField field,
        FilterOperator op,
        const std::string& value,
        FilterValueType valueType = FilterValueType::STRING,
        bool caseSensitive = true
    ) {
        FilterCondition fc;
        fc.field = field;
        fc.op = op;
        fc.value = value;
        fc.valueType = valueType;
        fc.caseSensitive = caseSensitive;
        return fc;
    }

private:
    size_t nextId = 1;
};

// --- Legacy Filter Tests (maintained for backward compatibility) ---

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
    FilterExpression cond1 = FilterExpression::create(createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO"));
    FilterExpression cond2 = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "user"));
    FilterExpression expr1 = cond1.And(cond2);

    ASSERT_TRUE(expr1.isLogical());
    EXPECT_EQ(*expr1.getLogicalOperator(), FilterLogicalOperator::AND);
    ASSERT_EQ(expr1.getExpressions().size(), 2);

    // (cond1 AND cond2) AND cond3 -> should be flattened to AND [cond1, cond2, cond3]
    FilterExpression cond3 = FilterExpression::create(createCondition(LogEntryField::SOURCE_FILE, FilterOperator::ENDS_WITH, ".log"));
    FilterExpression expr2 = expr1.And(cond3);

    ASSERT_TRUE(expr2.isLogical());
    EXPECT_EQ(*expr2.getLogicalOperator(), FilterLogicalOperator::AND);
    ASSERT_EQ(expr2.getExpressions().size(), 3);
}

TEST_F(FilterExpressionTest, FluentApiOrOptimization) {
    // cond1 OR cond2
    FilterExpression cond1 = FilterExpression::create(createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "ERROR"));
    FilterExpression cond2 = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "database"));
    FilterExpression expr1 = cond1.Or(cond2);

    ASSERT_TRUE(expr1.isLogical());
    EXPECT_EQ(*expr1.getLogicalOperator(), FilterLogicalOperator::OR);
    ASSERT_EQ(expr1.getExpressions().size(), 2);

    // (cond1 OR cond2) OR cond3 -> should be flattened to OR [cond1, cond2, cond3]
    FilterExpression cond3 = FilterExpression::create(createCondition(LogEntryField::SOURCE_FILE, FilterOperator::STARTS_WITH, "app"));
    FilterExpression expr2 = expr1.Or(cond3);

    ASSERT_TRUE(expr2.isLogical());
    EXPECT_EQ(*expr2.getLogicalOperator(), FilterLogicalOperator::OR);
    ASSERT_EQ(expr2.getExpressions().size(), 3);
}

TEST_F(FilterExpressionTest, FluentApiNot) {
    FilterExpression cond = FilterExpression::create(createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "DEBUG"));
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
    auto expr_cs_match = FilterExpression::create(createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO", FilterValueType::STRING, true));
    EXPECT_TRUE(expr_cs_match.evaluate(entry));

    // Case-sensitive mismatch
    auto expr_cs_mismatch = FilterExpression::create(createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "info", FilterValueType::STRING, true));
    EXPECT_FALSE(expr_cs_mismatch.evaluate(entry));

    // Case-insensitive match
    auto expr_ci_match = FilterExpression::create(createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "info", FilterValueType::STRING, false));
    EXPECT_TRUE(expr_ci_match.evaluate(entry));
}

TEST_F(FilterExpressionTest, EvaluateStringContains) {
    auto entry = createLogEntry(LogLevel::DEBUG, "Processing user_id:123", "worker.log");

    // Case-sensitive contains
    auto expr_cs_match = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "user_id", FilterValueType::STRING, true));
    EXPECT_TRUE(expr_cs_match.evaluate(entry));

    // Case-sensitive no match
    auto expr_cs_mismatch = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "User_id", FilterValueType::STRING, true));
    EXPECT_FALSE(expr_cs_mismatch.evaluate(entry));
    
    // Case-insensitive contains
    auto expr_ci_match = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "User_id", FilterValueType::STRING, false));
    EXPECT_TRUE(expr_ci_match.evaluate(entry));
}

TEST_F(FilterExpressionTest, EvaluateNumericComparison) {
    auto entry = createLogEntry(LogLevel::WARNING, "Response time high", "perf.log", {{"response_time_ms", "550"}});

    auto cond_gt = createCondition(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "500", FilterValueType::NUMERIC);
    cond_gt.customField = "response_time_ms";
    auto expr_gt = FilterExpression::create(cond_gt);
    EXPECT_TRUE(expr_gt.evaluate(entry));
    
    auto cond_lt = createCondition(LogEntryField::CUSTOM, FilterOperator::LESS_THAN, "600", FilterValueType::NUMERIC);
    cond_lt.customField = "response_time_ms";
    auto expr_lt = FilterExpression::create(cond_lt);
    EXPECT_TRUE(expr_lt.evaluate(entry));

    auto cond_gt_fail = createCondition(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "600", FilterValueType::NUMERIC);
    cond_gt_fail.customField = "response_time_ms";
    auto expr_gt_fail = FilterExpression::create(cond_gt_fail);
    EXPECT_FALSE(expr_gt_fail.evaluate(entry));
    
    // Test with a field that doesn't exist
    auto cond_missing = createCondition(LogEntryField::CUSTOM, FilterOperator::EQUALS, "100", FilterValueType::NUMERIC);
    cond_missing.customField = "non_existent";
    auto expr_missing_field = FilterExpression::create(cond_missing);
    EXPECT_FALSE(expr_missing_field.evaluate(entry));
}

TEST_F(FilterExpressionTest, EvaluateLogicalAnd) {
    auto entry_match = createLogEntry(LogLevel::ERROR, "DB connection failed", "db.log");
    auto entry_mismatch_level = createLogEntry(LogLevel::INFO, "DB connection ok", "db.log");
    auto entry_mismatch_msg = createLogEntry(LogLevel::ERROR, "Cache cleared", "cache.log");

    auto cond1 = FilterExpression::create(createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "ERROR", FilterValueType::STRING, false));
    auto cond2 = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "db connection", FilterValueType::STRING, false));
    auto expr = cond1.And(cond2);

    EXPECT_TRUE(expr.evaluate(entry_match));
    EXPECT_FALSE(expr.evaluate(entry_mismatch_level));
    EXPECT_FALSE(expr.evaluate(entry_mismatch_msg));
}

TEST_F(FilterExpressionTest, EvaluateLogicalOr) {
    auto entry_match_1 = createLogEntry(LogLevel::CRITICAL, "System halted", "kernel.log");
    auto entry_match_2 = createLogEntry(LogLevel::ERROR, "Service unavailable", "app.log");
    auto entry_mismatch = createLogEntry(LogLevel::WARNING, "High memory usage", "monitor.log");

    auto cond1 = FilterExpression::create(createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "CRITICAL"));
    auto cond2 = FilterExpression::create(createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "ERROR"));
    auto expr = cond1.Or(cond2);

    EXPECT_TRUE(expr.evaluate(entry_match_1));
    EXPECT_TRUE(expr.evaluate(entry_match_2));
    EXPECT_FALSE(expr.evaluate(entry_mismatch));
}

TEST_F(FilterExpressionTest, EvaluateLogicalNot) {
    auto entry_debug = createLogEntry(LogLevel::DEBUG, "Trace message", "trace.log");
    auto entry_info = createLogEntry(LogLevel::INFO, "Application starting", "app.log");

    auto cond = FilterExpression::create(createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "DEBUG"));
    auto expr = cond.Not();

    EXPECT_FALSE(expr.evaluate(entry_debug));
    EXPECT_TRUE(expr.evaluate(entry_info));
}

TEST_F(FilterExpressionTest, EvaluateComplexExpression) {
    // (LEVEL == "ERROR" AND MESSAGE CONTAINS "payment") OR (LEVEL == "CRITICAL")
    auto entry_match_1 = createLogEntry(LogLevel::ERROR, "Payment processing failed.", "billing.log");
    auto entry_match_2 = createLogEntry(LogLevel::CRITICAL, "Filesystem full.", "kernel.log");
    auto entry_mismatch_1 = createLogEntry(LogLevel::ERROR, "User auth failed.", "auth.log");
    auto entry_mismatch_2 = createLogEntry(LogLevel::WARNING, "Payment gateway slow.", "billing.log");

    auto cond_err = FilterExpression::create(createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "ERROR"));
    auto cond_payment = FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "payment", FilterValueType::STRING, false));
    auto cond_crit = FilterExpression::create(createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "CRITICAL"));
    
    auto expr = (cond_err.And(cond_payment)).Or(cond_crit);

    EXPECT_TRUE(expr.evaluate(entry_match_1));
    EXPECT_TRUE(expr.evaluate(entry_match_2));
    EXPECT_FALSE(expr.evaluate(entry_mismatch_1));
    EXPECT_FALSE(expr.evaluate(entry_mismatch_2));

    // NOT (LEVEL == "INFO" OR LEVEL == "DEBUG")
    auto entry_info = createLogEntry(LogLevel::INFO, "User logged in.", "auth.log");
    auto entry_debug = createLogEntry(LogLevel::DEBUG, "Cache miss.", "cache.log");
    auto entry_warn = createLogEntry(LogLevel::WARNING, "Disk space low.", "monitor.log");

    auto cond_info = FilterExpression::create(createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO"));
    auto cond_debug = FilterExpression::create(createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "DEBUG"));

    auto expr2 = (cond_info.Or(cond_debug)).Not();

    EXPECT_FALSE(expr2.evaluate(entry_info));
    EXPECT_FALSE(expr2.evaluate(entry_debug));
    EXPECT_TRUE(expr2.evaluate(entry_warn));
}