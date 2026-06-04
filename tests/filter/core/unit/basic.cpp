// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "filter/core.h"
#include "filter/condition.h" // Added
#include "filter/types.h" // Added
#include "filter/expression.h" // Added
#include "filter/concrete_filters.h" // Added
#include "core/log/types.h"
#include <chrono>
#include <map>
#include <sstream> // Required for std::stringstream for general string manipulation if needed

using namespace filter;

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
    // For PatternType::Wildcard, glob patterns are converted to regex and then matched against the FULL string.
    // This is the standard glob behavior (anchored).
    SourceFileFilter filter("server*", PatternType::Wildcard);
    auto entry1 = createLogEntry(1, "my-server-instance.log", now, LogLevel::INFO, "Server started");
    auto entry2 = createLogEntry(2, "server.log", now, LogLevel::INFO, "Server log");
    auto entry3 = createLogEntry(3, "another_log.txt", now, LogLevel::INFO, "No match");
    auto entry4 = createLogEntry(4, "log-from-server.log", now, LogLevel::INFO, "Log from server");

    EXPECT_FALSE(filter.matches(entry1)); // "server*" should NOT match "my-server-instance.log"
    EXPECT_TRUE(filter.matches(entry2)); // Matches "server.log"
    EXPECT_FALSE(filter.matches(entry3));
    EXPECT_FALSE(filter.matches(entry4)); // "server*" should NOT match "log-from-server.log"

    // Test a more specific substring glob using * at both ends
    SourceFileFilter filter2("*server*", PatternType::Wildcard);
    EXPECT_TRUE(filter2.matches(entry1));
    EXPECT_TRUE(filter2.matches(entry2));
    EXPECT_FALSE(filter2.matches(entry3));
    EXPECT_TRUE(filter2.matches(entry4));
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

TEST_F(FilterTest, MaxLevelFilterTest) {
    MaxLevelFilter filter(LogLevel::WARNING);
    auto entry_trace   = createLogEntry(1, "app.log", now, LogLevel::TRACE,    "Trace message");
    auto entry_debug   = createLogEntry(2, "app.log", now, LogLevel::DEBUG,    "Debug message");
    auto entry_info    = createLogEntry(3, "app.log", now, LogLevel::INFO,     "Info message");
    auto entry_warning = createLogEntry(4, "app.log", now, LogLevel::WARNING,  "Warning message");
    auto entry_error   = createLogEntry(5, "app.log", now, LogLevel::ERROR,    "Error occurred");
    auto entry_critical= createLogEntry(6, "app.log", now, LogLevel::CRITICAL, "Critical failure");
    auto entry_fatal   = createLogEntry(7, "app.log", now, LogLevel::FATAL,    "Fatal error");

    EXPECT_TRUE(filter.matches(entry_trace));
    EXPECT_TRUE(filter.matches(entry_debug));
    EXPECT_TRUE(filter.matches(entry_info));
    EXPECT_TRUE(filter.matches(entry_warning));
    EXPECT_FALSE(filter.matches(entry_error));
    EXPECT_FALSE(filter.matches(entry_critical));
    EXPECT_FALSE(filter.matches(entry_fatal));
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
    KeywordFilter filter({"error", "failed"}, KeywordFilter::Logic::OR);
    auto entry1 = createLogEntry(1, "app.log", now, LogLevel::INFO, "Request failed");
    auto entry2 = createLogEntry(2, "app.log", now, LogLevel::INFO, "An error was found");
    auto entry3 = createLogEntry(3, "app.log", now, LogLevel::INFO, "Success");
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_TRUE(filter.matches(entry2));
    EXPECT_FALSE(filter.matches(entry3));
}

TEST_F(FilterTest, KeywordFilterEmptyListAny) {
    KeywordFilter filter({}, KeywordFilter::Logic::OR); // Empty list for OR (formerly ANY)
    auto entry = createLogEntry(1, "app.log", now, LogLevel::INFO, "Any message");
    EXPECT_FALSE(filter.matches(entry)); // Empty ANY list should never match
}

