#include <gtest/gtest.h>
#include "../include/Filter.h"
#include "../include/LogTypes.h"
#include <chrono>
#include <map>
#include <sstream> // Required for std::stringstream and std::get_time
#include <nlohmann/json.hpp> // New include for JSON testing

// Test fixture for creating LogEntry objects
class FilterTest : public ::testing::Test {
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

// New test fixture for JSON serialization/deserialization tests
class FilterJsonTest : public ::testing::Test {
protected:
    // Utility to create a FilterCondition
    FilterCondition createFilterCondition(
        LogEntryField field,
        FilterOperator op,
        const std::string& value,
        FilterValueType valueType = FilterValueType::STRING,
        bool caseSensitive = false,
        std::optional<std::string> datetimeFormat = std::nullopt
    ) {
        FilterCondition fc;
        fc.field = field;
        fc.op = op;
        fc.value = value;
        fc.valueType = valueType;
        fc.caseSensitive = caseSensitive;
        fc.datetimeFormat = datetimeFormat;
        return fc;
    }
};

TEST_F(FilterTest, SourceFileFilterLiteral) {
    SourceFileFilter filter("server.log", PatternType::Literal, true);
    auto entry = createLogEntry(1, "server.log", now, LogLevel::INFO, "Server started");
    EXPECT_TRUE(filter.matches(entry));

    auto entry_wrong_case = createLogEntry(2, "Server.log", now, LogLevel::INFO, "Server started");
    EXPECT_FALSE(filter.matches(entry_wrong_case));

    SourceFileFilter filter_case_insensitive("server.log", PatternType::Literal, false);
    EXPECT_TRUE(filter_case_insensitive.matches(entry_wrong_case));
}

TEST_F(FilterTest, SourceFileFilterGlob) {
    SourceFileFilter filter("*.log", PatternType::Wildcard);
    auto entry1 = createLogEntry(1, "server.log", now, LogLevel::INFO, "Server started");
    auto entry2 = createLogEntry(2, "client.log", now, LogLevel::INFO, "Client started");
    auto entry3 = createLogEntry(3, "server.txt", now, LogLevel::INFO, "Server config");
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_TRUE(filter.matches(entry2));
    EXPECT_FALSE(filter.matches(entry3));

    SourceFileFilter filter2("server??.log", PatternType::Wildcard);
    auto entry4 = createLogEntry(4, "server01.log", now, LogLevel::INFO, "Server 01");
    auto entry5 = createLogEntry(5, "server.log", now, LogLevel::INFO, "Server");
    EXPECT_TRUE(filter2.matches(entry4));
    EXPECT_FALSE(filter2.matches(entry5));
}

TEST_F(FilterTest, SourceFileFilterGlobSubstringMatch) {
    SourceFileFilter filter("server*", PatternType::Wildcard); // Glob pattern matching "server" as a substring
    auto entry1 = createLogEntry(1, "my-server-instance.log", now, LogLevel::INFO, "Server started");
    auto entry2 = createLogEntry(2, "server.log", now, LogLevel::INFO, "Server log");
    auto entry3 = createLogEntry(3, "another_log.txt", now, LogLevel::INFO, "No match");
    auto entry4 = createLogEntry(4, "log-from-server.log", now, LogLevel::INFO, "Log from server");

    EXPECT_TRUE(filter.matches(entry1)); // Should match "server" as substring
    EXPECT_TRUE(filter.matches(entry2)); // Should match "server" as prefix
    EXPECT_FALSE(filter.matches(entry3));
    EXPECT_TRUE(filter.matches(entry4)); // Should match "server" as substring

    // Test a more specific substring glob
    SourceFileFilter filter2("*server*", PatternType::Wildcard);
    EXPECT_TRUE(filter2.matches(entry1));
    EXPECT_TRUE(filter2.matches(entry2));
    EXPECT_FALSE(filter2.matches(entry3));
    EXPECT_TRUE(filter2.matches(entry4));
}

TEST_F(FilterTest, SourceFileFilterRegex) {
    SourceFileFilter filter("server.*\\.log", PatternType::Regex); // Matches server anything .log
    auto entry1 = createLogEntry(1, "server-alpha.log", now, LogLevel::INFO, "Server started");
    auto entry2 = createLogEntry(2, "production.server.log", now, LogLevel::INFO, "Production server log");
    auto entry3 = createLogEntry(3, "server_log", now, LogLevel::INFO, "No match");
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_TRUE(filter.matches(entry2));
    EXPECT_FALSE(filter.matches(entry3));

    SourceFileFilter filter_icase("SERVER.*\\.LOG", PatternType::Regex, false);
    auto entry4 = createLogEntry(4, "Server-Alpha.log", now, LogLevel::INFO, "Server started");
    EXPECT_TRUE(filter_icase.matches(entry4));
}

TEST_F(FilterTest, FieldExistsFilter) {
    FieldExistsFilter filter("user_id");
    auto entry1 = createLogEntry(1, "app.log", now, LogLevel::INFO, "User logged in", {{"user_id", "123"}, {"session", "xyz"}});
    auto entry2 = createLogEntry(2, "app.log", now, LogLevel::WARNING, "User not found", {{"session", "abc"}});
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2));
}

TEST_F(FilterTest, FieldValueFilter) {
    FieldValueFilter filter("status_code", "404", PatternType::Literal);
    auto entry1 = createLogEntry(1, "web.log", now, LogLevel::WARNING, "Not found", {{"status_code", "404"}});
    auto entry2 = createLogEntry(2, "web.log", now, LogLevel::INFO, "OK", {{"status_code", "200"}});
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2));

    FieldValueFilter filter_icase("status_code", "NotFound", PatternType::Literal, false);
    auto entry3 = createLogEntry(3, "web.log", now, LogLevel::WARNING, "Not found", {{"status_code", "notfound"}});
    EXPECT_TRUE(filter_icase.matches(entry3));
}


TEST_F(FilterTest, FieldValueFilterWildcardSubstringMatch) {
    FieldValueFilter filter("user_id", "admin*", PatternType::Wildcard);
    auto entry1 = createLogEntry(1, "app.log", now, LogLevel::INFO, "msg", {{"user_id", "admin123"}});
    auto entry2 = createLogEntry(2, "app.log", now, LogLevel::INFO, "msg", {{"user_id", "superuser"}});
    auto entry3 = createLogEntry(3, "app.log", now, LogLevel::INFO, "msg", {{"user_id", "guest"}});
    auto entry4 = createLogEntry(4, "app.log", now, LogLevel::INFO, "msg", {{"user_id", "system-admin"}}); // Should match "admin" as substring

    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2));
    EXPECT_FALSE(filter.matches(entry3));
    EXPECT_TRUE(filter.matches(entry4));
}

