#include <gtest/gtest.h>
#include "filter/Expression.h"
#include "filter/EnumStringConversions.h"
#include "utils/Version.h"
#include "utils/IpAddress.h"
#include "utils/Time.h"
#include <chrono>
#include <map>
#include <optional>
#include <nlohmann/json.hpp>
#include <arpa/inet.h> // For AF_INET, AF_INET6

// Consolidated test fixture for FilterExpression advanced tests
class FilterExpressionAdvancedTest : public ::testing::Test {
protected:
    LogEntry createLogEntry(
        LogLevel level,
        const std::string& message,
        std::optional<size_t> id = std::nullopt,
        std::optional<size_t> lineNumber = std::nullopt,
        std::optional<std::string> threadId = std::nullopt,
        std::optional<std::string> module = std::nullopt,
        std::optional<std::string> host = std::nullopt,
        std::optional<std::chrono::system_clock::time_point> timestamp = std::nullopt,
        const std::map<std::string, std::string>& customFields = {},
        const std::string& sourceFile = "test.log"
    ) {
        LogEntry entry;
        entry.id = id;
        entry.sourceLineNumber = lineNumber;
        entry.sourceFile = sourceFile;
        entry.timestamp = timestamp.has_value() ? timestamp : std::make_optional(std::chrono::system_clock::now());
        entry.level = level;
        entry.message = message;
        entry.threadId = threadId;
        entry.module = module;
        entry.host = host;
        entry.customFields = customFields;
        return entry;
    }

    FilterCondition createCondition(
        LogEntryField field,
        FilterOperator op,
        const std::string& value,
        FilterValueType valueType = FilterValueType::STRING,
        bool caseSensitive = true,
        std::optional<std::string> customFieldName = std::nullopt
    ) {
        FilterCondition fc;
        fc.field = field;
        fc.op = op;
        fc.value = value;
        fc.valueType = valueType;
        fc.caseSensitive = caseSensitive;
        fc.customField = customFieldName;
        return fc;
    }

    // Helper for testing version comparisons
    void testVersionComparison(const std::string& v1, const std::string& v2, FilterOperator op, bool expected) {
        LogEntry entry = createLogEntry(LogLevel::INFO, "Version test", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, {{"version_field", v1}});
        FilterCondition cond = createCondition(LogEntryField::CUSTOM, op, v2, FilterValueType::VERSION, true, "version_field");
        FilterExpression expr = FilterExpression::create(cond);
        EXPECT_EQ(expr.evaluate(entry), expected) << "Comparison: '" << v1 << "' " << toString(op) << " '" << v2 << "'";
    }

    // Helper for testing IP address comparisons
    void testIpComparison(const std::string& ip1, const std::string& ip2, FilterOperator op, bool expected) {
        LogEntry entry = createLogEntry(LogLevel::INFO, "IP test", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, {{"ip_field", ip1}});
        FilterCondition cond = createCondition(LogEntryField::CUSTOM, op, ip2, FilterValueType::IP_ADDRESS, true, "ip_field");
        FilterExpression expr = FilterExpression::create(cond);
        EXPECT_EQ(expr.evaluate(entry), expected) << "Comparison: '" << ip1 << "' " << toString(op) << " '" << ip2 << "'";
    }
};

// --- Semantic Version Tests ---
TEST_F(FilterExpressionAdvancedTest, EvaluateVersionEquals) {
    testVersionComparison("1.0.0", "1.0.0", FilterOperator::EQUALS, true);
    testVersionComparison("1.0.0", "1.0.1", FilterOperator::EQUALS, false);
    testVersionComparison("1.0.0-alpha", "1.0.0-alpha", FilterOperator::EQUALS, true);
    testVersionComparison("1.0.0-alpha", "1.0.0-beta", FilterOperator::EQUALS, false);
    testVersionComparison("1.0.0+build123", "1.0.0+build123", FilterOperator::EQUALS, true);
    testVersionComparison("1.0.0+build123", "1.0.0+build456", FilterOperator::EQUALS, false); // Build metadata is considered for strict equality
}