TEST_F(FilterTest, KeywordFilterEmptyListAll) {
    KeywordFilter filter({}, KeywordFilter::Logic::AND); // Empty list for AND (formerly ALL)
    auto entry = createLogEntry(1, "app.log", now, LogLevel::INFO, "Any message");
    EXPECT_TRUE(filter.matches(entry)); // Empty ALL list should always match (vacuously true)
}

TEST_F(FilterTest, KeywordFilterMultiAll) {
    KeywordFilter filter({"database", "connection", "failed"}, KeywordFilter::Logic::AND);
    auto entry1 = createLogEntry(1, "db.log", now, LogLevel::ERROR, "Database connection failed");
    auto entry2 = createLogEntry(2, "db.log", now, LogLevel::WARNING, "Database connection is slow");
    auto entry3 = createLogEntry(3, "db.log", now, LogLevel::ERROR, "Request failed");
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2));
    EXPECT_FALSE(filter.matches(entry3));
}

// --- Missing Unit Tests from Audit Report ---

TEST_F(FilterTest, SourceFileFilterEmptyPatternLiteral) {
    SourceFileFilter filter("", PatternType::Literal);
    auto entry1 = createLogEntry(1, "somefile.log", now, LogLevel::INFO, "msg");
    auto entry2 = createLogEntry(2, "", now, LogLevel::INFO, "msg"); // Empty source file
    EXPECT_FALSE(filter.matches(entry1));
    EXPECT_TRUE(filter.matches(entry2)); // Empty pattern matches empty source file
}

TEST_F(FilterTest, SourceFileFilterEmptyPatternWildcard) {
    SourceFileFilter filter("", PatternType::Wildcard);
    auto entry1 = createLogEntry(1, "somefile.log", now, LogLevel::INFO, "msg");
    auto entry2 = createLogEntry(2, "", now, LogLevel::INFO, "msg"); // Empty source file
    // An empty wildcard pattern "" converts to "^$" regex.
    EXPECT_FALSE(filter.matches(entry1)); // Should NOT match non-empty string
    EXPECT_TRUE(filter.matches(entry2)); // Matches empty string
}

TEST_F(FilterTest, SourceFileFilterLogEntryEmptySourceFile) {
    // Test with literal pattern
    SourceFileFilter filter_literal("empty.log", PatternType::Literal);
    auto entry_empty_file = createLogEntry(1, "", now, LogLevel::INFO, "msg");
    auto entry_non_empty_file = createLogEntry(2, "empty.log", now, LogLevel::INFO, "msg");
    EXPECT_FALSE(filter_literal.matches(entry_empty_file));
    EXPECT_TRUE(filter_literal.matches(entry_non_empty_file));

    // Test with wildcard pattern
    SourceFileFilter filter_wildcard("*.log", PatternType::Wildcard);
    EXPECT_FALSE(filter_wildcard.matches(entry_empty_file)); // *.log should not match empty string

    SourceFileFilter filter_wildcard_empty("*", PatternType::Wildcard);
    EXPECT_TRUE(filter_wildcard_empty.matches(entry_empty_file)); // "*" converts to "^.*$" regex which matches empty string
}

TEST_F(FilterTest, SourceFileFilterWildcardOnlyStar) {
    SourceFileFilter filter("*", PatternType::Wildcard); // Matches any file
    auto entry1 = createLogEntry(1, "any.log", now, LogLevel::INFO, "msg");
    auto entry2 = createLogEntry(2, "another.txt", now, LogLevel::INFO, "msg");
    auto entry3 = createLogEntry(3, "", now, LogLevel::INFO, "msg"); // Empty source file
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_TRUE(filter.matches(entry2));
    EXPECT_TRUE(filter.matches(entry3)); // "^.*$" matches empty string
}

