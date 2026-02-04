#include <gtest/gtest.h>
#include "filter/Filter.h"
#include "core/LogTypes.h"
#include <chrono>
#include <map>
#include <sstream> // Required for std::stringstream and std::get_time
#include <nlohmann/json.hpp> // New include for JSON testing (might be needed for some types)

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
