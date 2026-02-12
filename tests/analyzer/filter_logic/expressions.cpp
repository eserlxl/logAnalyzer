// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer_test_fixture.h"
#include "filter/expression.h"
#include "filter/condition.h"
#include "filter/types.h"
#include "core/log/types.h"

using namespace filter;

// --- getFilteredEntries(const FilterExpression& expression) tests ---

TEST_F(AnalyzerTestFixture, FilterByExpressionValidationFailure) {
    FilterExpression invalidExpression(FilterLogicalOperator::AND, {}); 
    auto result = analyzer.getFilteredEntries(invalidExpression);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::ValidationError);
    EXPECT_NE(result.error().message.find("Filter expression is invalid"), std::string::npos);
}

TEST_F(AnalyzerTestFixture, FilterByExpressionEvaluationFailure) {
    auto cond = FilterCondition::createString(LogEntryField::MESSAGE, FilterOperator::REGEX, "[");
    ASSERT_TRUE(cond.has_value()); 
    FilterExpression expression(*cond);
    auto result = analyzer.getFilteredEntries(expression);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidRegex);
}


// --- getSortedFilteredEntries(const FilterExpression& expression, ...) tests ---

TEST_F(AnalyzerTestFixture, SortedFilteredEntriesByExpressionWithInvalidRegex) {
    auto cond = FilterCondition::createString(LogEntryField::MESSAGE, FilterOperator::REGEX, "[");
    ASSERT_TRUE(cond.has_value());
    FilterExpression expression(*cond);

    auto result = analyzer.getSortedFilteredEntries(expression, SortBy::TIMESTAMP, SortOrder::ASCENDING);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidRegex);
}

TEST_F(AnalyzerTestFixture, SortedFilteredEntriesByExpressionWithValidationFailure) {
    FilterExpression invalidExpression(FilterLogicalOperator::AND, {}); 
    auto result = analyzer.getSortedFilteredEntries(invalidExpression, SortBy::TIMESTAMP, SortOrder::ASCENDING);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::ValidationError);
}

// Additional test for sorting, combining filtering and then sorting
TEST_F(AnalyzerTestFixture, FilterAndSortCombined) {
    FilterCriteria criteria;
    criteria.levels = {LogLevel::INFO, LogLevel::DEBUG}; 
    
    auto result = analyzer.getSortedFilteredEntries(criteria, SortBy::MESSAGE, SortOrder::DESCENDING);
    ASSERT_TRUE(result.has_value());
    
    ASSERT_EQ(result.value().size(), 4);
    EXPECT_EQ(result.value()[0].message, "User logged out.");
    EXPECT_EQ(result.value()[1].message, "User logged in.");
    EXPECT_EQ(result.value()[2].message, "Request completed.");
    EXPECT_EQ(result.value()[3].message, "Processing request.");
}

// Edge case: no matching entries but valid filter/sort
TEST_F(AnalyzerTestFixture, ValidFilterNoMatchingEntries) {
    FilterCriteria criteria;
    criteria.keyword = "NoSuchEntry";
    auto result = analyzer.getSortedFilteredEntries(criteria, SortBy::TIMESTAMP, SortOrder::ASCENDING);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

// Edge case: Empty analyzer with valid filter/sort
TEST_F(AnalyzerTestFixture, EmptyAnalyzerValidFilterSort) {
    LogAnalyzer emptyAnalyzer;
    FilterCriteria criteria;
    criteria.keyword = "test";
    auto result = emptyAnalyzer.getSortedFilteredEntries(criteria, SortBy::TIMESTAMP, SortOrder::ASCENDING);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}