TEST_F(FilterExpressionAdvancedTest, EvaluateVersionNotEquals) {
    testVersionComparison("1.0.0", "1.0.1", FilterOperator::NOT_EQUALS, true);
    testVersionComparison("1.0.0", "1.0.0", FilterOperator::NOT_EQUALS, false);
}

TEST_F(FilterExpressionAdvancedTest, EvaluateVersionGreaterThan) {
    testVersionComparison("1.0.1", "1.0.0", FilterOperator::GREATER_THAN, true);
    testVersionComparison("1.1.0", "1.0.0", FilterOperator::GREATER_THAN, true);
    testVersionComparison("2.0.0", "1.0.0", FilterOperator::GREATER_THAN, true);
    testVersionComparison("1.0.0", "1.0.0", FilterOperator::GREATER_THAN, false);
    testVersionComparison("1.0.0-beta", "1.0.0-alpha", FilterOperator::GREATER_THAN, true);
    testVersionComparison("1.0.0", "1.0.0-rc.1", FilterOperator::GREATER_THAN, true); // No prerelease > prerelease
}

TEST_F(FilterExpressionAdvancedTest, EvaluateVersionLessThan) {
    testVersionComparison("1.0.0", "1.0.1", FilterOperator::LESS_THAN, true);
    testVersionComparison("1.0.0-alpha", "1.0.0-beta", FilterOperator::LESS_THAN, true);
    testVersionComparison("1.0.0-rc.1", "1.0.0", FilterOperator::LESS_THAN, true); // Prerelease < no prerelease
    testVersionComparison("1.0.0", "1.0.0", FilterOperator::LESS_THAN, false);
}

TEST_F(FilterExpressionAdvancedTest, EvaluateVersionGreaterThanOrEqual) {
    testVersionComparison("1.0.0", "1.0.0", FilterOperator::GREATER_THAN_OR_EQUAL, true);
    testVersionComparison("1.0.1", "1.0.0", FilterOperator::GREATER_THAN_OR_EQUAL, true);
    testVersionComparison("1.0.0", "1.0.1", FilterOperator::GREATER_THAN_OR_EQUAL, false);
}

TEST_F(FilterExpressionAdvancedTest, EvaluateVersionLessThanOrEqual) {
    testVersionComparison("1.0.0", "1.0.0", FilterOperator::LESS_THAN_OR_EQUAL, true);
    testVersionComparison("1.0.0", "1.0.1", FilterOperator::LESS_THAN_OR_EQUAL, true);
    testVersionComparison("1.0.1", "1.0.0", FilterOperator::LESS_THAN_OR_EQUAL, false);
}

TEST_F(FilterExpressionAdvancedTest, EvaluateVersionPrereleasePrecedence) {
    testVersionComparison("1.0.0-alpha.1", "1.0.0-alpha", FilterOperator::LESS_THAN, false); // alpha.1 is higher precedence than alpha? SemVer says alpha is lower.
    testVersionComparison("1.0.0-alpha", "1.0.0-alpha.1", FilterOperator::LESS_THAN, true);
    testVersionComparison("1.0.0-beta", "1.0.0-alpha", FilterOperator::GREATER_THAN, true);
    testVersionComparison("1.0.0-rc.1", "1.0.0-beta.2", FilterOperator::GREATER_THAN, true);
    testVersionComparison("1.0.0-alpha.0.0.1", "1.0.0-alpha.0.0", FilterOperator::GREATER_THAN, true); // Longer numeric prerelease wins if equal otherwise
    testVersionComparison("1.0.0-alpha.0.0", "1.0.0-alpha.0.0.1", FilterOperator::LESS_THAN, true);

}
TEST_F(FilterExpressionAdvancedTest, EvaluateVersionInvalidInput) {
    testVersionComparison("not-a-version", "1.0.0", FilterOperator::EQUALS, false);
    testVersionComparison("1.0.0", "not-a-version", FilterOperator::EQUALS, false);
    testVersionComparison("invalid.version.string", "1.0.0", FilterOperator::GREATER_THAN, false);
}

