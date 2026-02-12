// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "filter/core.h"
#include "filter/condition.h" // Added
#include "filter/types.h" // Added
#include "filter/concrete_filters.h" // Added
#include "core/log/types.h"
#include "helper.h"
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
    auto entry1 = createLogEntry(1, LogLevel::INFO, "Server started", now, {}, "server-alpha.log");
    auto entry2 = createLogEntry(2, LogLevel::INFO, "Production server log", now, {}, "production.server.log");
    auto entry3 = createLogEntry(3, LogLevel::INFO, "No match", now, {}, "server_log");
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_TRUE(filter.matches(entry2));
    EXPECT_FALSE(filter.matches(entry3));

    SourceFileFilter filter_icase("SERVER.*\\.LOG", PatternType::Regex, false);
    auto entry4 = createLogEntry(4, LogLevel::INFO, "Server started", now, {}, "Server-Alpha.log");
    EXPECT_TRUE(filter_icase.matches(entry4));
}

TEST_F(FilterTest, SourceFileFilterWildcard) {
    // New test for standardized wildcard behavior (anchored)
    SourceFileFilter filter("server*.log", PatternType::Wildcard);
    auto entry1 = createLogEntry(1, LogLevel::INFO, "Match", now, {}, "server-alpha.log");
    auto entry2 = createLogEntry(2, LogLevel::INFO, "No Match", now, {}, "production.server.log");
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2)); // Should not match because of anchored regex
}

TEST_F(FilterTest, FieldValueFilterRegex) {
    // Default caseSensitive is false, so it should be case-insensitive
    FieldValueFilter filter_regex("error_code", R"(ERR\d{3})", PatternType::Regex);
    auto entry3 = createLogEntry(3, LogLevel::ERROR, "DB error", now, {{"error_code", "ERR501"}}, "app.log");
    auto entry4 = createLogEntry(4, LogLevel::ERROR, "Network error", now, {{"error_code", "err200"}}, "app.log");
    EXPECT_TRUE(filter_regex.matches(entry3)); // ERR501 matches ERR\d{3} case-insensitively
    EXPECT_TRUE(filter_regex.matches(entry4)); // err200 matches ERR\d{3} case-insensitively

    FieldValueFilter filter_regex_icase_explicit("error_code", R"(ERR\d{3})", PatternType::Regex, false);
    EXPECT_TRUE(filter_regex_icase_explicit.matches(entry3)); // ERR501 matches ERR\\d{3} case-insensitively
    EXPECT_TRUE(filter_regex_icase_explicit.matches(entry4)); // err200 matches ERR\\d{3} case-insensitively
}

TEST_F(FilterTest, FieldValueFilterRegexCaseSensitive) {
    FieldValueFilter filter_regex_cs("error_code", R"(ERR\d{3})", PatternType::Regex, true); // Explicitly case sensitive
    auto entry3 = createLogEntry(3, LogLevel::ERROR, "DB error", now, {{"error_code", "ERR501"}}, "app.log");
    auto entry4 = createLogEntry(4, LogLevel::ERROR, "Network error", now, {{"error_code", "err200"}}, "app.log");
    EXPECT_TRUE(filter_regex_cs.matches(entry3));
    EXPECT_FALSE(filter_regex_cs.matches(entry4)); // Should not match 'err'
}