TEST_F(FilterTest, FieldValueFilterRegex) {
    // Default caseSensitive is false, so it should be case-insensitive
    FieldValueFilter filter_regex("error_code", R"(ERR\\d{3})", PatternType::Regex);
    auto entry3 = createLogEntry(3, "app.log", now, LogLevel::ERROR, "DB error", {{"error_code", "ERR501"}});
    auto entry4 = createLogEntry(4, "app.log", now, LogLevel::ERROR, "Network error", {{"error_code", "err200"}});
    EXPECT_TRUE(filter_regex.matches(entry3)); // ERR501 matches ERR\\d{3} case-insensitively
    EXPECT_TRUE(filter_regex.matches(entry4)); // err200 matches ERR\\d{3} case-insensitively

    FieldValueFilter filter_regex_icase_explicit("error_code", R"(ERR\\d{3})", PatternType::Regex, false);
    EXPECT_TRUE(filter_regex_icase_explicit.matches(entry3)); // ERR501 matches ERR\\d{3} case-insensitively
    EXPECT_TRUE(filter_regex_icase_explicit.matches(entry4)); // err200 matches ERR\\d{3} case-insensitively
}

TEST_F(FilterTest, FieldValueFilterRegexCaseSensitive) {
    FieldValueFilter filter_regex_cs("error_code", R"(ERR\\d{3})", PatternType::Regex, true); // Explicitly case sensitive
    auto entry3 = createLogEntry(3, "app.log", now, LogLevel::ERROR, "DB error", {{"error_code", "ERR501"}});
    auto entry4 = createLogEntry(4, "app.log", now, LogLevel::ERROR, "Network error", {{"error_code", "err200"}});
    EXPECT_TRUE(filter_regex_cs.matches(entry3));
    EXPECT_FALSE(filter_regex_cs.matches(entry4)); // Should not match 'err'
}

TEST_F(FilterTest, PredicateFilter) {
    PredicateFilter filter([](const LogEntry& entry) {
        return entry.message.length() > 15;
    });
    auto entry1 = createLogEntry(1, "test.log", now, LogLevel::INFO, "This is a long message");
    auto entry2 = createLogEntry(2, "test.log", now, LogLevel::INFO, "Short msg");
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2));
}

TEST_F(FilterTest, LogLevelSetFilter) {
    LogLevelSetFilter filter({LogLevel::WARNING, LogLevel::ERROR, LogLevel::FATAL});
    auto entry1 = createLogEntry(1, "sys.log", now, LogLevel::WARNING, "Disk space low");
    auto entry2 = createLogEntry(2, "sys.log", now, LogLevel::INFO, "System nominal");
    auto entry3 = createLogEntry(3, "sys.log", now, LogLevel::FATAL, "System shutting down");
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2));
    EXPECT_TRUE(filter.matches(entry3));
}

TEST_F(FilterTest, LevelFilterTest) {
    LevelFilter filter(LogLevel::INFO);
    auto entry_info = createLogEntry(1, "app.log", now, LogLevel::INFO, "Info message");
    auto entry_debug = createLogEntry(2, "app.log", now, LogLevel::DEBUG, "Debug message");
    auto entry_warning = createLogEntry(3, "app.log", now, LogLevel::WARNING, "Warning message");

    EXPECT_TRUE(filter.matches(entry_info));
    EXPECT_FALSE(filter.matches(entry_debug));
    EXPECT_FALSE(filter.matches(entry_warning));
}

TEST_F(FilterTest, MinLevelFilterTest) {
    MinLevelFilter filter(LogLevel::WARNING);
    auto entry_fatal = createLogEntry(1, "app.log", now, LogLevel::FATAL, "Fatal error");
    auto entry_error = createLogEntry(2, "app.log", now, LogLevel::ERROR, "Error occurred");
    auto entry_warning = createLogEntry(3, "app.log", now, LogLevel::WARNING, "Warning message");
    auto entry_info = createLogEntry(4, "app.log", now, LogLevel::INFO, "Info message");
    auto entry_debug = createLogEntry(5, "app.log", now, LogLevel::DEBUG, "Debug message");

    EXPECT_TRUE(filter.matches(entry_fatal));
    EXPECT_TRUE(filter.matches(entry_error));
    EXPECT_TRUE(filter.matches(entry_warning));
    EXPECT_FALSE(filter.matches(entry_info));
    EXPECT_FALSE(filter.matches(entry_debug));
}

TEST_F(FilterTest, BoolFilterTest) {
    BoolFilter filter_true("flag", true);
    EXPECT_TRUE(filter_true.matches(createLogEntry(1, "log", now, LogLevel::INFO, "msg", {{"flag", "true"}})));
    EXPECT_TRUE(filter_true.matches(createLogEntry(2, "log", now, LogLevel::INFO, "msg", {{"flag", "TRUE"}})));
    EXPECT_TRUE(filter_true.matches(createLogEntry(3, "log", now, LogLevel::INFO, "msg", {{"flag", "1"}})));
    EXPECT_TRUE(filter_true.matches(createLogEntry(4, "log", now, LogLevel::INFO, "msg", {{"flag", "yes"}})));
    EXPECT_FALSE(filter_true.matches(createLogEntry(5, "log", now, LogLevel::INFO, "msg", {{"flag", "false"}})));
    EXPECT_FALSE(filter_true.matches(createLogEntry(6, "log", now, LogLevel::INFO, "msg", {{"flag", "0"}})));
    EXPECT_FALSE(filter_true.matches(createLogEntry(7, "log", now, LogLevel::INFO, "msg", {{"flag", "no"}})));
    EXPECT_FALSE(filter_true.matches(createLogEntry(8, "log", now, LogLevel::INFO, "msg", {{"flag", "other"}})));
    EXPECT_FALSE(filter_true.matches(createLogEntry(9, "log", now, LogLevel::INFO, "msg", {{"other_flag", "true"}})));

    BoolFilter filter_false("flag", false);
    EXPECT_TRUE(filter_false.matches(createLogEntry(10, "log", now, LogLevel::INFO, "msg", {{"flag", "false"}})));
    EXPECT_TRUE(filter_false.matches(createLogEntry(11, "log", now, LogLevel::INFO, "msg", {{"flag", "FALSE"}})));
    EXPECT_TRUE(filter_false.matches(createLogEntry(12, "log", now, LogLevel::INFO, "msg", {{"flag", "0"}})));
    EXPECT_TRUE(filter_false.matches(createLogEntry(13, "log", now, LogLevel::INFO, "msg", {{"flag", "no"}})));
    EXPECT_FALSE(filter_false.matches(createLogEntry(14, "log", now, LogLevel::INFO, "msg", {{"flag", "true"}})));
    EXPECT_FALSE(filter_false.matches(createLogEntry(15, "log", now, LogLevel::INFO, "msg", {{"flag", "1"}})));
    EXPECT_FALSE(filter_false.matches(createLogEntry(16, "log", now, LogLevel::INFO, "msg", {{"flag", "yes"}})));
}

TEST_F(FilterTest, KeywordFilterSingle) {
    KeywordFilter filter("error", true);
    auto entry1 = createLogEntry(1, "app.log", now, LogLevel::INFO, "An error occurred");
    auto entry2 = createLogEntry(2, "app.log", now, LogLevel::INFO, "All good");
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2));
}