TEST_F(FilterTest, SourceFileFilterWildcardOnlyQuestionMark) {
    SourceFileFilter filter("?", PatternType::Wildcard); // Matches any single character file name
    auto entry1 = createLogEntry(1, "a", now, LogLevel::INFO, "msg");
    auto entry2 = createLogEntry(2, "ab", now, LogLevel::INFO, "msg");
    auto entry3 = createLogEntry(3, "", now, LogLevel::INFO, "msg");
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2));
    EXPECT_FALSE(filter.matches(entry3));
}

TEST_F(FilterTest, PredicateFilterAlwaysTrue) {
    PredicateFilter filter([](const LogEntry&){ return true; });
    auto entry = createLogEntry(1, "any.log", now, LogLevel::INFO, "msg");
    EXPECT_TRUE(filter.matches(entry));
}

TEST_F(FilterTest, PredicateFilterAlwaysFalse) {
    PredicateFilter filter([](const LogEntry&){ return false; });
    auto entry = createLogEntry(1, "any.log", now, LogLevel::INFO, "msg");
    EXPECT_FALSE(filter.matches(entry));
}

TEST_F(FilterTest, BoolFilterEmptyStringValue) {
    BoolFilter filter_true("flag", true);
    BoolFilter filter_false("flag", false);

    // An empty string is not "true", "false", "1", "0", "yes", or "no".
    // Therefore, tryParseBool should return nullopt, and matches should be false.
    EXPECT_FALSE(filter_true.matches(createLogEntry(1, "log", now, LogLevel::INFO, "msg", {{"flag", ""}})));
    EXPECT_FALSE(filter_false.matches(createLogEntry(2, "log", now, LogLevel::INFO, "msg", {{"flag", ""}})));
}

TEST_F(FilterTest, BoolFilterWhitespaceAndMixedCaseValues) {
    BoolFilter filter_true("flag", true, false); // case-insensitive
    BoolFilter filter_false("flag", false, false); // case-insensitive

    EXPECT_TRUE(filter_true.matches(createLogEntry(1, "log", now, LogLevel::INFO, "msg", {{"flag", " TRUE "}})));
    EXPECT_TRUE(filter_true.matches(createLogEntry(2, "log", now, LogLevel::INFO, "msg", {{"flag", "yEs"}})));
    EXPECT_TRUE(filter_true.matches(createLogEntry(3, "log", now, LogLevel::INFO, "msg", {{"flag", "1 "}})));

    EXPECT_TRUE(filter_false.matches(createLogEntry(4, "log", now, LogLevel::INFO, "msg", {{"flag", " FALSE"}})));
    EXPECT_TRUE(filter_false.matches(createLogEntry(5, "log", now, LogLevel::INFO, "msg", {{"flag", "no "}})));
    EXPECT_TRUE(filter_false.matches(createLogEntry(6, "log", now, LogLevel::INFO, "msg", {{"flag", " 0"}})));

    // Ensure non-bool numeric values don't accidentally match
    EXPECT_FALSE(filter_true.matches(createLogEntry(7, "log", now, LogLevel::INFO, "msg", {{"flag", "0.0"}})));
    EXPECT_FALSE(filter_false.matches(createLogEntry(8, "log", now, LogLevel::INFO, "msg", {{"flag", "0.0"}})));
    EXPECT_FALSE(filter_true.matches(createLogEntry(9, "log", now, LogLevel::INFO, "msg", {{"flag", "2"}})));
    EXPECT_FALSE(filter_false.matches(createLogEntry(10, "log", now, LogLevel::INFO, "msg", {{"flag", "-1"}})));
}