TEST_F(FilterTest, RegexFilterCaseSensitive) {
    auto filter_res_cs = RegexFilter::create("Error", true); // Using the new factory, case sensitive
    ASSERT_TRUE(filter_res_cs.has_value()) << filter_res_cs.error();
    auto filter_cs = filter_res_cs.value();

    auto entry1 = createLogEntry(1, LogLevel::INFO, "error found", now, {}, "app.log");
    auto entry2 = createLogEntry(2, LogLevel::INFO, "Error found", now, {}, "app.log");
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
    EXPECT_TRUE(filter_complex->matches(createLogEntry(3, LogLevel::WARNING, "Warning: Something happened", now, {}, "app.log")));
    EXPECT_TRUE(filter_complex->matches(createLogEntry(4, LogLevel::ERROR, "ERROR: Critical issue", now, {}, "app.log")));
    EXPECT_TRUE(filter_complex->matches(createLogEntry(5, LogLevel::FATAL, "fatal: System down", now, {}, "app.log")));
    EXPECT_FALSE(filter_complex->matches(createLogEntry(6, LogLevel::INFO, "info: All good", now, {}, "app.log")));
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
    auto entry_eq_match = createLogEntry(1, LogLevel::INFO, "Success", now, {{"status_code", "200"}}, "web.log");
    auto entry_eq_mismatch = createLogEntry(2, LogLevel::INFO, "Not found", now, {{"status_code", "404"}}, "web.log");
    auto entry_eq_double = createLogEntry(3, LogLevel::INFO, "Partial OK", now, {{"response_time", "250.75"}}, "web.log");
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
    auto entry_non_numeric = createLogEntry(4, LogLevel::INFO, "Text status", now, {{"status_code", "OK"}}, "web.log");
    EXPECT_FALSE(non_numeric_filter.matches(entry_non_numeric));
}

TEST_F(FilterTest, DottedKeyNumericComparisonFilter) {
    // EQ
    DottedKeyNumericComparisonFilter eq_filter("request.duration_ms", 50.5, NumericComparisonFilter::Operator::EQ);
    auto entry_match = createLogEntry(1, LogLevel::INFO, "Request processed", now, {{"request.duration_ms", "50.5"}}, "api.log");
    auto entry_mismatch = createLogEntry(2, LogLevel::INFO, "Request processed", now, {{"request.duration_ms", "100.0"}}, "api.log");
    auto entry_nested_mismatch = createLogEntry(3, LogLevel::INFO, "Request processed", now, {{"response.duration_ms", "50.5"}}, "api.log");
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
    auto entry_non_numeric = createLogEntry(4, LogLevel::INFO, "Request processed", now, {{"request.duration_ms", "fast"}}, "api.log");
    EXPECT_FALSE(eq_filter.matches(entry_non_numeric));
}

TEST_F(FilterTest, NumericComparisonFilterFloatingPointPrecision) {
    // Test EQ with values very close
    NumericComparisonFilter eq_filter("value", 100.0, NumericComparisonFilter::Operator::EQ);
    EXPECT_TRUE(eq_filter.matches(createLogEntry(1, LogLevel::INFO, "msg", now, {{"value", "100.0000000001"}}, "log")));
    EXPECT_TRUE(eq_filter.matches(createLogEntry(2, LogLevel::INFO, "msg", now, {{"value", "99.9999999999"}}, "log")));
    EXPECT_FALSE(eq_filter.matches(createLogEntry(3, LogLevel::INFO, "msg", now, {{"value", "100.000001"}}, "log")));

    // Test NEQ with values very close
    NumericComparisonFilter neq_filter("value", 100.0, NumericComparisonFilter::Operator::NEQ);
    EXPECT_FALSE(neq_filter.matches(createLogEntry(4, LogLevel::INFO, "msg", now, {{"value", "100.0000000001"}}, "log")));
    EXPECT_FALSE(neq_filter.matches(createLogEntry(5, LogLevel::INFO, "msg", now, {{"value", "99.9999999999"}}, "log")));
    EXPECT_TRUE(neq_filter.matches(createLogEntry(6, LogLevel::INFO, "msg", now, {{"value", "100.000001"}}, "log")));
}