TEST_F(FilterTest, KeywordFilterSingleCaseInsensitive) {
    KeywordFilter filter("Error", false);
    auto entry1 = createLogEntry(1, "app.log", now, LogLevel::INFO, "An error occurred");
    auto entry2 = createLogEntry(2, "app.log", now, LogLevel::INFO, "ERROR");
    auto entry3 = createLogEntry(3, "app.log", now, LogLevel::INFO, "No issues here");
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_TRUE(filter.matches(entry2));
    EXPECT_FALSE(filter.matches(entry3));
}

TEST_F(FilterTest, KeywordFilterMultiAny) {
    KeywordFilter filter({"error", "failed"}, KeywordFilter::Logic::ANY);
    auto entry1 = createLogEntry(1, "app.log", now, LogLevel::INFO, "Request failed");
    auto entry2 = createLogEntry(2, "app.log", now, LogLevel::INFO, "An error was found");
    auto entry3 = createLogEntry(3, "app.log", now, LogLevel::INFO, "Success");
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_TRUE(filter.matches(entry2));
    EXPECT_FALSE(filter.matches(entry3));
}

TEST_F(FilterTest, KeywordFilterEmptyListAny) {
    KeywordFilter filter({}, KeywordFilter::Logic::ANY); // Empty list for ANY
    auto entry = createLogEntry(1, "app.log", now, LogLevel::INFO, "Any message");
    EXPECT_FALSE(filter.matches(entry)); // Empty ANY list should never match
}

TEST_F(FilterTest, KeywordFilterEmptyListAll) {
    KeywordFilter filter({}, KeywordFilter::Logic::ALL); // Empty list for ALL
    auto entry = createLogEntry(1, "app.log", now, LogLevel::INFO, "Any message");
    EXPECT_TRUE(filter.matches(entry)); // Empty ALL list should always match (vacuously true)
}

TEST_F(FilterTest, KeywordFilterMultiAll) {
    KeywordFilter filter({"database", "connection", "failed"}, KeywordFilter::Logic::ALL);
    auto entry1 = createLogEntry(1, "db.log", now, LogLevel::ERROR, "Database connection failed");
    auto entry2 = createLogEntry(2, "db.log", now, LogLevel::WARNING, "Database connection is slow");
    auto entry3 = createLogEntry(3, "db.log", now, LogLevel::ERROR, "Request failed");
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2));
    EXPECT_FALSE(filter.matches(entry3));
}

TEST_F(FilterTest, RegexFilterCaseSensitive) {
    auto filter_res_cs = RegexFilter::create("Error", true); // Using the new factory, case sensitive
    ASSERT_TRUE(filter_res_cs.has_value()) << filter_res_cs.error();
    auto filter_cs = filter_res_cs.value();

    auto entry1 = createLogEntry(1, "app.log", now, LogLevel::INFO, "error found");
    auto entry2 = createLogEntry(2, "app.log", now, LogLevel::INFO, "Error found");
    EXPECT_FALSE(filter_cs->matches(entry1));
    EXPECT_TRUE(filter_cs->matches(entry2));

    auto filter_res_icase = RegexFilter::create("[Ee]rror", false); // Using the new factory, case insensitive flag will be applied
    ASSERT_TRUE(filter_res_icase.has_value()) << filter_res_icase.error();
    auto filter_icase = filter_res_icase.value();

    EXPECT_TRUE(filter_icase->matches(entry1)); // Should match 'error'
    EXPECT_TRUE(filter_icase->matches(entry2)); // Should match 'Error'

    // Test with a complex regex and mixed case
    auto filter_complex_res = RegexFilter::create("^(warn|error|fatal).*$", false);
    ASSERT_TRUE(filter_complex_res.has_value()) << filter_complex_res.error();
    auto filter_complex = filter_complex_res.value();
    EXPECT_TRUE(filter_complex->matches(createLogEntry(3, "app.log", now, LogLevel::WARNING, "Warning: Something happened")));
    EXPECT_TRUE(filter_complex->matches(createLogEntry(4, "app.log", now, LogLevel::ERROR, "ERROR: Critical issue")));
    EXPECT_TRUE(filter_complex->matches(createLogEntry(5, "app.log", now, LogLevel::FATAL, "fatal: System down")));
    EXPECT_FALSE(filter_complex->matches(createLogEntry(6, "app.log", now, LogLevel::INFO, "info: All good")));
}

TEST_F(FilterTest, RegexFilterInvalidPattern) {
    auto filter_res = RegexFilter::create("["); // Invalid regex pattern
    EXPECT_FALSE(filter_res.has_value());
    EXPECT_EQ(filter_res.error().code, Code::InvalidRegex);
    EXPECT_NE(filter_res.error().message.find("The expression contained an invalid character class name"), std::string::npos);
}


TEST_F(FilterTest, NumericComparisonFilter) {
    // EQ
    NumericComparisonFilter eq_filter("status_code", 200, NumericComparisonFilter::Operator::EQ);
    auto entry_eq_match = createLogEntry(1, "web.log", now, LogLevel::INFO, "Success", {{"status_code", "200"}});
    auto entry_eq_mismatch = createLogEntry(2, "web.log", now, LogLevel::INFO, "Not found", {{"status_code", "404"}});
    auto entry_eq_double = createLogEntry(3, "web.log", now, LogLevel::INFO, "Partial OK", {{"response_time", "250.75"}});
    EXPECT_TRUE(eq_filter.matches(entry_eq_match));
    EXPECT_FALSE(eq_filter.matches(entry_eq_mismatch));
    EXPECT_FALSE(eq_filter.matches(entry_eq_double)); // Comparing double with int

    // NEQ
    NumericComparisonFilter neq_filter("status_code", 200, NumericComparisonFilter::Operator::NEQ);
    EXPECT_FALSE(neq_filter.matches(entry_eq_match));
    EXPECT_TRUE(neq_filter.matches(entry_eq_mismatch));

    // GT
    NumericComparisonFilter gt_filter("response_time", 100.5, NumericComparisonFilter::Operator::GT);
    EXPECT_TRUE(gt_filter.matches(entry_eq_double)); // 250.75 > 100.5
    EXPECT_FALSE(gt_filter.matches(entry_eq_match)); // 200 is not > 100.5

    // LT
    NumericComparisonFilter lt_filter("response_time", 300.0, NumericComparisonFilter::Operator::LT);
    EXPECT_TRUE(lt_filter.matches(entry_eq_double)); // 250.75 < 300.0
    EXPECT_FALSE(lt_filter.matches(entry_eq_match)); // 200 is not < 300.0

    // GTE
    NumericComparisonFilter gte_filter("response_time", 250.75, NumericComparisonFilter::Operator::GTE);
    EXPECT_TRUE(gte_filter.matches(entry_eq_double)); // 250.75 >= 250.75
    EXPECT_FALSE(gte_filter.matches(entry_eq_match)); // 200 is not >= 250.75

    // LTE
    NumericComparisonFilter lte_filter("response_time", 250.75, NumericComparisonFilter::Operator::LTE);
    EXPECT_TRUE(lte_filter.matches(entry_eq_double)); // 250.75 <= 250.75
    EXPECT_FALSE(lte_filter.matches(entry_eq_match)); // 200 is not <= 250.75

    // Field not found
    NumericComparisonFilter missing_field_filter("non_existent_field", 100, NumericComparisonFilter::Operator::EQ);
    EXPECT_FALSE(missing_field_filter.matches(entry_eq_match));

    // Non-numeric field
    NumericComparisonFilter non_numeric_filter("status_code", 100, NumericComparisonFilter::Operator::EQ);
    auto entry_non_numeric = createLogEntry(4, "web.log", now, LogLevel::INFO, "Text status", {{"status_code", "OK"}});
    EXPECT_FALSE(non_numeric_filter.matches(entry_non_numeric));
}