// --- IP Address Tests ---
TEST_F(FilterExpressionAdvancedTest, EvaluateIpAddressEquals) {
    testIpComparison("192.168.1.1", "192.168.1.1", FilterOperator::EQUALS, true);
    testIpComparison("192.168.1.1", "192.168.1.2", FilterOperator::EQUALS, false);
    testIpComparison("::1", "::1", FilterOperator::EQUALS, true);
    testIpComparison("::1", "::2", FilterOperator::EQUALS, false);
    testIpComparison("192.168.1.1", "::1", FilterOperator::EQUALS, false); // Different families
}

TEST_F(FilterExpressionAdvancedTest, EvaluateIpAddressNotEquals) {
    testIpComparison("192.168.1.1", "192.168.1.2", FilterOperator::NOT_EQUALS, true);
    testIpComparison("192.168.1.1", "192.168.1.1", FilterOperator::NOT_EQUALS, false);
}

TEST_F(FilterExpressionAdvancedTest, EvaluateIpAddressGreaterThan) {
    testIpComparison("192.168.1.2", "192.168.1.1", FilterOperator::GREATER_THAN, true);
    testIpComparison("10.0.0.1", "192.168.1.1", FilterOperator::GREATER_THAN, false); // 10.0.0.1 < 192.168.1.1
    testIpComparison("::2", "::1", FilterOperator::GREATER_THAN, true);
    testIpComparison("2001:0db8::1", "::1", FilterOperator::GREATER_THAN, true);
    testIpComparison("192.168.1.1", "::1", FilterOperator::GREATER_THAN, false); // IPv4 less than IPv6 by arbitrary rule
}

TEST_F(FilterExpressionAdvancedTest, EvaluateIpAddressLessThan) {
    testIpComparison("192.168.1.1", "192.168.1.2", FilterOperator::LESS_THAN, true);
    testIpComparison("::1", "::2", FilterOperator::LESS_THAN, true);
    testIpComparison("::1", "2001:0db8::1", FilterOperator::LESS_THAN, true);
    testIpComparison("::1", "192.168.1.1", FilterOperator::LESS_THAN, true); // IPv6 greater than IPv4 by arbitrary rule
}

TEST_F(FilterExpressionAdvancedTest, EvaluateIpAddressInvalidInput) {
    testIpComparison("invalid-ip", "192.168.1.1", FilterOperator::EQUALS, false);
    testIpComparison("192.168.1.1", "invalid-ip", FilterOperator::EQUALS, false);
    testIpComparison("256.0.0.1", "1.0.0.0", FilterOperator::GREATER_THAN, false); // Invalid IPv4
}

// --- FilterOperator::IN and FilterOperator::NOT_IN Edge Cases ---
TEST_F(FilterExpressionAdvancedTest, EvaluateInOperatorInvalidJson) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Message");
    FilterCondition cond = createCondition(LogEntryField::MESSAGE, FilterOperator::IN, "not-json", FilterValueType::STRING);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry));
}

TEST_F(FilterExpressionAdvancedTest, EvaluateInOperatorNonArrayJson) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Message");
    FilterCondition cond = createCondition(LogEntryField::MESSAGE, FilterOperator::IN, "\"single_string\"", FilterValueType::STRING);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry));
    
    cond = createCondition(LogEntryField::MESSAGE, FilterOperator::IN, "{\"key\": \"value\"}", FilterValueType::STRING);
    expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry));
}

TEST_F(FilterExpressionAdvancedTest, EvaluateInOperatorNonStringItems) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "test");
    // "test" is not in ["abc", 123, true]
    FilterCondition cond = createCondition(LogEntryField::MESSAGE, FilterOperator::IN, "[\"abc\", 123, true]", FilterValueType::STRING);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry));

    // "abc" is in ["abc", 123, true]
    LogEntry entry2 = createLogEntry(LogLevel::INFO, "abc");
    EXPECT_TRUE(expr.evaluate(entry2));

    // "123" (as string) is not in ["abc", 123, true] because it only compares string items
    LogEntry entry3 = createLogEntry(LogLevel::INFO, "123");
    EXPECT_FALSE(expr.evaluate(entry3));
}