TEST_F(FilterTest, DottedKeyNumericComparisonFilterFloatingPointPrecision) {
    // Test EQ with values very close
    DottedKeyNumericComparisonFilter eq_filter("metric.value", 100.0, NumericComparisonFilter::Operator::EQ);
    EXPECT_TRUE(eq_filter.matches(createLogEntry(1, LogLevel::INFO, "msg", now, {{"metric.value", "100.0000000001"}}, "log")));
    EXPECT_TRUE(eq_filter.matches(createLogEntry(2, LogLevel::INFO, "msg", now, {{"metric.value", "99.9999999999"}}, "log")));
    EXPECT_FALSE(eq_filter.matches(createLogEntry(3, LogLevel::INFO, "msg", now, {{"metric.value", "100.000001"}}, "log")));

    // Test NEQ with values very close
    DottedKeyNumericComparisonFilter neq_filter("metric.value", 100.0, NumericComparisonFilter::Operator::NEQ);
    EXPECT_FALSE(neq_filter.matches(createLogEntry(4, LogLevel::INFO, "msg", now, {{"metric.value", "100.0000000001"}}, "log")));
    EXPECT_FALSE(neq_filter.matches(createLogEntry(5, LogLevel::INFO, "msg", now, {{"metric.value", "99.9999999999"}}, "log")));
    EXPECT_TRUE(neq_filter.matches(createLogEntry(6, LogLevel::INFO, "msg", now, {{"metric.value", "100.000001"}}, "log")));
}

TEST_F(FilterTest, DottedKeyBoolFilter) {
    DottedKeyBoolFilter filter_true("user.is_admin", true);
    auto entry_true_match = createLogEntry(1, LogLevel::INFO, "User info", now, {{"user.is_admin", "true"}}, "user.log");
    auto entry_true_mismatch = createLogEntry(2, LogLevel::INFO, "User info", now, {{"user.is_admin", "false"}}, "user.log");
    auto entry_true_numeric = createLogEntry(3, LogLevel::INFO, "User info", now, {{"user.is_admin", "1"}}, "user.log");
    auto entry_true_nested_mismatch = createLogEntry(4, LogLevel::INFO, "User info", now, {{"account.is_admin", "true"}}, "user.log");

    EXPECT_TRUE(filter_true.matches(entry_true_match));
    EXPECT_FALSE(filter_true.matches(entry_true_mismatch));
    EXPECT_TRUE(filter_true.matches(entry_true_numeric)); // Should handle "1" as true
    EXPECT_FALSE(filter_true.matches(entry_true_nested_mismatch));

    DottedKeyBoolFilter filter_false("user.is_admin", false);
    EXPECT_TRUE(filter_false.matches(createLogEntry(5, LogLevel::INFO, "User info", now, {{"user.is_admin", "0"}}, "user.log")));
    EXPECT_TRUE(filter_false.matches(createLogEntry(6, LogLevel::INFO, "User info", now, {{"user.is_admin", "false"}}, "user.log")));
    EXPECT_FALSE(filter_false.matches(entry_true_match));
    EXPECT_FALSE(filter_false.matches(entry_true_numeric));

    // Field not found
    DottedKeyBoolFilter missing_field_filter("user.non_existent", true);
    EXPECT_FALSE(missing_field_filter.matches(entry_true_match));
}

TEST_F(FilterTest, DottedKeyFieldValueFilter) {
    // Literal, case-insensitive
    DottedKeyFieldValueFilter filter_literal("user.role", "admin", PatternType::Literal, false);
    auto entry_match = createLogEntry(1, LogLevel::INFO, "User role", now, {{"user.role", "Admin"}}, "user.log");
    auto entry_mismatch = createLogEntry(2, LogLevel::INFO, "User role", now, {{"user.role", "user"}}, "user.log");
    auto entry_nested_mismatch = createLogEntry(3, LogLevel::INFO, "User role", now, {{"account.role", "Admin"}}, "user.log");
    EXPECT_TRUE(filter_literal.matches(entry_match));
    EXPECT_FALSE(filter_literal.matches(entry_mismatch));
    EXPECT_FALSE(filter_literal.matches(entry_nested_mismatch));

    // Regex
    DottedKeyFieldValueFilter filter_regex("user.id", R"(U\d{3})", PatternType::Regex);
    auto entry_regex_match = createLogEntry(4, LogLevel::INFO, "User ID", now, {{"user.id", "U123"}}, "user.log");
    auto entry_regex_mismatch = createLogEntry(5, LogLevel::INFO, "User ID", now, {{"user.id", "123"}}, "user.log");
    EXPECT_TRUE(filter_regex.matches(entry_regex_match));
    EXPECT_FALSE(filter_regex.matches(entry_regex_mismatch));

    // Wildcard
    DottedKeyFieldValueFilter filter_wildcard("request.path", "/api/v1/*", PatternType::Wildcard);
    auto entry_wildcard_match = createLogEntry(6, LogLevel::INFO, "API call", now, {{"request.path", "/api/v1/users"}}, "api.log");
    auto entry_wildcard_mismatch = createLogEntry(7, LogLevel::INFO, "API call", now, {{"request.path", "/api/v2/users"}}, "api.log");
    EXPECT_TRUE(filter_wildcard.matches(entry_wildcard_match));
    EXPECT_FALSE(filter_wildcard.matches(entry_wildcard_mismatch));

    // Field not found
    DottedKeyFieldValueFilter missing_field_filter("user.non_existent", "any", PatternType::Literal);
    EXPECT_FALSE(missing_field_filter.matches(entry_match));
}