TEST_F(FilterTest, NestedNumericComparisonFilter) {
    // EQ
    NestedNumericComparisonFilter eq_filter("request.duration_ms", 50.5, NumericComparisonFilter::Operator::EQ);
    auto entry_match = createLogEntry(1, "api.log", now, LogLevel::INFO, "Request processed", {{"request.duration_ms", "50.5"}});
    auto entry_mismatch = createLogEntry(2, "api.log", now, LogLevel::INFO, "Request processed", {{"request.duration_ms", "100.0"}});
    auto entry_nested_mismatch = createLogEntry(3, "api.log", now, LogLevel::INFO, "Request processed", {{"response.duration_ms", "50.5"}});
    EXPECT_TRUE(eq_filter.matches(entry_match));
    EXPECT_FALSE(eq_filter.matches(entry_mismatch));
    EXPECT_FALSE(eq_filter.matches(entry_nested_mismatch));

    // GT
    NestedNumericComparisonFilter gt_filter("request.duration_ms", 40.0, NumericComparisonFilter::Operator::GT);
    EXPECT_TRUE(gt_filter.matches(entry_match)); // 50.5 > 40.0
    EXPECT_TRUE(gt_filter.matches(entry_mismatch)); // 100.0 is > 40.0, so this should be true

    // LTE
    NestedNumericComparisonFilter lte_filter("request.duration_ms", 50.5, NumericComparisonFilter::Operator::LTE);
    EXPECT_TRUE(lte_filter.matches(entry_match)); // 50.5 <= 50.5
    EXPECT_FALSE(lte_filter.matches(entry_mismatch)); // 100.0 is not <= 50.5

    // Field not found
    NestedNumericComparisonFilter missing_field_filter("request.non_existent", 10.0, NumericComparisonFilter::Operator::EQ);
    EXPECT_FALSE(missing_field_filter.matches(entry_match));

    // Non-numeric field
    auto entry_non_numeric = createLogEntry(4, "api.log", now, LogLevel::INFO, "Request processed", {{"request.duration_ms", "fast"}});
    EXPECT_FALSE(eq_filter.matches(entry_non_numeric));
}

TEST_F(FilterTest, NumericComparisonFilterFloatingPointPrecision) {
    // Test EQ with values very close
    NumericComparisonFilter eq_filter("value", 100.0, NumericComparisonFilter::Operator::EQ);
    EXPECT_TRUE(eq_filter.matches(createLogEntry(1, "log", now, LogLevel::INFO, "msg", {{"value", "100.0000000001"}}))); // Within epsilon
    EXPECT_TRUE(eq_filter.matches(createLogEntry(2, "log", now, LogLevel::INFO, "msg", {{"value", "99.9999999999"}})));  // Within epsilon
    EXPECT_FALSE(eq_filter.matches(createLogEntry(3, "log", now, LogLevel::INFO, "msg", {{"value", "100.000001"}}))); // Outside epsilon (e.g., 1e-6 difference)

    // Test NEQ with values very close
    NumericComparisonFilter neq_filter("value", 100.0, NumericComparisonFilter::Operator::NEQ);
    EXPECT_FALSE(neq_filter.matches(createLogEntry(4, "log", now, LogLevel::INFO, "msg", {{"value", "100.0000000001"}}))); // Within epsilon, so considered equal
    EXPECT_FALSE(neq_filter.matches(createLogEntry(5, "log", now, LogLevel::INFO, "msg", {{"value", "99.9999999999"}})));  // Within epsilon, so considered equal
    EXPECT_TRUE(neq_filter.matches(createLogEntry(6, "log", now, LogLevel::INFO, "msg", {{"value", "100.000001"}})));  // Outside epsilon, so considered not equal
}

TEST_F(FilterTest, NestedNumericComparisonFilterFloatingPointPrecision) {
    // Test EQ with values very close
    NestedNumericComparisonFilter eq_filter("metric.value", 100.0, NumericComparisonFilter::Operator::EQ);
    EXPECT_TRUE(eq_filter.matches(createLogEntry(1, "log", now, LogLevel::INFO, "msg", {{"metric.value", "100.0000000001"}})));
    EXPECT_TRUE(eq_filter.matches(createLogEntry(2, "log", now, LogLevel::INFO, "msg", {{"metric.value", "99.9999999999"}})));
    EXPECT_FALSE(eq_filter.matches(createLogEntry(3, "log", now, LogLevel::INFO, "msg", {{"metric.value", "100.000001"}})));

    // Test NEQ with values very close
    NestedNumericComparisonFilter neq_filter("metric.value", 100.0, NumericComparisonFilter::Operator::NEQ);
    EXPECT_FALSE(neq_filter.matches(createLogEntry(4, "log", now, LogLevel::INFO, "msg", {{"metric.value", "100.0000000001"}})));
    EXPECT_FALSE(neq_filter.matches(createLogEntry(5, "log", now, LogLevel::INFO, "msg", {{"metric.value", "99.9999999999"}})));
    EXPECT_TRUE(neq_filter.matches(createLogEntry(6, "log", now, LogLevel::INFO, "msg", {{"metric.value", "100.000001"}})));
}

TEST_F(FilterTest, NestedBoolFilter) {
    NestedBoolFilter filter_true("user.is_admin", true);
    auto entry_true_match = createLogEntry(1, "user.log", now, LogLevel::INFO, "User info", {{"user.is_admin", "true"}});
    auto entry_true_mismatch = createLogEntry(2, "user.log", now, LogLevel::INFO, "User info", {{"user.is_admin", "false"}});
    auto entry_true_numeric = createLogEntry(3, "user.log", now, LogLevel::INFO, "User info", {{"user.is_admin", "1"}});
    auto entry_true_nested_mismatch = createLogEntry(4, "user.log", now, LogLevel::INFO, "User info", {{"account.is_admin", "true"}});

    EXPECT_TRUE(filter_true.matches(entry_true_match));
    EXPECT_FALSE(filter_true.matches(entry_true_mismatch));
    EXPECT_TRUE(filter_true.matches(entry_true_numeric)); // Should handle "1" as true
    EXPECT_FALSE(filter_true.matches(entry_true_nested_mismatch));

    NestedBoolFilter filter_false("user.is_admin", false);
    EXPECT_TRUE(filter_false.matches(createLogEntry(5, "user.log", now, LogLevel::INFO, "User info", {{"user.is_admin", "0"}}))); // Handle "0" as false
    EXPECT_TRUE(filter_false.matches(createLogEntry(6, "user.log", now, LogLevel::INFO, "User info", {{"user.is_admin", "false"}})));
    EXPECT_FALSE(filter_false.matches(entry_true_match));
    EXPECT_FALSE(filter_false.matches(entry_true_numeric));

    // Field not found
    NestedBoolFilter missing_field_filter("user.non_existent", true);
    EXPECT_FALSE(missing_field_filter.matches(entry_true_match));
}