TEST_F(FilterExpressionAdvancedTest, EvaluateInOperatorEmptyArray) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "test");
    FilterCondition cond = createCondition(LogEntryField::MESSAGE, FilterOperator::IN, "[]", FilterValueType::STRING);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry));
}

TEST_F(FilterExpressionAdvancedTest, EvaluateNotInOperatorEmptyArray) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "test");
    FilterCondition cond = createCondition(LogEntryField::MESSAGE, FilterOperator::NOT_IN, "[]", FilterValueType::STRING);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry)); // Not in an empty set is always true
}

// --- Error Handling for Numeric and Regex Conversions ---
TEST_F(FilterExpressionAdvancedTest, EvaluateNumericInvalidInput) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Test", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, {{"value", "abc"}});
    FilterCondition cond = createCondition(LogEntryField::CUSTOM, FilterOperator::EQUALS, "123", FilterValueType::INT, true, "value");
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry)); // "abc" cannot be converted to INT

    cond = createCondition(LogEntryField::CUSTOM, FilterOperator::EQUALS, "not-a-num", FilterValueType::INT, true, "value");
    expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry)); // "not-a-num" cannot be converted to INT
}

TEST_F(FilterExpressionAdvancedTest, EvaluateDoubleInvalidInput) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Test", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, {{"value", "3.14a"}});
    FilterCondition cond = createCondition(LogEntryField::CUSTOM, FilterOperator::EQUALS, "3.14", FilterValueType::DOUBLE, true, "value");
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry)); // "3.14a" cannot be converted to DOUBLE
}

TEST_F(FilterExpressionAdvancedTest, EvaluateRegexInvalidPattern) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Some message");
    FilterCondition cond = createCondition(LogEntryField::MESSAGE, FilterOperator::REGEX_MATCH, "[invalid regex", FilterValueType::STRING);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry)); // Invalid regex should not crash and return false
}

// --- LogEntryField::ID and LINE_NUMBER with Optional Values ---
TEST_F(FilterExpressionAdvancedTest, EvaluateIdOptionalPresent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg", 100);
    FilterCondition cond = createCondition(LogEntryField::ID, FilterOperator::EQUALS, "100", FilterValueType::INT);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry));
}

TEST_F(FilterExpressionAdvancedTest, EvaluateIdOptionalAbsent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg", std::nullopt);
    FilterCondition cond = createCondition(LogEntryField::ID, FilterOperator::IS_ABSENT, "", FilterValueType::STRING);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry));
    
    cond = createCondition(LogEntryField::ID, FilterOperator::IS_PRESENT, "", FilterValueType::STRING);
    expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry));
}

TEST_F(FilterExpressionAdvancedTest, EvaluateLineNumberOptionalPresent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg", std::nullopt, 50);
    FilterCondition cond = createCondition(LogEntryField::LINE_NUMBER, FilterOperator::EQUALS, "50", FilterValueType::INT);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry));
}

TEST_F(FilterExpressionAdvancedTest, EvaluateLineNumberOptionalAbsent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg", std::nullopt, std::nullopt);
    FilterCondition cond = createCondition(LogEntryField::LINE_NUMBER, FilterOperator::IS_ABSENT, "", FilterValueType::STRING);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry));
    
    cond = createCondition(LogEntryField::LINE_NUMBER, FilterOperator::IS_PRESENT, "", FilterValueType::STRING);
    expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry));
}

// --- LogEntryField::THREAD_ID, MODULE, HOST (now direct members) ---
TEST_F(FilterExpressionAdvancedTest, EvaluateThreadIdOptionalPresent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg", std::nullopt, std::nullopt, "thread-123");
    FilterCondition cond = createCondition(LogEntryField::THREAD_ID, FilterOperator::EQUALS, "thread-123");
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry));
}

