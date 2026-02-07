// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "filter/Core.h"
#include "filter/Condition.h" // Added
#include "filter/Types.h" // Added
#include "filter/ConcreteFilters.h" // Added
#include "core/Log/Types.h"
#include "Utils.h"
#include <chrono>
#include <map>
#include <sstream>
#include <cmath>

using namespace filter;

// Define epsilon for floating point comparisons if not already available
constexpr double EPSILON = 1e-9;

class FilterTest : public ::testing::Test {
protected:
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
};

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

TEST_F(FilterTest, SourceFileFilterWildcard) {
    // New test for standardized wildcard behavior (anchored)
    SourceFileFilter filter("server*.log", PatternType::Wildcard);
    auto entry1 = createLogEntry(1, "server-alpha.log", now, LogLevel::INFO, "Match");
    auto entry2 = createLogEntry(2, "production.server.log", now, LogLevel::INFO, "No Match");
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2)); // Should not match because of anchored regex
}

TEST_F(FilterTest, FieldValueFilterRegex) {
    // Default caseSensitive is false, so it should be case-insensitive
    FieldValueFilter filter_regex("error_code", R"(ERR\d{3})", PatternType::Regex);
    auto entry3 = createLogEntry(3, "app.log", now, LogLevel::ERROR, "DB error", {{"error_code", "ERR501"}});
    auto entry4 = createLogEntry(4, "app.log", now, LogLevel::ERROR, "Network error", {{"error_code", "err200"}});
    EXPECT_TRUE(filter_regex.matches(entry3)); // ERR501 matches ERR\d{3} case-insensitively
    EXPECT_TRUE(filter_regex.matches(entry4)); // err200 matches ERR\d{3} case-insensitively

    FieldValueFilter filter_regex_icase_explicit("error_code", R"(ERR\d{3})", PatternType::Regex, false);
    EXPECT_TRUE(filter_regex_icase_explicit.matches(entry3)); // ERR501 matches ERR\\d{3} case-insensitively
    EXPECT_TRUE(filter_regex_icase_explicit.matches(entry4)); // err200 matches ERR\\d{3} case-insensitively
}

