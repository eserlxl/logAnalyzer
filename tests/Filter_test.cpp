#include <gtest/gtest.h>
#include "../include/Filter.h"
#include "../include/LogTypes.h"
#include <chrono>
#include <map>
#include <sstream> // Required for std::stringstream and std::get_time

// Test fixture for creating LogEntry objects
class FilterTest : public ::testing::Test {
protected:
    LogEntry createLogEntry(
        size_t id,
        const std::string& sourceFile,
        std::chrono::system_clock::time_point timestamp,
        LogLevel level,
        const std::string& message,
        const std::map<std::string, std::string>& structuredFields = {}
    ) {
        LogEntry entry;
        entry.id = id;
        entry.sourceFile = sourceFile;
        entry.timestamp = timestamp;
        entry.level = level;
        entry.message = message;
        entry.structuredFields = structuredFields;
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
    SourceFileFilter filter("*.log", PatternType::Glob);
    auto entry1 = createLogEntry(1, "server.log", now, LogLevel::INFO, "Server started");
    auto entry2 = createLogEntry(2, "client.log", now, LogLevel::INFO, "Client started");
    auto entry3 = createLogEntry(3, "server.txt", now, LogLevel::INFO, "Server config");
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_TRUE(filter.matches(entry2));
    EXPECT_FALSE(filter.matches(entry3));

    SourceFileFilter filter2("server??.log", PatternType::Glob);
    auto entry4 = createLogEntry(4, "server01.log", now, LogLevel::INFO, "Server 01");
    auto entry5 = createLogEntry(5, "server.log", now, LogLevel::INFO, "Server");
    EXPECT_TRUE(filter2.matches(entry4));
    EXPECT_FALSE(filter2.matches(entry5));
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


TEST_F(FilterTest, FieldValueFilterRegex) {
    // Default caseSensitive is false, so it should be case-insensitive
    FieldValueFilter filter_regex("error_code", R"(ERR\d{3})", PatternType::Regex);
    auto entry3 = createLogEntry(3, "app.log", now, LogLevel::ERROR, "DB error", {{"error_code", "ERR501"}});
    auto entry4 = createLogEntry(4, "app.log", now, LogLevel::ERROR, "Network error", {{"error_code", "err200"}});
    EXPECT_TRUE(filter_regex.matches(entry3)); // ERR501 matches ERR\d{3} case-insensitively
    EXPECT_TRUE(filter_regex.matches(entry4)); // err200 matches ERR\d{3} case-insensitively

    FieldValueFilter filter_regex_icase_explicit("error_code", R"(ERR\d{3})", PatternType::Regex, false);
    EXPECT_TRUE(filter_regex_icase_explicit.matches(entry3)); // ERR501 matches ERR\d{3} case-insensitively
    EXPECT_TRUE(filter_regex_icase_explicit.matches(entry4)); // err200 matches ERR\d{3} case-insensitively
}

TEST_F(FilterTest, FieldValueFilterRegexCaseSensitive) {
    FieldValueFilter filter_regex_cs("error_code", R"(ERR\d{3})", PatternType::Regex, true); // Explicitly case sensitive
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
    RegexFilter filter("Error", true); // Should be case sensitive, so "error" should NOT match "Error"
    auto entry1 = createLogEntry(1, "app.log", now, LogLevel::INFO, "error found");
    auto entry2 = createLogEntry(2, "app.log", now, LogLevel::INFO, "Error found");
    EXPECT_FALSE(filter.matches(entry1));
    EXPECT_TRUE(filter.matches(entry2));

    RegexFilter filter_ctor2("[Ee]rror", true); // This regex itself handles both cases, but filter is case sensitive
    EXPECT_TRUE(filter_ctor2.matches(entry1)); // Should match 'error'
    EXPECT_TRUE(filter_ctor2.matches(entry2)); // Should match 'Error'
}

TEST_F(FilterTest, RegexFilterCaseInsensitive) {
    RegexFilter filter("error", false); // Should be case insensitive
    auto entry1 = createLogEntry(1, "app.log", now, LogLevel::INFO, "error found");
    auto entry2 = createLogEntry(2, "app.log", now, LogLevel::INFO, "Error found");
    auto entry3 = createLogEntry(3, "app.log", now, LogLevel::INFO, "No issues");
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_TRUE(filter.matches(entry2));
    EXPECT_FALSE(filter.matches(entry3));
}

TEST_F(FilterTest, TimeRangeFilterFromStrings) {
    auto filter_res = TimeRangeFilter::fromStrings("2023-01-15 10:00:00", "2023-01-15 11:00:00");
    ASSERT_TRUE(filter_res.has_value());
    auto filter = filter_res.value();
    
    std::tm tm = {};
    std::stringstream ss("2023-01-15 10:30:00");
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto time_in = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    std::stringstream ss2("2023-01-15 12:00:00");
    ss2 >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto time_out = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    auto entry1 = createLogEntry(1, "time.log", time_in, LogLevel::INFO, "In time");
    auto entry2 = createLogEntry(2, "time.log", time_out, LogLevel::INFO, "Out of time");

    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2));
}

TEST_F(FilterTest, TimeRangeFilterSince) {
    auto filter_res = TimeRangeFilter::since("5m ago");
    ASSERT_TRUE(filter_res.has_value());
    auto filter = filter_res.value();
    
    auto time_in = std::chrono::system_clock::now() - std::chrono::minutes(2);
    auto time_out = std::chrono::system_clock::now() - std::chrono::minutes(10);

    auto entry1 = createLogEntry(1, "time.log", time_in, LogLevel::INFO, "In time");
    auto entry2 = createLogEntry(2, "time.log", time_out, LogLevel::INFO, "Out of time");

    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