TEST_F(FilterTest, DottedKeyFieldValueFilterWildcardSubstringMatch) {
    DottedKeyFieldValueFilter filter("request.path", "/api/*/users", PatternType::Wildcard);
    auto entry1 = createLogEntry(1, LogLevel::INFO, "msg", now, {{"request.path", "/api/v1/users"}}, "api.log");
    auto entry2 = createLogEntry(2, LogLevel::INFO, "msg", now, {{"request.path", "/api/v2/admin/users"}}, "api.log");
    auto entry3 = createLogEntry(3, LogLevel::INFO, "msg", now, {{"request.path", "/api/v1/data"}}, "api.log");
    
    // NOTE: Wildcards are now anchored to match the full string (standard glob behavior).
    // "/internal/api/v1/users" should NOT match "/api/*/users" because of the prefix.
    auto entry4 = createLogEntry(4, LogLevel::INFO, "msg", now, {{"request.path", "/internal/api/v1/users"}}, "api.log");

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

    auto entry1 = createLogEntry(1, LogLevel::INFO, "In time", time_in, {}, "time.log");
    auto entry2 = createLogEntry(2, LogLevel::INFO, "Out of time", time_out, {}, "time.log");

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
    EXPECT_TRUE(filter_ymd.matches(createLogEntry(1, LogLevel::INFO, "Morning log", day_in_morning, {}, "day.log")));

    std::stringstream ss_day_in_evening("2023-03-10 22:15:00");
    ss_day_in_evening >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto day_in_evening = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    EXPECT_TRUE(filter_ymd.matches(createLogEntry(2, LogLevel::INFO, "Evening log", day_in_evening, {}, "day.log")));

    std::stringstream ss_day_before("2023-03-09 23:59:59");
    ss_day_before >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto day_before = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    EXPECT_FALSE(filter_ymd.matches(createLogEntry(3, LogLevel::INFO, "Day before", day_before, {}, "day.log")));

    std::stringstream ss_day_after("2023-03-11 00:00:00");
    ss_day_after >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto day_after = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    EXPECT_FALSE(filter_ymd.matches(createLogEntry(4, LogLevel::INFO, "Day after", day_after, {}, "day.log")));
    
    // Test end of day boundary (fixed bug)
    std::stringstream ss_day_end("2023-03-10 23:59:59");
    ss_day_end >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto day_end = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    EXPECT_TRUE(filter_ymd.matches(createLogEntry(5, LogLevel::INFO, "End of day", day_end, {}, "day.log")));

    // Valid case MM/DD/YYYY
    auto filter_res_mdy = TimeRangeFilter::forDay("03/10/2023");
    ASSERT_TRUE(filter_res_mdy.has_value()) << filter_res_mdy.error();
    auto filter_mdy = filter_res_mdy.value();
    EXPECT_TRUE(filter_mdy.matches(createLogEntry(6, LogLevel::INFO, "Morning log MM/DD/YYYY", day_in_morning, {}, "day.log")));

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

    auto entry1 = createLogEntry(1, LogLevel::INFO, "In time", time_in, {}, "time.log");
    auto entry2 = createLogEntry(2, LogLevel::INFO, "Out of time", time_out, {}, "time.log");

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