TEST_F(FilterTest, NestedFieldValueFilter) {
    // Literal, case-insensitive
    NestedFieldValueFilter filter_literal("user.role", "admin", PatternType::Literal, false);
    auto entry_match = createLogEntry(1, "user.log", now, LogLevel::INFO, "User role", {{"user.role", "Admin"}});
    auto entry_mismatch = createLogEntry(2, "user.log", now, LogLevel::INFO, "User role", {{"user.role", "user"}});
    auto entry_nested_mismatch = createLogEntry(3, "user.log", now, LogLevel::INFO, "User role", {{"account.role", "Admin"}});
    EXPECT_TRUE(filter_literal.matches(entry_match));
    EXPECT_FALSE(filter_literal.matches(entry_mismatch));
    EXPECT_FALSE(filter_literal.matches(entry_nested_mismatch));

    // Regex
    NestedFieldValueFilter filter_regex("user.id", R"(U\\d{3})", PatternType::Regex);
    auto entry_regex_match = createLogEntry(4, "user.log", now, LogLevel::INFO, "User ID", {{"user.id", "U123"}});
    auto entry_regex_mismatch = createLogEntry(5, "user.log", now, LogLevel::INFO, "User ID", {{"user.id", "123"}});
    EXPECT_TRUE(filter_regex.matches(entry_regex_match));
    EXPECT_FALSE(filter_regex.matches(entry_regex_mismatch));

    // Wildcard
    NestedFieldValueFilter filter_wildcard("request.path", "/api/v1/*", PatternType::Wildcard);
    auto entry_wildcard_match = createLogEntry(6, "api.log", now, LogLevel::INFO, "API call", {{"request.path", "/api/v1/users"}});
    auto entry_wildcard_mismatch = createLogEntry(7, "api.log", now, LogLevel::INFO, "API call", {{"request.path", "/api/v2/users"}});
    EXPECT_TRUE(filter_wildcard.matches(entry_wildcard_match));
    EXPECT_FALSE(filter_wildcard.matches(entry_wildcard_mismatch));

    // Field not found
    NestedFieldValueFilter missing_field_filter("user.non_existent", "any", PatternType::Literal);
    EXPECT_FALSE(missing_field_filter.matches(entry_match));
}

TEST_F(FilterTest, CompositeFilterEmptyListAND) {
    CompositeFilter filter(CompositeFilter::Logic::AND);
    auto entry = createLogEntry(1, "web.log", now, LogLevel::INFO, "Any message");
    EXPECT_TRUE(filter.matches(entry)); // Empty AND should always return true
}

TEST_F(FilterTest, CompositeFilterEmptyListOR) {
    CompositeFilter filter(CompositeFilter::Logic::OR);
    auto entry = createLogEntry(1, "web.log", now, LogLevel::INFO, "Any message");
    EXPECT_FALSE(filter.matches(entry)); // Empty OR should always return false
}

TEST_F(FilterTest, NestedFieldValueFilterWildcardSubstringMatch) {
    NestedFieldValueFilter filter("request.path", "/api/*/users", PatternType::Wildcard);
    auto entry1 = createLogEntry(1, "api.log", now, LogLevel::INFO, "msg", {{"request.path", "/api/v1/users"}});
    auto entry2 = createLogEntry(2, "api.log", now, LogLevel::INFO, "msg", {{"request.path", "/api/v2/admin/users"}});
    auto entry3 = createLogEntry(3, "api.log", now, LogLevel::INFO, "msg", {{"request.path", "/api/v1/data"}});
    auto entry4 = createLogEntry(4, "api.log", now, LogLevel::INFO, "msg", {{"request.path", "/internal/api/v1/users"}}); // Should match "/api/v1/users" as substring

    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_TRUE(filter.matches(entry2));
    EXPECT_FALSE(filter.matches(entry3));
    EXPECT_TRUE(filter.matches(entry4));
}

TEST_F(FilterTest, CompositeFilterAND) {
    CompositeFilter filter(CompositeFilter::Logic::AND);
    filter.add(std::make_shared<FieldExistsFilter>("user_id"));
    filter.add(std::make_shared<FieldValueFilter>("status_code", "200", PatternType::Literal));
    
    auto entry1 = createLogEntry(1, "web.log", now, LogLevel::INFO, "Success", {{"user_id", "123"}, {"status_code", "200"}});
    auto entry2 = createLogEntry(2, "web.log", now, LogLevel::INFO, "Success with user", {{"user_id", "456"}});
    auto entry3 = createLogEntry(3, "web.log", now, LogLevel::INFO, "Success no user", {{"status_code", "200"}});
    auto entry4 = createLogEntry(4, "web.log", now, LogLevel::INFO, "Failure", {{"user_id", "789"}, {"status_code", "500"}});

    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2));
    EXPECT_FALSE(filter.matches(entry3));
    EXPECT_FALSE(filter.matches(entry4));
}

TEST_F(FilterTest, CompositeFilterOR) {
    CompositeFilter filter(CompositeFilter::Logic::OR);
    filter.add(std::make_shared<FieldExistsFilter>("user_id"));
    filter.add(std::make_shared<FieldValueFilter>("status_code", "404", PatternType::Literal));

    auto entry1 = createLogEntry(1, "web.log", now, LogLevel::INFO, "Success", {{"user_id", "123"}, {"status_code", "200"}});
    auto entry2 = createLogEntry(2, "web.log", now, LogLevel::INFO, "Success with user", {{"user_id", "456"}});
    auto entry3 = createLogEntry(3, "web.log", now, LogLevel::INFO, "Not Found", {{"status_code", "404"}});
    auto entry4 = createLogEntry(4, "web.log", now, LogLevel::INFO, "Failure", {{"user_id", "789"}, {"status_code", "500"}});

    EXPECT_TRUE(filter.matches(entry1)); // Has user_id, so should match for OR filter
    EXPECT_TRUE(filter.matches(entry2));  // Has user_id
    EXPECT_TRUE(filter.matches(entry3));  // Has status_code 404
    EXPECT_TRUE(filter.matches(entry4)); // Has user_id, so should match for OR filter
}

TEST_F(FilterTest, ExclusionFilter) {
    auto base_filter = std::make_shared<FieldExistsFilter>("error_code");
    ExclusionFilter filter(base_filter);

    auto entry1 = createLogEntry(1, "app.log", now, LogLevel::ERROR, "An error occurred", {{"error_code", "E101"}});
    auto entry2 = createLogEntry(2, "app.log", now, LogLevel::INFO, "Operation successful", {{"status", "OK"}});

    EXPECT_FALSE(filter.matches(entry1)); // error_code exists, so exclusion filter should return false
    EXPECT_TRUE(filter.matches(entry2));  // error_code does not exist, so exclusion filter should return true
}