TEST_F(FilterExpressionAdvancedTest, EvaluateThreadIdOptionalAbsent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg");
    FilterCondition cond = createCondition(LogEntryField::THREAD_ID, FilterOperator::IS_ABSENT, "");
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry));
}

TEST_F(FilterExpressionAdvancedTest, EvaluateModuleOptionalPresent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg", std::nullopt, std::nullopt, std::nullopt, "Analytics");
    FilterCondition cond = createCondition(LogEntryField::MODULE, FilterOperator::EQUALS, "Analytics");
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry));
}

TEST_F(FilterExpressionAdvancedTest, EvaluateModuleOptionalAbsent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg");
    FilterCondition cond = createCondition(LogEntryField::MODULE, FilterOperator::IS_ABSENT, "");
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry));
}

TEST_F(FilterExpressionAdvancedTest, EvaluateHostOptionalPresent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg", std::nullopt, std::nullopt, std::nullopt, std::nullopt, "server-prod-01");
    FilterCondition cond = createCondition(LogEntryField::HOST, FilterOperator::EQUALS, "server-prod-01");
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry));
}

TEST_F(FilterExpressionAdvancedTest, EvaluateHostOptionalAbsent) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg");
    FilterCondition cond = createCondition(LogEntryField::HOST, FilterOperator::IS_ABSENT, "");
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_TRUE(expr.evaluate(entry));
}

// --- FilterValueType::DATETIME Edge Cases ---
TEST_F(FilterExpressionAdvancedTest, EvaluateDateTimeInvalidInput) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, Utils::parseTime("2023-01-01 10:00:00").value());
    FilterCondition cond = createCondition(LogEntryField::TIMESTAMP, FilterOperator::EQUALS, "not-a-date", FilterValueType::DATETIME);
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry));

    cond = createCondition(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-invalid-date", FilterValueType::DATETIME);
    expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry));
}

TEST_F(FilterExpressionAdvancedTest, EvaluateDateTimeDifferentFormats) {
    // Assuming Utils::parseTime can handle these. If not, this test should fail or be adjusted.
    auto t1 = Utils::parseTime("2023-01-01 10:00:00").value();
    auto t2 = Utils::parseTime("2023/01/01 10:00:00").value(); // Assuming this format is handled
    
    LogEntry entry1 = createLogEntry(LogLevel::INFO, "Msg", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, t1);
    FilterCondition cond = createCondition(LogEntryField::TIMESTAMP, FilterOperator::EQUALS, "2023/01/01 10:00:00", FilterValueType::DATETIME);
    FilterExpression expr = FilterExpression::create(cond);
    // This will pass if both formats parse to the same time_point
    EXPECT_TRUE(expr.evaluate(entry1)); 
}

// --- stringToBool Helper Function ---
TEST_F(FilterExpressionAdvancedTest, StringToBoolValidCases) {
    LogEntry entry_true = createLogEntry(LogLevel::INFO, "true", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, {{"bool_val", "true"}});
    LogEntry entry_TRUE = createLogEntry(LogLevel::INFO, "TRUE", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, {{"bool_val", "TRUE"}});
    LogEntry entry_1 = createLogEntry(LogLevel::INFO, "1", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, {{"bool_val", "1"}});
    LogEntry entry_false = createLogEntry(LogLevel::INFO, "false", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, {{"bool_val", "false"}});
    LogEntry entry_FALSE = createLogEntry(LogLevel::INFO, "FALSE", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, {{"bool_val", "FALSE"}});
    LogEntry entry_0 = createLogEntry(LogLevel::INFO, "0", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, {{"bool_val", "0"}});

    FilterCondition cond_true = createCondition(LogEntryField::CUSTOM, FilterOperator::EQUALS, "true", FilterValueType::BOOL, true, "bool_val");
    FilterExpression expr_true = FilterExpression::create(cond_true);
    EXPECT_TRUE(expr_true.evaluate(entry_true));
    EXPECT_TRUE(expr_true.evaluate(entry_TRUE));
    EXPECT_TRUE(expr_true.evaluate(entry_1));
    EXPECT_FALSE(expr_true.evaluate(entry_false));

    FilterCondition cond_false = createCondition(LogEntryField::CUSTOM, FilterOperator::EQUALS, "false", FilterValueType::BOOL, true, "bool_val");
    FilterExpression expr_false = FilterExpression::create(cond_false);
    EXPECT_TRUE(expr_false.evaluate(entry_false));
    EXPECT_TRUE(expr_false.evaluate(entry_FALSE));
    EXPECT_TRUE(expr_false.evaluate(entry_0));
    EXPECT_FALSE(expr_false.evaluate(entry_true));
}