TEST_F(FilterTest, KeywordFilterEmptyLogMessage) {
    KeywordFilter filter_any({"error"}, KeywordFilter::Logic::OR);
    KeywordFilter filter_all({"error"}, KeywordFilter::Logic::AND);

    auto entry_empty_msg = createLogEntry(1, "file.log", now, LogLevel::INFO, "");
    auto entry_with_msg = createLogEntry(2, "file.log", now, LogLevel::INFO, "some message");

    EXPECT_FALSE(filter_any.matches(entry_empty_msg));
    EXPECT_FALSE(filter_all.matches(entry_empty_msg));
    EXPECT_FALSE(filter_any.matches(entry_with_msg)); // "error" not in "some message"
    EXPECT_FALSE(filter_all.matches(entry_with_msg)); // "error" not in "some message"
}

TEST_F(FilterTest, KeywordFilterOverlappingKeywordsAny) {
    KeywordFilter filter({"apple", "apple pie"}, KeywordFilter::Logic::OR);
    auto entry1 = createLogEntry(1, "log", now, LogLevel::INFO, "I like apple");
    auto entry2 = createLogEntry(2, "log", now, LogLevel::INFO, "I like apple pie");
    auto entry3 = createLogEntry(3, "log", now, LogLevel::INFO, "I like fruit");
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_TRUE(filter.matches(entry2));
    EXPECT_FALSE(filter.matches(entry3));
}

TEST_F(FilterTest, KeywordFilterOverlappingKeywordsAll) {
    KeywordFilter filter({"apple", "apple pie"}, KeywordFilter::Logic::AND);
    auto entry1 = createLogEntry(1, "log", now, LogLevel::INFO, "I like apple and apple pie");
    auto entry2 = createLogEntry(2, "log", now, LogLevel::INFO, "I like apple"); // Only 'apple'
    auto entry3 = createLogEntry(3, "log", now, LogLevel::INFO, "I like pie"); // Neither
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2));
    EXPECT_FALSE(filter.matches(entry3));
}

TEST_F(FilterTest, KeywordFilterSpecialCharacters) {
    // Assuming simple string matching, not regex.
    KeywordFilter filter("error.log", true);
    auto entry1 = createLogEntry(1, "log", now, LogLevel::INFO, "Found error.log file");
    auto entry2 = createLogEntry(2, "log", now, LogLevel::INFO, "Found error log file");
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2));

    KeywordFilter filter_regex_like("([0-9]+) errors", true); // Should be treated as literal string
    auto entry3 = createLogEntry(3, "log", now, LogLevel::INFO, "Found 123 errors");
    auto entry4 = createLogEntry(4, "log", now, LogLevel::INFO, "Found (123) errors");
    EXPECT_FALSE(filter_regex_like.matches(entry3)); // Literal match fails
    EXPECT_FALSE(filter_regex_like.matches(entry4)); // Literal match fails (missing space? or exact content)
}

// --- Composite and Exclusion Filters ---

TEST_F(FilterTest, CompositeFilterAND) {
    CompositeFilter and_filter(CompositeFilter::Logic::AND);
    and_filter.add(std::make_shared<LevelFilter>(LogLevel::ERROR));
    and_filter.add(std::make_shared<KeywordFilter>("Database", true)); // Fixed case sensitivity

    auto entry1 = createLogEntry(1, "db.log", now, LogLevel::ERROR, "Database connection error");
    auto entry2 = createLogEntry(2, "app.log", now, LogLevel::ERROR, "Network error"); // Wrong keyword
    auto entry3 = createLogEntry(3, "db.log", now, LogLevel::INFO, "Database backup success"); // Wrong level
    auto entry4 = createLogEntry(4, "sys.log", now, LogLevel::FATAL, "Critical system failure");

    EXPECT_TRUE(and_filter.matches(entry1));
    EXPECT_FALSE(and_filter.matches(entry2));
    EXPECT_FALSE(and_filter.matches(entry3));
    EXPECT_FALSE(and_filter.matches(entry4));
}