TEST_F(FilterTest, FieldValueFilterRegexCaseSensitive) {
    FieldValueFilter filter_regex_cs("error_code", R"(ERR\d{3})", PatternType::Regex, true); // Explicitly case sensitive
    auto entry3 = createLogEntry(3, "app.log", now, LogLevel::ERROR, "DB error", {{"error_code", "ERR501"}});
    auto entry4 = createLogEntry(4, "app.log", now, LogLevel::ERROR, "Network error", {{"error_code", "err200"}});
    EXPECT_TRUE(filter_regex_cs.matches(entry3));
    EXPECT_FALSE(filter_regex_cs.matches(entry4)); // Should not match 'err'
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
    // Error message varies by platform/compiler, just check it's not empty and related to regex if possible
    EXPECT_FALSE(filter_res.error().message.empty());
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

TEST_F(FilterTest, DottedKeyNumericComparisonFilter) {
    // EQ
    DottedKeyNumericComparisonFilter eq_filter("request.duration_ms", 50.5, NumericComparisonFilter::Operator::EQ);
    auto entry_match = createLogEntry(1, "api.log", now, LogLevel::INFO, "Request processed", {{"request.duration_ms", "50.5"}});
    auto entry_mismatch = createLogEntry(2, "api.log", now, LogLevel::INFO, "Request processed", {{"request.duration_ms", "100.0"}});
    auto entry_nested_mismatch = createLogEntry(3, "api.log", now, LogLevel::INFO, "Request processed", {{"response.duration_ms", "50.5"}});
    EXPECT_TRUE(eq_filter.matches(entry_match));
    EXPECT_FALSE(eq_filter.matches(entry_mismatch));
    EXPECT_FALSE(eq_filter.matches(entry_nested_mismatch));

    // GT
    DottedKeyNumericComparisonFilter gt_filter("request.duration_ms", 40.0, NumericComparisonFilter::Operator::GT);
    EXPECT_TRUE(gt_filter.matches(entry_match)); // 50.5 > 40.0
    EXPECT_TRUE(gt_filter.matches(entry_mismatch)); // 100.0 is > 40.0, so this should be true

    // LTE
    DottedKeyNumericComparisonFilter lte_filter("request.duration_ms", 50.5, NumericComparisonFilter::Operator::LTE);
    EXPECT_TRUE(lte_filter.matches(entry_match)); // 50.5 <= 50.5
    EXPECT_FALSE(lte_filter.matches(entry_mismatch)); // 100.0 is not <= 50.5

    // Field not found
    DottedKeyNumericComparisonFilter missing_field_filter("request.non_existent", 10.0, NumericComparisonFilter::Operator::EQ);
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

TEST_F(FilterTest, DottedKeyNumericComparisonFilterFloatingPointPrecision) {
    // Test EQ with values very close
    DottedKeyNumericComparisonFilter eq_filter("metric.value", 100.0, NumericComparisonFilter::Operator::EQ);
    EXPECT_TRUE(eq_filter.matches(createLogEntry(1, "log", now, LogLevel::INFO, "msg", {{"metric.value", "100.0000000001"}})));
    EXPECT_TRUE(eq_filter.matches(createLogEntry(2, "log", now, LogLevel::INFO, "msg", {{"metric.value", "99.9999999999"}})));
    EXPECT_FALSE(eq_filter.matches(createLogEntry(3, "log", now, LogLevel::INFO, "msg", {{"metric.value", "100.000001"}})));

    // Test NEQ with values very close
    DottedKeyNumericComparisonFilter neq_filter("metric.value", 100.0, NumericComparisonFilter::Operator::NEQ);
    EXPECT_FALSE(neq_filter.matches(createLogEntry(4, "log", now, LogLevel::INFO, "msg", {{"metric.value", "100.0000000001"}})));
    EXPECT_FALSE(neq_filter.matches(createLogEntry(5, "log", now, LogLevel::INFO, "msg", {{"metric.value", "99.9999999999"}})));  // Within epsilon, so considered equal
    EXPECT_TRUE(neq_filter.matches(createLogEntry(6, "log", now, LogLevel::INFO, "msg", {{"metric.value", "100.000001"}})));  // Outside epsilon, so considered not equal
}

TEST_F(FilterTest, DottedKeyBoolFilter) {
    DottedKeyBoolFilter filter_true("user.is_admin", true);
    auto entry_true_match = createLogEntry(1, "user.log", now, LogLevel::INFO, "User info", {{"user.is_admin", "true"}});
    auto entry_true_mismatch = createLogEntry(2, "user.log", now, LogLevel::INFO, "User info", {{"user.is_admin", "false"}});
    auto entry_true_numeric = createLogEntry(3, "user.log", now, LogLevel::INFO, "User info", {{"user.is_admin", "1"}});
    auto entry_true_nested_mismatch = createLogEntry(4, "user.log", now, LogLevel::INFO, "User info", {{"account.is_admin", "true"}});

    EXPECT_TRUE(filter_true.matches(entry_true_match));
    EXPECT_FALSE(filter_true.matches(entry_true_mismatch));
    EXPECT_TRUE(filter_true.matches(entry_true_numeric)); // Should handle "1" as true
    EXPECT_FALSE(filter_true.matches(entry_true_nested_mismatch));

    DottedKeyBoolFilter filter_false("user.is_admin", false);
    EXPECT_TRUE(filter_false.matches(createLogEntry(5, "user.log", now, LogLevel::INFO, "User info", {{"user.is_admin", "0"}}))); // Handle "0" as false
    EXPECT_TRUE(filter_false.matches(createLogEntry(6, "user.log", now, LogLevel::INFO, "User info", {{"user.is_admin", "false"}})));
    EXPECT_FALSE(filter_false.matches(entry_true_match));
    EXPECT_FALSE(filter_false.matches(entry_true_numeric));

    // Field not found
    DottedKeyBoolFilter missing_field_filter("user.non_existent", true);
    EXPECT_FALSE(missing_field_filter.matches(entry_true_match));
}

TEST_F(FilterTest, DottedKeyFieldValueFilter) {
    // Literal, case-insensitive
    DottedKeyFieldValueFilter filter_literal("user.role", "admin", PatternType::Literal, false);
    auto entry_match = createLogEntry(1, "user.log", now, LogLevel::INFO, "User role", {{"user.role", "Admin"}});
    auto entry_mismatch = createLogEntry(2, "user.log", now, LogLevel::INFO, "User role", {{"user.role", "user"}});
    auto entry_nested_mismatch = createLogEntry(3, "user.log", now, LogLevel::INFO, "User role", {{"account.role", "Admin"}});
    EXPECT_TRUE(filter_literal.matches(entry_match));
    EXPECT_FALSE(filter_literal.matches(entry_mismatch));
    EXPECT_FALSE(filter_literal.matches(entry_nested_mismatch));

    // Regex
    DottedKeyFieldValueFilter filter_regex("user.id", R"(U\d{3})", PatternType::Regex);
    auto entry_regex_match = createLogEntry(4, "user.log", now, LogLevel::INFO, "User ID", {{"user.id", "U123"}});
    auto entry_regex_mismatch = createLogEntry(5, "user.log", now, LogLevel::INFO, "User ID", {{"user.id", "123"}});
    EXPECT_TRUE(filter_regex.matches(entry_regex_match));
    EXPECT_FALSE(filter_regex.matches(entry_regex_mismatch));

    // Wildcard
    DottedKeyFieldValueFilter filter_wildcard("request.path", "/api/v1/*", PatternType::Wildcard);
    auto entry_wildcard_match = createLogEntry(6, "api.log", now, LogLevel::INFO, "API call", {{"request.path", "/api/v1/users"}});
    auto entry_wildcard_mismatch = createLogEntry(7, "api.log", now, LogLevel::INFO, "API call", {{"request.path", "/api/v2/users"}});
    EXPECT_TRUE(filter_wildcard.matches(entry_wildcard_match));
    EXPECT_FALSE(filter_wildcard.matches(entry_wildcard_mismatch));

    // Field not found
    DottedKeyFieldValueFilter missing_field_filter("user.non_existent", "any", PatternType::Literal);
    EXPECT_FALSE(missing_field_filter.matches(entry_match));
}

TEST_F(FilterTest, DottedKeyFieldValueFilterWildcardSubstringMatch) {
    DottedKeyFieldValueFilter filter("request.path", "/api/*/users", PatternType::Wildcard);
    auto entry1 = createLogEntry(1, "api.log", now, LogLevel::INFO, "msg", {{"request.path", "/api/v1/users"}});
    auto entry2 = createLogEntry(2, "api.log", now, LogLevel::INFO, "msg", {{"request.path", "/api/v2/admin/users"}});
    auto entry3 = createLogEntry(3, "api.log", now, LogLevel::INFO, "msg", {{"request.path", "/api/v1/data"}});
    
    // NOTE: Wildcards are now anchored to match the full string (standard glob behavior).
    // "/internal/api/v1/users" should NOT match "/api/*/users" because of the prefix.
    auto entry4 = createLogEntry(4, "api.log", now, LogLevel::INFO, "msg", {{"request.path", "/internal/api/v1/users"}});

    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_TRUE(filter.matches(entry2));
    EXPECT_FALSE(filter.matches(entry3));
    EXPECT_FALSE(filter.matches(entry4)); // Changed expectation to FALSE
}

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
    
    // Test end of day boundary (fixed bug)
    std::stringstream ss_day_end("2023-03-10 23:59:59");
    ss_day_end >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto day_end = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    EXPECT_TRUE(filter_ymd.matches(createLogEntry(5, "day.log", day_end, LogLevel::INFO, "End of day")));

    // Valid case MM/DD/YYYY
    auto filter_res_mdy = TimeRangeFilter::forDay("03/10/2023");
    ASSERT_TRUE(filter_res_mdy.has_value()) << filter_res_mdy.error();
    auto filter_mdy = filter_res_mdy.value();
    EXPECT_TRUE(filter_mdy.matches(createLogEntry(6, "day.log", day_in_morning, LogLevel::INFO, "Morning log MM/DD/YYYY")));

    // Invalid date format
    auto invalid_date_res = TimeRangeFilter::forDay("not-a-date");
    EXPECT_FALSE(invalid_date_res.has_value());
    EXPECT_NE(invalid_date_res.error().find("Invalid date format"), std::string::npos);
}

TEST_F(FilterTest, TimeRangeFilterSince) {
    auto filter_res = TimeRangeFilter::since("5m ago");
    ASSERT_TRUE(filter_res.has_value()) << filter_res.error();
    auto filter = filter_res.value();
    
    // Robustness fix: Use a wider gap to avoid race conditions with multiple now() calls.
    // "5m ago" puts the start time at roughly T - 5m.
    // We test with T - 2m (should be safely inside) and T - 20m (should be safely outside).
    // Even if execution stalls for a few seconds/minutes, the gap is large enough.
    auto time_in = std::chrono::system_clock::now() - std::chrono::minutes(2);
    auto time_out = std::chrono::system_clock::now() - std::chrono::minutes(20);

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