TEST_F(FilterExpressionAdvancedTest, StringToBoolInvalidCases) {
    LogEntry entry_invalid = createLogEntry(LogLevel::INFO, "maybe", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, {{"bool_val", "maybe"}});
    FilterCondition cond_true = createCondition(LogEntryField::CUSTOM, FilterOperator::EQUALS, "true", FilterValueType::BOOL, true, "bool_val");
    FilterExpression expr_true = FilterExpression::create(cond_true);
    EXPECT_FALSE(expr_true.evaluate(entry_invalid)); // Invalid bool string should not match
}

// --- Fluent API - Complex Combinations and Negation ---
TEST_F(FilterExpressionAdvancedTest, EvaluateComplexLogicalCombination) {
    // (LEVEL == INFO AND MESSAGE CONTAINS "user") OR (HOST == "backend" AND NOT (MODULE == "auth"))
    LogEntry entry1 = createLogEntry(LogLevel::INFO, "User 'john' logged in", std::nullopt, std::nullopt, std::nullopt, "web", "frontend"); // Matches only 1st part
    LogEntry entry2 = createLogEntry(LogLevel::DEBUG, "Request to backend", std::nullopt, std::nullopt, std::nullopt, "api", "backend"); // Matches 2nd part (HOST=backend AND NOT MODULE=auth)
    LogEntry entry3 = createLogEntry(LogLevel::ERROR, "Auth failed", std::nullopt, std::nullopt, std::nullopt, "auth", "backend"); // Doesn't match 2nd part (HOST=backend but MODULE=auth)
    LogEntry entry4 = createLogEntry(LogLevel::WARNING, "Disk full", std::nullopt, std::nullopt, std::nullopt, "monitor", "local"); // No match

    auto cond_info = createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO");
    auto cond_user_msg = createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "user");
    auto cond_host_backend = createCondition(LogEntryField::HOST, FilterOperator::EQUALS, "backend");
    auto cond_module_auth = createCondition(LogEntryField::MODULE, FilterOperator::EQUALS, "auth");

    FilterExpression expr = (FilterExpression::create(cond_info).And(FilterExpression::create(cond_user_msg)))
                            .Or(FilterExpression::create(cond_host_backend).And(FilterExpression::create(cond_module_auth).Not()));

    EXPECT_TRUE(expr.evaluate(entry1));
    EXPECT_TRUE(expr.evaluate(entry2));
    EXPECT_FALSE(expr.evaluate(entry3));
    EXPECT_FALSE(expr.evaluate(entry4));
}

TEST_F(FilterExpressionAdvancedTest, EvaluateNegationOfLogicalExpression) {
    // NOT(LEVEL == INFO OR LEVEL == DEBUG)
    LogEntry entry_info = createLogEntry(LogLevel::INFO, "Log");
    LogEntry entry_debug = createLogEntry(LogLevel::DEBUG, "Log");
    LogEntry entry_warn = createLogEntry(LogLevel::WARNING, "Log");

    auto cond_info = createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO");
    auto cond_debug = createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "DEBUG");

    FilterExpression expr = (FilterExpression::create(cond_info).Or(FilterExpression::create(cond_debug))).Not();

    EXPECT_FALSE(expr.evaluate(entry_info));
    EXPECT_FALSE(expr.evaluate(entry_debug));
    EXPECT_TRUE(expr.evaluate(entry_warn));
}