// Re-add TimeRangeFilter tests here to ensure correct order if inserting above
TEST_F(FilterTest, TimeRangeFilterFromStrings) {
    // Valid case
    auto filter_res = TimeRangeFilter::fromStrings("2023-01-15 10:00:00", "2023-01-15 11:00:00");
    ASSERT_TRUE(filter_res.has_value()) << filter_res.error();
    auto filter = filter_res.value();
    
    std::tm tm = {};
    std::stringstream ss_in("2023-01-15 10:30:00");
    ss_in >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto time_in = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    std::stringstream ss_out("2023-01-15 12:00:00");
    ss_out >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto time_out = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    auto entry1 = createLogEntry(1, "time.log", time_in, LogLevel::INFO, "In time");
    auto entry2 = createLogEntry(2, "time.log", time_out, LogLevel::INFO, "Out of time");

    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2));

    // Invalid start time
    auto invalid_start_res = TimeRangeFilter::fromStrings("invalid-date", "2023-01-15 11:00:00");
    EXPECT_FALSE(invalid_start_res.has_value());
    EXPECT_NE(invalid_start_res.error().find("Invalid"), std::string::npos);

    // Invalid end time
    auto invalid_end_res = TimeRangeFilter::fromStrings("2023-01-15 10:00:00", "invalid-date");
    EXPECT_FALSE(invalid_end_res.has_value());
    EXPECT_NE(invalid_end_res.error().find("Invalid"), std::string::npos);
}

TEST_F(FilterTest, TimeRangeFilterForDay) {
    // Valid case YYYY-MM-DD
    auto filter_res_ymd = TimeRangeFilter::forDay("2023-03-10");
    ASSERT_TRUE(filter_res_ymd.has_value()) << filter_res_ymd.error();
    auto filter_ymd = filter_res_ymd.value();

    std::tm tm = {};
    std::stringstream ss_day_in_morning("2023-03-10 08:30:00");
    ss_day_in_morning >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto day_in_morning = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    EXPECT_TRUE(filter_ymd.matches(createLogEntry(1, "day.log", day_in_morning, LogLevel::INFO, "Morning log")));

    std::stringstream ss_day_in_evening("2023-03-10 22:15:00");
    ss_day_in_evening >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto day_in_evening = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    EXPECT_TRUE(filter_ymd.matches(createLogEntry(2, "day.log", day_in_evening, LogLevel::INFO, "Evening log")));

    std::stringstream ss_day_before("2023-03-09 23:59:59");
    ss_day_before >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto day_before = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    EXPECT_FALSE(filter_ymd.matches(createLogEntry(3, "day.log", day_before, LogLevel::INFO, "Day before")));

    std::stringstream ss_day_after("2023-03-11 00:00:00");
    ss_day_after >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto day_after = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    EXPECT_FALSE(filter_ymd.matches(createLogEntry(4, "day.log", day_after, LogLevel::INFO, "Day after")));

    // Valid case MM/DD/YYYY
    auto filter_res_mdy = TimeRangeFilter::forDay("03/10/2023");
    ASSERT_TRUE(filter_res_mdy.has_value()) << filter_res_mdy.error();
    auto filter_mdy = filter_res_mdy.value();
    EXPECT_TRUE(filter_mdy.matches(createLogEntry(5, "day.log", day_in_morning, LogLevel::INFO, "Morning log MM/DD/YYYY")));

    // Invalid date format
    auto invalid_date_res = TimeRangeFilter::forDay("not-a-date");
    EXPECT_FALSE(invalid_date_res.has_value());
    EXPECT_NE(invalid_date_res.error().find("Invalid date format"), std::string::npos);
}

TEST_F(FilterTest, TimeRangeFilterSince) {
    auto filter_res = TimeRangeFilter::since("5m ago");
    ASSERT_TRUE(filter_res.has_value()) << filter_res.error();
    auto filter = filter_res.value();
    
    auto time_in = std::chrono::system_clock::now() - std::chrono::minutes(2);
    auto time_out = std::chrono::system_clock::now() - std::chrono::minutes(10);

    auto entry1 = createLogEntry(1, "time.log", time_in, LogLevel::INFO, "In time");
    auto entry2 = createLogEntry(2, "time.log", time_out, LogLevel::INFO, "Out of time");

    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2));

    // Invalid relative time
    auto invalid_rel_time_res = TimeRangeFilter::since("foo bar");
    EXPECT_FALSE(invalid_rel_time_res.has_value());
    EXPECT_NE(invalid_rel_time_res.error().find("Invalid relative time format"), std::string::npos);
    
    // Another invalid relative time
    auto invalid_rel_time_res2 = TimeRangeFilter::since("1 year from now");
    EXPECT_FALSE(invalid_rel_time_res2.has_value());
    EXPECT_NE(invalid_rel_time_res2.error().find("Invalid relative time format"), std::string::npos);
}

TEST_F(FilterTest, ValueSetFilter) {
    std::set<std::string> allowed_users = {"admin", "guest", "root"};
    ValueSetFilter filter("user", allowed_users, true); // Case-sensitive

    EXPECT_TRUE(filter.matches(createLogEntry(1, "log", now, LogLevel::INFO, "msg", {{"user", "admin"}})));
    EXPECT_TRUE(filter.matches(createLogEntry(2, "log", now, LogLevel::INFO, "msg", {{"user", "guest"}})));
    EXPECT_FALSE(filter.matches(createLogEntry(3, "log", now, LogLevel::INFO, "msg", {{"user", "Admin"}}))); // Case-sensitive mismatch
    EXPECT_FALSE(filter.matches(createLogEntry(4, "log", now, LogLevel::INFO, "msg", {{"user", "dev"}})));
    EXPECT_FALSE(filter.matches(createLogEntry(5, "log", now, LogLevel::INFO, "msg", {{"role", "admin"}}))); // Wrong field

    std::set<std::string> allowed_statuses = {"OK", "ERROR"};
    ValueSetFilter filter_icase("status", allowed_statuses, false); // Case-insensitive

    EXPECT_TRUE(filter_icase.matches(createLogEntry(6, "log", now, LogLevel::INFO, "msg", {{"status", "ok"}})));
    EXPECT_TRUE(filter_icase.matches(createLogEntry(7, "log", now, LogLevel::INFO, "msg", {{"status", "ERROR"}})));
    EXPECT_FALSE(filter_icase.matches(createLogEntry(8, "log", now, LogLevel::INFO, "msg", {{"status", "pending"}})));
}