TEST_F(FilterTest, CompositeFilterOR) {
    CompositeFilter or_filter(CompositeFilter::Logic::OR);
    or_filter.add(std::make_shared<LevelFilter>(LogLevel::ERROR));
    or_filter.add(std::make_shared<KeywordFilter>("Database", true)); // Fixed case sensitivity

    auto entry1 = createLogEntry(1, "db.log", now, LogLevel::ERROR, "Database connection error"); // Both match
    auto entry2 = createLogEntry(2, "app.log", now, LogLevel::ERROR, "Network error"); // Level matches
    auto entry3 = createLogEntry(3, "db.log", now, LogLevel::INFO, "Database backup success"); // Keyword matches
    auto entry4 = createLogEntry(4, "sys.log", now, LogLevel::DEBUG, "Debug message"); // Neither match

    EXPECT_TRUE(or_filter.matches(entry1));
    EXPECT_TRUE(or_filter.matches(entry2));
    EXPECT_TRUE(or_filter.matches(entry3));
    EXPECT_FALSE(or_filter.matches(entry4));
}

TEST_F(FilterTest, CompositeFilterEmpty) {
    CompositeFilter empty_and_filter(CompositeFilter::Logic::AND);
    EXPECT_TRUE(empty_and_filter.matches(createLogEntry(1, "any.log", now, LogLevel::INFO, "any"))); // Vacuously true

    CompositeFilter empty_or_filter(CompositeFilter::Logic::OR);
    EXPECT_FALSE(empty_or_filter.matches(createLogEntry(1, "any.log", now, LogLevel::INFO, "any"))); // Vacuously false
}

TEST_F(FilterTest, ExclusionFilter) {
    auto info_level_filter = std::make_shared<LevelFilter>(LogLevel::INFO);
    auto not_info_filter = std::make_shared<ExclusionFilter>(info_level_filter); // Create as shared_ptr directly

    auto entry_info = createLogEntry(1, "app.log", now, LogLevel::INFO, "Info message");
    auto entry_error = createLogEntry(2, "app.log", now, LogLevel::ERROR, "Error message");

    EXPECT_FALSE(not_info_filter->matches(entry_info)); // Use -> for shared_ptr
    EXPECT_TRUE(not_info_filter->matches(entry_error)); // Use -> for shared_ptr

    // Combine with another filter
    CompositeFilter and_filter(CompositeFilter::Logic::AND);
    and_filter.add(std::make_shared<SourceFileFilter>("app.log", PatternType::Literal));
    and_filter.add(not_info_filter); // Now this is a shared_ptr<ExclusionFilter> which implicitly converts to shared_ptr<IFilter>

    EXPECT_FALSE(and_filter.matches(entry_info)); // app.log AND NOT INFO -> false
    EXPECT_TRUE(and_filter.matches(entry_error)); // app.log AND NOT INFO -> true
    EXPECT_FALSE(and_filter.matches(createLogEntry(3, "other.log", now, LogLevel::ERROR, "Other log error")));
}

TEST_F(FilterTest, ExclusionFilterOfComposite) {
    CompositeFilter or_filter(CompositeFilter::Logic::OR);
    or_filter.add(std::make_shared<LevelFilter>(LogLevel::INFO));
    or_filter.add(std::make_shared<LevelFilter>(LogLevel::DEBUG));
    auto not_info_or_debug = std::make_shared<ExclusionFilter>(std::make_shared<CompositeFilter>(or_filter));

    auto entry_info = createLogEntry(1, "app.log", now, LogLevel::INFO, "Info message");
    auto entry_debug = createLogEntry(2, "app.log", now, LogLevel::DEBUG, "Debug message");
    auto entry_warning = createLogEntry(3, "app.log", now, LogLevel::WARNING, "Warning message");

    EXPECT_FALSE(not_info_or_debug->matches(entry_info));
    EXPECT_FALSE(not_info_or_debug->matches(entry_debug));
    EXPECT_TRUE(not_info_or_debug->matches(entry_warning));
}