// --- FilterExpression::EMPTY Type ---
TEST_F(FilterExpressionAdvancedTest, EvaluateEmptyExpression) {
    FilterExpression empty_expr; // Default constructor creates an EMPTY expression
    LogEntry any_entry = createLogEntry(LogLevel::INFO, "Any message");
    EXPECT_TRUE(empty_expr.evaluate(any_entry));
}

// --- Comprehensive String Operator Tests ---
TEST_F(FilterExpressionAdvancedTest, EvaluateStringStartsEndsWithEdgeCases) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "HelloWorld");

    // STARTS_WITH
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "Hello")).evaluate(entry));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "hello", FilterValueType::STRING, true)).evaluate(entry));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "hello", FilterValueType::STRING, false)).evaluate(entry));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH_I, "hello")).evaluate(entry));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "HelloWorld")).evaluate(entry));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "HellWorld")).evaluate(entry)); // Longer prefix
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "")).evaluate(entry)); // Empty prefix
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH_I, "")).evaluate(entry)); // Empty prefix
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::STARTS_WITH, "LongerThanMessage")).evaluate(entry));

    // ENDS_WITH
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "World")).evaluate(entry));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "world", FilterValueType::STRING, true)).evaluate(entry));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "world", FilterValueType::STRING, false)).evaluate(entry));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH_I, "world")).evaluate(entry));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "HelloWorld")).evaluate(entry));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "HelloWord")).evaluate(entry)); // Longer suffix
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "")).evaluate(entry)); // Empty suffix
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH_I, "")).evaluate(entry)); // Empty suffix
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::ENDS_WITH, "LongerThanMessage")).evaluate(entry));
}

TEST_F(FilterExpressionAdvancedTest, EvaluateStringContainsNotContainsEdgeCases) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Hello World Hello");

    // CONTAINS
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "World")).evaluate(entry));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "world", FilterValueType::STRING, true)).evaluate(entry));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "world", FilterValueType::STRING, false)).evaluate(entry));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS_I, "world")).evaluate(entry));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "Hello")).evaluate(entry));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "")).evaluate(entry)); // Empty pattern matches
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS_I, "")).evaluate(entry)); // Empty pattern matches

    // NOT_CONTAINS
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::NOT_CONTAINS, "World")).evaluate(entry));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::NOT_CONTAINS, "world", FilterValueType::STRING, true)).evaluate(entry));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::NOT_CONTAINS, "world", FilterValueType::STRING, false)).evaluate(entry));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::NOT_CONTAINS_I, "world")).evaluate(entry));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::NOT_CONTAINS, "")).evaluate(entry)); // Empty pattern does not match (is contained)
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::NOT_CONTAINS_I, "")).evaluate(entry)); // Empty pattern does not match (is contained)
}

// --- Numeric Operator Boundary Tests ---
TEST_F(FilterExpressionAdvancedTest, EvaluateNumericEqualityBoundary) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Msg", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, {{"num_val", "10"}});

    FilterCondition cond_eq = createCondition(LogEntryField::CUSTOM, FilterOperator::EQUALS, "10", FilterValueType::INT, true, "num_val");
    EXPECT_TRUE(FilterExpression::create(cond_eq).evaluate(entry));

    FilterCondition cond_ge = createCondition(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN_OR_EQUAL, "10", FilterValueType::INT, true, "num_val");
    EXPECT_TRUE(FilterExpression::create(cond_ge).evaluate(entry));

    FilterCondition cond_le = createCondition(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "10", FilterValueType::INT, true, "num_val");
    EXPECT_TRUE(FilterExpression::create(cond_le).evaluate(entry));

    FilterCondition cond_gt = createCondition(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "10", FilterValueType::INT, true, "num_val");
    EXPECT_FALSE(FilterExpression::create(cond_gt).evaluate(entry));

    FilterCondition cond_lt = createCondition(LogEntryField::CUSTOM, FilterOperator::LESS_THAN, "10", FilterValueType::INT, true, "num_val");
    EXPECT_FALSE(FilterExpression::create(cond_lt).evaluate(entry));
}