TEST_F(FilterTest, NestedValueSetFilter) {
    std::set<std::string> allowed_roles = {"superadmin", "moderator"};
    NestedValueSetFilter filter("user.role", allowed_roles, true); // Case-sensitive

    EXPECT_TRUE(filter.matches(createLogEntry(1, "log", now, LogLevel::INFO, "msg", {{"user.role", "superadmin"}})));
    EXPECT_FALSE(filter.matches(createLogEntry(2, "log", now, LogLevel::INFO, "msg", {{"user.role", "SuperAdmin"}}))); // Case-sensitive mismatch
    EXPECT_FALSE(filter.matches(createLogEntry(3, "log", now, LogLevel::INFO, "msg", {{"user.role", "viewer"}})));
    EXPECT_FALSE(filter.matches(createLogEntry(4, "log", now, LogLevel::INFO, "msg", {{"account.role", "superadmin"}}))); // Wrong field path

    std::set<std::string> allowed_events = {"LOGIN", "LOGOUT"};
    NestedValueSetFilter filter_icase("event.type", allowed_events, false); // Case-insensitive

    EXPECT_TRUE(filter_icase.matches(createLogEntry(5, "log", now, LogLevel::INFO, "msg", {{"event.type", "login"}})));
    EXPECT_TRUE(filter_icase.matches(createLogEntry(6, "log", now, LogLevel::INFO, "msg", {{"event.type", "LOGOUT"}})));
    EXPECT_FALSE(filter_icase.matches(createLogEntry(7, "log", now, LogLevel::INFO, "msg", {{"event.type", "failed_login"}})));
}

// FilterRule JSON tests
TEST_F(FilterJsonTest, FilterRuleToJson) {
    FilterRule fr;
    fr.field = LogEntryField::MESSAGE;
    fr.op = FilterOperator::CONTAINS;
    fr.value = "error";
    fr.caseSensitive = true;

    nlohmann::json j;
    to_json(j, fr);

    EXPECT_EQ(j["field"], "message");
    EXPECT_EQ(j["op"], "CONTAINS");
    EXPECT_EQ(j["value"], "error");
    EXPECT_EQ(j["caseSensitive"], true);
}

TEST_F(FilterJsonTest, FilterRuleFromJsonSuccess) {
    nlohmann::json j = {
        {"field", "level"},
        {"op", "EQUALS"},
        {"value", "INFO"},
        {"caseSensitive", false}
    };

    FilterRule fr;
    auto result = from_json(j, fr);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_EQ(fr.field, LogEntryField::LEVEL);
    EXPECT_EQ(fr.op, FilterOperator::EQUALS);
    EXPECT_EQ(fr.value, "INFO");
    EXPECT_EQ(fr.caseSensitive, false);
}

TEST_F(FilterJsonTest, FilterRuleFromJsonInvalidField) {
    nlohmann::json j = {
        {"field", "non_existent_field"},
        {"op", "EQUALS"},
        {"value", "INFO"}
    };

    FilterRule fr;
    auto result = from_json(j, fr);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("unrecognized field"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterRuleFromJsonMissingField) {
    nlohmann::json j = {
        {"op", "EQUALS"},
        {"value", "INFO"}
    };

    FilterRule fr;
    auto result = from_json(j, fr);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("missing or has invalid 'field'"), std::string::npos);
}

// FilterCondition JSON tests
TEST_F(FilterJsonTest, FilterConditionToJson) {
    FilterCondition fc = createFilterCondition(
        LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-01-01T00:00:00Z",
        FilterValueType::DATETIME, false, "%Y-%m-%dT%H:%M:%SZ"
    );

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "timestamp");
    EXPECT_EQ(j["op"], "GREATER_THAN");
    EXPECT_EQ(j["value"], "2023-01-01T00:00:00Z");
    EXPECT_EQ(j["value_type"], static_cast<int>(FilterValueType::DATETIME));
    EXPECT_EQ(j["caseSensitive"], false);
    EXPECT_EQ(j["datetimeFormat"], "%Y-%m-%dT%H:%M:%SZ");
}

TEST_F(FilterJsonTest, FilterConditionFromJsonSuccess) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "warning"},
        {"value_type", static_cast<int>(FilterValueType::STRING)},
        {"caseSensitive", true}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::MESSAGE);
    EXPECT_EQ(fc.op, FilterOperator::CONTAINS);
    EXPECT_EQ(fc.value, "warning");
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
    EXPECT_EQ(fc.caseSensitive, true);
    EXPECT_FALSE(fc.datetimeFormat.has_value());
}

