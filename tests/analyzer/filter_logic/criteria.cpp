// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer_test_fixture.h"
#include "helper.h"
#include <algorithm>

bool compareLogEntryVectors(const std::vector<LogEntry>& vec1, const std::vector<LogEntry>& vec2) {
    if (vec1.size() != vec2.size()) {
        return false;
    }
    for (size_t i = 0; i < vec1.size(); ++i) {
        if (vec1[i].id != vec2[i].id) {
            return false;
        }
    }
    return true;
}

// --- getFilteredEntries(const filter::FilterCriteria& criteria) tests ---

TEST_F(AnalyzerTestFixture, FilterByLevelCriteria) {
    filter::FilterCriteria criteria;
    criteria.levels = {LogLevel::INFO};
    auto exprRes = analyzer.createFilterExpressionFromCriteria(criteria);
    ASSERT_TRUE(exprRes.has_value());
    auto result = analyzer.getFilteredEntries(*exprRes);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 2);
    EXPECT_TRUE(std::all_of(result.value().begin(), result.value().end(), [](const LogEntry& e){
        return e.level == LogLevel::INFO;
    }));
}

TEST_F(AnalyzerTestFixture, FilterByKeywordCriteria) {
    filter::FilterCriteria criteria;
    criteria.keyword = "User";
    criteria.keywordCaseSensitive = true;
    auto exprRes = analyzer.createFilterExpressionFromCriteria(criteria);
    ASSERT_TRUE(exprRes.has_value());
    auto result = analyzer.getFilteredEntries(*exprRes);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 2);
    EXPECT_TRUE(std::all_of(result.value().begin(), result.value().end(), [](const LogEntry& e){
        return e.message.find("User") != std::string::npos;
    }));
}

TEST_F(AnalyzerTestFixture, FilterByRegexCriteriaValid) {
    filter::FilterCriteria criteria;
    criteria.regexPattern = ".*logged (in|out).*";
    auto exprRes = analyzer.createFilterExpressionFromCriteria(criteria);
    ASSERT_TRUE(exprRes.has_value());
    auto result = analyzer.getFilteredEntries(*exprRes);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 2);
    EXPECT_TRUE(std::all_of(result.value().begin(), result.value().end(), [](const LogEntry& e){
        return e.message.find("logged in") != std::string::npos || e.message.find("logged out") != std::string::npos;
    }));
}

TEST_F(AnalyzerTestFixture, FilterByRegexCriteriaInvalid) {
    filter::FilterCriteria criteria;
    criteria.regexPattern = "["; // Invalid regex
    auto result = analyzer.createFilterExpressionFromCriteria(criteria);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidRegex);
}

TEST_F(AnalyzerTestFixture, FilterByTimeRangeCriteria) {
    filter::FilterCriteria criteria;
    criteria.startTime = Utils::parseTime("2023-01-01 10:01:30").value();
    criteria.endTime = Utils::parseTime("2023-01-01 10:04:30").value();
    auto exprRes = analyzer.createFilterExpressionFromCriteria(criteria);
    ASSERT_TRUE(exprRes.has_value());
    auto result = analyzer.getFilteredEntries(*exprRes);
    ASSERT_TRUE(result.has_value()) << result.error().toString();
    EXPECT_EQ(result.value().size(), 3); // Entries 3, 4, 5
    std::vector<LogEntry> expected_entries = {baseEntries[2], baseEntries[3], baseEntries[4]};
    EXPECT_TRUE(compareLogEntryVectors(result.value(), expected_entries));
}

TEST_F(AnalyzerTestFixture, FilterByEmptyCriteria) {
    filter::FilterCriteria criteria; // Empty criteria should match all
    auto exprRes = analyzer.createFilterExpressionFromCriteria(criteria);
    ASSERT_TRUE(exprRes.has_value());
    auto result = analyzer.getFilteredEntries(*exprRes);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), baseEntries.size());
    EXPECT_TRUE(compareLogEntryVectors(result.value(), baseEntries));
}

TEST_F(AnalyzerTestFixture, FilterByCombinedCriteria) {
    filter::FilterCriteria criteria;
    criteria.levels = {LogLevel::ERROR};
    criteria.keyword = "DB";
    auto exprRes = analyzer.createFilterExpressionFromCriteria(criteria);
    ASSERT_TRUE(exprRes.has_value());
    auto result = analyzer.getFilteredEntries(*exprRes);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 1);
    EXPECT_EQ(result.value()[0].id, 5); // Failed to connect to DB.
}

TEST_F(AnalyzerTestFixture, FilterReturnsEmptyWhenNoMatch) {
    filter::FilterCriteria criteria;
    criteria.keyword = "NonExistentKeyword";
    auto exprRes = analyzer.createFilterExpressionFromCriteria(criteria);
    ASSERT_TRUE(exprRes.has_value());
    auto result = analyzer.getFilteredEntries(*exprRes);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

TEST_F(AnalyzerTestFixture, FilterOnEmptyEntries) {
    LogAnalyzer emptyAnalyzer; // Analyzer with no entries
    filter::FilterCriteria criteria;
    criteria.keyword = "test";
    auto exprRes = emptyAnalyzer.createFilterExpressionFromCriteria(criteria);
    ASSERT_TRUE(exprRes.has_value());
    auto result = emptyAnalyzer.getFilteredEntries(*exprRes);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}
