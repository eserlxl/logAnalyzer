// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer_test_fixture.h"

// --- getSortedFilteredEntries(const filter::FilterCriteria& criteria, ...) tests ---

TEST_F(AnalyzerTestFixture, SortByTimestampAscendingCriteria) {
    filter::FilterCriteria criteria; // All entries
    auto result = analyzer.getSortedFilteredEntries(criteria, filter::SortBy::TIMESTAMP, filter::SortOrder::ASCENDING);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), baseEntries.size());
    for (size_t i = 0; i < result.value().size() - 1; ++i) {
        EXPECT_LE(result.value()[i].timestamp, result.value()[i+1].timestamp);
    }
}

TEST_F(AnalyzerTestFixture, SortByTimestampDescendingCriteria) {
    filter::FilterCriteria criteria; // All entries
    auto result = analyzer.getSortedFilteredEntries(criteria, filter::SortBy::TIMESTAMP, filter::SortOrder::DESCENDING);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), baseEntries.size());
    for (size_t i = 0; i < result.value().size() - 1; ++i) {
        EXPECT_GE(result.value()[i].timestamp, result.value()[i+1].timestamp);
    }
}

TEST_F(AnalyzerTestFixture, SortByLevelAscendingCriteria) {
    filter::FilterCriteria criteria; // All entries
    auto result = analyzer.getSortedFilteredEntries(criteria, filter::SortBy::LEVEL, filter::SortOrder::ASCENDING);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), baseEntries.size());
    std::vector<LogLevel> expectedOrder = {
        LogLevel::DEBUG, LogLevel::DEBUG, LogLevel::INFO, LogLevel::INFO, 
        LogLevel::WARNING, LogLevel::ERROR, LogLevel::ERROR, LogLevel::CRITICAL
    };
    for (size_t i = 0; i < result.value().size(); ++i) {
        EXPECT_EQ(result.value()[i].level, expectedOrder[i]);
    }
}

TEST_F(AnalyzerTestFixture, SortByMessageDescendingCriteria) {
    filter::FilterCriteria criteria; // All entries
    auto result = analyzer.getSortedFilteredEntries(criteria, filter::SortBy::MESSAGE, filter::SortOrder::DESCENDING);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), baseEntries.size());
    for (size_t i = 0; i < result.value().size() - 1; ++i) {
        EXPECT_GE(result.value()[i].message, result.value()[i+1].message);
    }
}

TEST_F(AnalyzerTestFixture, SortByThreadIdAscendingCriteria) {
    filter::FilterCriteria criteria; // All entries
    auto result = analyzer.getSortedFilteredEntries(criteria, filter::SortBy::THREAD_ID, filter::SortOrder::ASCENDING);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), baseEntries.size());

    size_t current_idx = 0;
    while(current_idx < result.value().size() && !result.value()[current_idx].threadId.has_value()) {
        current_idx++;
    }
    EXPECT_EQ(current_idx, 3); // Expect 3 nullopt entries first

    for (size_t i = current_idx; i < result.value().size() - 1; ++i) {
        ASSERT_TRUE(result.value()[i].threadId.has_value());
        ASSERT_TRUE(result.value()[i+1].threadId.has_value());
        EXPECT_LE(result.value()[i].threadId.value(), result.value()[i+1].threadId.value());
    }
}

TEST_F(AnalyzerTestFixture, SortByThreadIdDescendingCriteria) {
    filter::FilterCriteria criteria; // All entries
    auto result = analyzer.getSortedFilteredEntries(criteria, filter::SortBy::THREAD_ID, filter::SortOrder::DESCENDING);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), baseEntries.size());

    size_t current_idx = 0;
    while(current_idx < result.value().size() && result.value()[current_idx].threadId.has_value()) {
        current_idx++;
    }
    EXPECT_EQ(current_idx, 5); 
    
    for (size_t i = 0; i < 4; ++i) { 
        ASSERT_TRUE(result.value()[i].threadId.has_value());
        ASSERT_TRUE(result.value()[i+1].threadId.has_value());
        EXPECT_GE(result.value()[i].threadId.value(), result.value()[i+1].threadId.value());
    }
    for (size_t i = 5; i < result.value().size(); ++i) {
        EXPECT_FALSE(result.value()[i].threadId.has_value());
    }
}

TEST_F(AnalyzerTestFixture, SortedFilteredEntriesWithInvalidRegexCriteria) {
    filter::FilterCriteria criteria;
    criteria.regexPattern = "["; // Invalid regex
    auto result = analyzer.getSortedFilteredEntries(criteria, filter::SortBy::TIMESTAMP, filter::SortOrder::ASCENDING);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidRegex);
}