TEST_F(FilterJsonTest, FilterConditionFromJsonDatetimeSuccess) {
    nlohmann::json j = {
        {"field", "timestamp"},
        {"op", "LESS_THAN"},
        {"value", "2023-12-31 23:59:59"},
        {"value_type", static_cast<int>(FilterValueType::DATETIME)},
        {"caseSensitive", false},
        {"datetimeFormat", "%Y-%m-%d %H:%M:%S"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::TIMESTAMP);
    EXPECT_EQ(fc.op, FilterOperator::LESS_THAN);
    EXPECT_EQ(fc.value, "2023-12-31 23:59:59");
    EXPECT_EQ(fc.valueType, FilterValueType::DATETIME);
    EXPECT_TRUE(fc.datetimeFormat.has_value());
    EXPECT_EQ(*fc.datetimeFormat, "%Y-%m-%d %H:%M:%S");
}

TEST_F(FilterJsonTest, FilterConditionFromJsonInvalidField) {
    nlohmann::json j = {
        {"field", "bad_field"},
        {"op", "EQUALS"},
        {"value", "value"},
        {"value_type", static_cast<int>(FilterValueType::STRING)}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("unrecognized 'field' string"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterConditionFromJsonMissingOp) {
    nlohmann::json j = {
        {"field", "message"},
        {"value", "value"},
        {"value_type", static_cast<int>(FilterValueType::STRING)}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("missing or has invalid 'op'"), std::string::npos);
}

// FilterCondition datetime constructor validation
TEST_F(FilterJsonTest, FilterConditionDatetimeConstructorValidation) {
    // Should throw if DATETIME type is used without format
    EXPECT_THROW(
        FilterCondition(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-01-01", FilterValueType::DATETIME),
        std::invalid_argument
    );

    // Should not throw if DATETIME type is used with format
    EXPECT_NO_THROW(
        FilterCondition(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-01-01", "%Y-%m-%d")
    );

    // Should not throw for other value types
    EXPECT_NO_THROW(
        FilterCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "error", FilterValueType::STRING)
    );
}

// FilterExpression JSON tests
TEST_F(FilterJsonTest, FilterExpressionToJsonCondition) {
    FilterCondition fc = createFilterCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "hello");
    FilterExpression fe(fc);

    nlohmann::json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("condition"));
    EXPECT_EQ(j["condition"]["field"], "message");
    EXPECT_EQ(j["condition"]["op"], "CONTAINS");
    EXPECT_EQ(j["condition"]["value"], "hello");
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonCondition) {
    nlohmann::json j = {
        {"condition", {
            {"field", "source_file"},
            {"op", "ENDS_WITH"},
            {"value", ".log"},
            {"value_type", static_cast<int>(FilterValueType::STRING)}
        }}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isCondition());
    ASSERT_TRUE(fe.getCondition().has_value());
    EXPECT_EQ(fe.getCondition()->field, LogEntryField::SOURCE_FILE);
    EXPECT_EQ(fe.getCondition()->op, FilterOperator::ENDS_WITH);
    EXPECT_EQ(fe.getCondition()->value, ".log");
}

TEST_F(FilterJsonTest, FilterExpressionToJsonLogicalAND) {
    FilterExpression fe = FilterExpression::create(createFilterCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO"))
                            .And(FilterExpression::create(createFilterCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "user")));

    nlohmann::json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("operator"));
    EXPECT_EQ(j["operator"], "AND");
    ASSERT_TRUE(j.contains("operands"));
    ASSERT_EQ(j["operands"].size(), 2);

    // Check first operand
    EXPECT_EQ(j["operands"][0]["condition"]["field"], "level");
    EXPECT_EQ(j["operands"][0]["condition"]["value"], "INFO");

    // Check second operand
    EXPECT_EQ(j["operands"][1]["condition"]["field"], "message");
    EXPECT_EQ(j["operands"][1]["condition"]["value"], "user");
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonLogicalOR) {
    nlohmann::json j = {
        {"operator", "OR"},
        {"operands", nlohmann::json::array({
            {{"condition", {{"field", "level"}, {"op", "EQUALS"}, {"value", "ERROR"}, {"value_type", 0}}}},
            {{"condition", {{"field", "level"}, {"op", "EQUALS"}, {"value", "FATAL"}, {"value_type", 0}}}}
        })}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isLogical());
    ASSERT_TRUE(fe.getLogicalOperator().has_value());
    EXPECT_EQ(*fe.getLogicalOperator(), FilterLogicalOperator::OR);
    ASSERT_EQ(fe.getExpressions().size(), 2);

    // Check first operand
    ASSERT_TRUE(fe.getExpressions()[0].isCondition());
    EXPECT_EQ(fe.getExpressions()[0].getCondition()->field, LogEntryField::LEVEL);
    EXPECT_EQ(fe.getExpressions()[0].getCondition()->value, "ERROR");

    // Check second operand
    ASSERT_TRUE(fe.getExpressions()[1].isCondition());
    EXPECT_EQ(fe.getExpressions()[1].getCondition()->field, LogEntryField::LEVEL);
    EXPECT_EQ(fe.getExpressions()[1].getCondition()->value, "FATAL");
}

TEST_F(FilterJsonTest, FilterExpressionToJsonLogicalNOT) {
    FilterExpression fe = FilterExpression::create(createFilterCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "DEBUG"))
                            .Not();

    nlohmann::json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("operator"));
    EXPECT_EQ(j["operator"], "NOT");
    ASSERT_TRUE(j.contains("operands"));
    ASSERT_EQ(j["operands"].size(), 1);

    // Check operand
    EXPECT_EQ(j["operands"][0]["condition"]["field"], "level");
    EXPECT_EQ(j["operands"][0]["condition"]["value"], "DEBUG");
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonLogicalNOT) {
    nlohmann::json j = {
        {"operator", "NOT"},
        {"operands", nlohmann::json::array({
            {{"condition", {{"field", "message"}, {"op", "STARTS_WITH"}, {"value", "Success"}, {"value_type", 0}}}}
        })}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isLogical());
    ASSERT_TRUE(fe.getLogicalOperator().has_value());
    EXPECT_EQ(*fe.getLogicalOperator(), FilterLogicalOperator::NOT);
    ASSERT_EQ(fe.getExpressions().size(), 1);

    // Check operand
    ASSERT_TRUE(fe.getExpressions()[0].isCondition());
    EXPECT_EQ(fe.getExpressions()[0].getCondition()->field, LogEntryField::MESSAGE);
    EXPECT_EQ(fe.getExpressions()[0].getCondition()->value, "Success");
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonNested) {
    nlohmann::json j = {
        {"operator", "AND"},
        {"operands", nlohmann::json::array({
            nlohmann::json {{"condition", {{"field", "level"}, {"op", "EQUALS"}, {"value", "ERROR"}, {"value_type", 0}}}},
            nlohmann::json {{"operator", "OR"},
             {"operands", nlohmann::json::array({
                nlohmann::json {{"condition", {{"field", "message"}, {"op", "CONTAINS"}, {"value", "database"}, {"value_type", 0}}}},
                nlohmann::json {{"condition", {{"field", "message"}, {"op", "CONTAINS"}, {"value", "network"}, {"value_type", 0}}}}
             })}}
        })}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isLogical());
    ASSERT_TRUE(fe.getLogicalOperator().has_value());
    EXPECT_EQ(*fe.getLogicalOperator(), FilterLogicalOperator::AND);
    ASSERT_EQ(fe.getExpressions().size(), 2);

    // Check first level operand (condition)
    ASSERT_TRUE(fe.getExpressions()[0].isCondition());
    EXPECT_EQ(fe.getExpressions()[0].getCondition()->field, LogEntryField::LEVEL);
    EXPECT_EQ(fe.getExpressions()[0].getCondition()->value, "ERROR");

    // Check second level operand (logical OR)
    ASSERT_TRUE(fe.getExpressions()[1].isLogical());
    EXPECT_EQ(*fe.getExpressions()[1].getLogicalOperator(), FilterLogicalOperator::OR);
    ASSERT_EQ(fe.getExpressions()[1].getExpressions().size(), 2);

    EXPECT_EQ(fe.getExpressions()[1].getExpressions()[0].getCondition()->field, LogEntryField::MESSAGE);
    EXPECT_EQ(fe.getExpressions()[1].getExpressions()[0].getCondition()->value, "database");
    EXPECT_EQ(fe.getExpressions()[1].getExpressions()[1].getCondition()->field, LogEntryField::MESSAGE);
    EXPECT_EQ(fe.getExpressions()[1].getExpressions()[1].getCondition()->value, "network");
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonInvalidOperand) {
    nlohmann::json j = {
        {"operator", "AND"},
        {"operands", nlohmann::json::array({
            {{"condition", {{"field", "level"}, {"op", "EQUALS"}, {"value", "ERROR"}, {"value_type", 0}}}},
            {{"condition", {{"field", "message"}, {"op", "CONTAINS"}, {"value", "database"}, {"value_type", 0}}}},
            // Invalid operand: missing "op"
            {{"condition", {{"field", "source_file"}, {"value", "main.cpp"}, {"value_type", 0}}}} 
        })}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("missing or has invalid 'op'"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonMissingOperands) {
    nlohmann::json j = {
        {"operator", "AND"}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("must contain 'operands' array"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonUnknownOperator) {
    nlohmann::json j = {
        {"operator", "XOR"}, // Unknown operator
        {"operands", nlohmann::json::array({
            {{"condition", {{"field", "level"}, {"op", "EQUALS"}, {"value", "ERROR"}, {"value_type", 0}}}}
        })}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Unknown filter logical operator"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonMissingConditionOrOperator) {
    nlohmann::json j = {
        {"invalid_key", "some_value"}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("must contain either 'condition' or 'operator'"), std::string::npos);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
