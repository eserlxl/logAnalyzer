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

// --- Coverage for the module/host/id/lineNumber sort keys (LogAnalyzer::lessByField) ---

TEST_F(AnalyzerTestFixture, LessByFieldModuleOrdersByValueAndNulloptFirst) {
    LogEntry a; a.module = "auth";
    LogEntry b; b.module = "db";
    LogEntry none; // nullopt module
    EXPECT_TRUE(LogAnalyzer::lessByField(a, b, filter::SortBy::MODULE));
    EXPECT_FALSE(LogAnalyzer::lessByField(b, a, filter::SortBy::MODULE));
    EXPECT_TRUE(LogAnalyzer::lessByField(none, a, filter::SortBy::MODULE)); // nullopt sorts first
    EXPECT_FALSE(LogAnalyzer::lessByField(a, none, filter::SortBy::MODULE));
}

TEST_F(AnalyzerTestFixture, LessByFieldHostOrdersByValueAndNulloptFirst) {
    LogEntry a; a.host = "alpha";
    LogEntry b; b.host = "beta";
    LogEntry none;
    EXPECT_TRUE(LogAnalyzer::lessByField(a, b, filter::SortBy::HOST));
    EXPECT_FALSE(LogAnalyzer::lessByField(b, a, filter::SortBy::HOST));
    EXPECT_TRUE(LogAnalyzer::lessByField(none, a, filter::SortBy::HOST));
}

TEST_F(AnalyzerTestFixture, LessByFieldIdOrdersNumericallyAndNulloptFirst) {
    LogEntry a; a.id = 2;
    LogEntry b; b.id = 10;
    LogEntry none;
    // size_t comparison is numeric, so 2 < 10 (a lexicographic sort would say "10" < "2").
    EXPECT_TRUE(LogAnalyzer::lessByField(a, b, filter::SortBy::ID));
    EXPECT_FALSE(LogAnalyzer::lessByField(b, a, filter::SortBy::ID));
    EXPECT_TRUE(LogAnalyzer::lessByField(none, a, filter::SortBy::ID));
}

TEST_F(AnalyzerTestFixture, LessByFieldLineNumberOrdersNumericallyAndNulloptFirst) {
    LogEntry a; a.sourceLineNumber = 5;
    LogEntry b; b.sourceLineNumber = 40;
    LogEntry none;
    EXPECT_TRUE(LogAnalyzer::lessByField(a, b, filter::SortBy::LINE_NUMBER));
    EXPECT_FALSE(LogAnalyzer::lessByField(b, a, filter::SortBy::LINE_NUMBER));
    EXPECT_TRUE(LogAnalyzer::lessByField(none, a, filter::SortBy::LINE_NUMBER));
}

TEST_F(AnalyzerTestFixture, SortByIdAscendingOrdersEntriesNumerically) {
    std::vector<LogEntry> entries;
    LogEntry e1; e1.id = 30; e1.message = "c";
    LogEntry e2; e2.id = 4;  e2.message = "a";
    LogEntry e3; e3.id = 12; e3.message = "b";
    entries.push_back(e1); entries.push_back(e2); entries.push_back(e3);
    analyzer.replaceEntries(entries);
    filter::FilterCriteria criteria;
    auto result = analyzer.getSortedFilteredEntries(criteria, filter::SortBy::ID, filter::SortOrder::ASCENDING);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 3u);
    EXPECT_EQ(result.value()[0].id.value(), 4u);
    EXPECT_EQ(result.value()[1].id.value(), 12u);
    EXPECT_EQ(result.value()[2].id.value(), 30u);
}

TEST_F(AnalyzerTestFixture, SortedFilteredEntriesWithInvalidRegexCriteria) {
    filter::FilterCriteria criteria;
    criteria.regexPattern = "["; // Invalid regex
    auto result = analyzer.getSortedFilteredEntries(criteria, filter::SortBy::TIMESTAMP, filter::SortOrder::ASCENDING);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidRegex);
}