// --- IS_PRESENT and IS_ABSENT ---
TEST_F(FilterExpressionAdvancedTest, EvaluateIsPresentAbsentAllFieldTypes) {
    // LogEntry with some fields present, some absent
    LogEntry entry_partial = createLogEntry(
        LogLevel::INFO, "Partial log", 
        123,                // ID present
        std::nullopt,       // Line Number absent
        "thread_A",         // Thread ID present
        std::nullopt,       // Module absent
        "host_X",           // Host present
        Utils::parseTime("2023-01-01 12:00:00").has_value() ? std::make_optional(Utils::parseTime("2023-01-01 12:00:00").value()) : std::nullopt,
        {{"custom_field_present", "value"}, {"custom_field_empty", ""}} // Custom fields
    );

    // ID
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::ID, FilterOperator::IS_PRESENT, "")).evaluate(entry_partial));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::ID, FilterOperator::IS_ABSENT, "")).evaluate(entry_partial));

    // LINE_NUMBER
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::LINE_NUMBER, FilterOperator::IS_PRESENT, "")).evaluate(entry_partial));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::LINE_NUMBER, FilterOperator::IS_ABSENT, "")).evaluate(entry_partial));

    // THREAD_ID
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::THREAD_ID, FilterOperator::IS_PRESENT, "")).evaluate(entry_partial));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::THREAD_ID, FilterOperator::IS_ABSENT, "")).evaluate(entry_partial));

    // MODULE
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MODULE, FilterOperator::IS_PRESENT, "")).evaluate(entry_partial));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MODULE, FilterOperator::IS_ABSENT, "")).evaluate(entry_partial));

    // HOST
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::HOST, FilterOperator::IS_PRESENT, "")).evaluate(entry_partial));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::HOST, FilterOperator::IS_ABSENT, "")).evaluate(entry_partial));

    // TIMESTAMP
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::TIMESTAMP, FilterOperator::IS_PRESENT, "")).evaluate(entry_partial));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::TIMESTAMP, FilterOperator::IS_ABSENT, "")).evaluate(entry_partial));
    
    // LEVEL (always present)
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::LEVEL, FilterOperator::IS_PRESENT, "")).evaluate(entry_partial));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::LEVEL, FilterOperator::IS_ABSENT, "")).evaluate(entry_partial));

    // MESSAGE (always present)
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::IS_PRESENT, "")).evaluate(entry_partial));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::MESSAGE, FilterOperator::IS_ABSENT, "")).evaluate(entry_partial));

    // CUSTOM field (present with value)
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::CUSTOM, FilterOperator::IS_PRESENT, "", FilterValueType::STRING, true, "custom_field_present")).evaluate(entry_partial));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::CUSTOM, FilterOperator::IS_ABSENT, "", FilterValueType::STRING, true, "custom_field_present")).evaluate(entry_partial));

    // CUSTOM field (present with empty string value, but still considered 'present')
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::CUSTOM, FilterOperator::IS_PRESENT, "", FilterValueType::STRING, true, "custom_field_empty")).evaluate(entry_partial));
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::CUSTOM, FilterOperator::IS_ABSENT, "", FilterValueType::STRING, true, "custom_field_empty")).evaluate(entry_partial));

    // CUSTOM field (not present at all)
    EXPECT_FALSE(FilterExpression::create(createCondition(LogEntryField::CUSTOM, FilterOperator::IS_PRESENT, "", FilterValueType::STRING, true, "custom_field_absent")).evaluate(entry_partial));
    EXPECT_TRUE(FilterExpression::create(createCondition(LogEntryField::CUSTOM, FilterOperator::IS_ABSENT, "", FilterValueType::STRING, true, "custom_field_absent")).evaluate(entry_partial));
}
