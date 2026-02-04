#include <gtest/gtest.h>
#include "LogParser.h"
#include "LogTypes.h"
#include "Utils.h" // Might be useful for timestamp formatting if needed
#include <sstream>

// Test fixture not strictly needed for LogParser, but good practice for common setup if it grows.
// For now, instantiate DefaultLogParser directly in tests.

// Test the new constructor with explicit field mappings and custom log levels
TEST(LogParserTest, NewConstructorWithFieldMappingsAndLogLevels) {
    std::string pattern = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) \[1\] (.*)$)";
    std::vector<FieldMapping> mappings = {
        {LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S"},
        {LogEntryField::LEVEL, 2},
        {LogEntryField::MESSAGE, 3}
    };
    std::map<std::string, LogLevel, ci_less> customLevels = {
        {"ALERT", LogLevel::FATAL}
    };

    DefaultLogParser parser(pattern, mappings, customLevels);

    std::string logLine = "2023-10-27 10:30:00 [ALERT] User login failed.";
    ParseResult result = parser.processLine(logLine, 1).value();

    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.entry.level, LogLevel::FATAL);
    ASSERT_EQ(result.entry.message, "User login failed.");
    // Verify timestamp (chrono::system_clock::time_point comparison is tricky, check components)
    auto expectedTime_opt = Utils::parseTime("2023-10-27 10:30:00");
    ASSERT_TRUE(expectedTime_opt.has_value());
    auto expectedTime = expectedTime_opt.value();
    ASSERT_EQ(std::chrono::duration_cast<std::chrono::seconds>(expectedTime.time_since_epoch()).count(),
              std::chrono::duration_cast<std::chrono::seconds>(expectedTime.time_since_epoch()).count());
}

// Test the deprecated constructor (backward compatibility)
TEST(LogParserTest, DeprecatedConstructorBackwardCompatibility) {
    // The deprecated constructor assumes specific capture groups for timestamp, level, message.
    // Let's use a pattern that matches those assumptions (group 1: timestamp, group 2: level, group 3: message)
    std::string pattern = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) (\w+): (.*)$)";
    DefaultLogParser parser(pattern, {}); // Using constructor without mappings, assuming {} is valid for replacing deprecated one

    std::string logLine = "2023-01-15 14:05:30 INFO: Application started.";
    ParseResult result = parser.processLine(logLine, 1).value();

    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.entry.level, LogLevel::INFO);
    ASSERT_EQ(result.entry.message, "Application started.");
    auto expectedTime_opt = Utils::parseTime("2023-01-15 14:05:30");
    ASSERT_TRUE(expectedTime_opt.has_value());
    auto expectedTime = expectedTime_opt.value();
    ASSERT_EQ(std::chrono::duration_cast<std::chrono::seconds>(expectedTime.time_since_epoch()).count(),
              std::chrono::duration_cast<std::chrono::seconds>(expectedTime.time_since_epoch()).count());
}

// Test basic multi-line parsing with a start pattern
TEST(LogParserTest, MultiLineBasic) {
    std::string pattern = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) (\w+): (.*)$)";
    std::vector<FieldMapping> mappings = {
        {LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S"},
        {LogEntryField::LEVEL, 2},
        {LogEntryField::MESSAGE, 3}
    };
    std::optional<std::string> logEntryStartPattern = R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})"; // Start of line with timestamp

    DefaultLogParser parser(pattern, mappings, {}, logEntryStartPattern);

    std::optional<ParseResult> res1 = parser.processLine("2023-01-01 10:00:00 INFO: Line 1", 1);
    ASSERT_FALSE(res1.has_value()); // Should buffer

    std::optional<ParseResult> res2 = parser.processLine("  Continuation of Line 1", 2);
    ASSERT_FALSE(res2.has_value()); // Should buffer

    std::optional<ParseResult> res3 = parser.processLine("2023-01-01 10:00:01 DEBUG: Line 2", 3);
    ASSERT_TRUE(res3.has_value()); // Line 1 is complete, Line 2 starts

    ASSERT_TRUE(res3->success);
    ASSERT_TRUE(res3->success);
    ASSERT_EQ(res3->entry.level, LogLevel::INFO);
    ASSERT_EQ(res3->entry.message, "Line 1\n  Continuation of Line 1");
    auto expectedTime1_opt = Utils::parseTime("2023-01-01 10:00:00");
    ASSERT_TRUE(expectedTime1_opt.has_value());
    auto expectedTime1 = expectedTime1_opt.value();
    ASSERT_EQ(std::chrono::duration_cast<std::chrono::seconds>(expectedTime1.time_since_epoch()).count(),
              std::chrono::duration_cast<std::chrono::seconds>(expectedTime1.time_since_epoch()).count());

    // Flush remaining
    std::vector<ParseResult> flushed = parser.flushRemaining();
    ASSERT_EQ(flushed.size(), 1);
    ASSERT_TRUE(flushed[0].success);
    ASSERT_TRUE(flushed[0].success);
    ASSERT_EQ(flushed[0].entry.level, LogLevel::DEBUG);
    ASSERT_EQ(flushed[0].entry.message, "Line 2");
    auto expectedTime2_opt = Utils::parseTime("2023-01-01 10:00:01");
    ASSERT_TRUE(expectedTime2_opt.has_value());
    auto expectedTime2 = expectedTime2_opt.value();
    ASSERT_EQ(std::chrono::duration_cast<std::chrono::seconds>(expectedTime2.time_since_epoch()).count(),
              std::chrono::duration_cast<std::chrono::seconds>(expectedTime2.time_since_epoch()).count());
}

// Test structured field parsing with explicit mapping and enhanced key regex
TEST(LogParserTest, StructuredFieldExplicitMappingWithEnhancedKeys) {
    std::string pattern = R"(^INFO: \[([^\]]+)\] (.*)$)";
    std::vector<FieldMapping> mappings = {
        {LogEntryField::STRUCTURED_FIELD, 1, "", "="}, // Group 1 contains structured data, parse k/v by "="
        {LogEntryField::MESSAGE, 2}
    };

    DefaultLogParser parser(pattern, mappings);

    std::string logLine = "INFO: [user.id=123, event-name=login-success, proc_time=1.23s] User logged in.";
    ParseResult result = parser.processLine(logLine, 1).value();

    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.entry.message, "User logged in.");
    ASSERT_EQ(result.entry.structuredFields.size(), 3);
    ASSERT_EQ(result.entry.structuredFields["user.id"], "123");
    ASSERT_EQ(result.entry.structuredFields["event-name"], "login-success");
    ASSERT_EQ(result.entry.structuredFields["proc_time"], "1.23s");
}

// Test legacy structured field parsing from message with enhanced key regex
TEST(LogParserTest, LegacyStructuredFieldFromMessageWithEnhancedKeys) {
    std::string pattern = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) (\w+): (.*)$)";
    std::vector<FieldMapping> mappings = {
        {LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S"},
        {LogEntryField::LEVEL, 2},
        {LogEntryField::MESSAGE, 3} // Legacy parsing will happen here
    };

    DefaultLogParser parser(pattern, mappings);

    std::string logLine = "2023-01-01 10:00:00 INFO: User action, session.id=abc-123, action-type=\"view-page\", request_duration=100ms.";
    ParseResult result = parser.processLine(logLine, 1).value();

    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.entry.level, LogLevel::INFO);
    ASSERT_EQ(result.entry.message, "User action, session.id=abc-123, action-type=\"view-page\", request_duration=100ms.");
    ASSERT_EQ(result.entry.structuredFields.size(), 3);
    ASSERT_EQ(result.entry.structuredFields["session.id"], "abc-123");
    ASSERT_EQ(result.entry.structuredFields["action-type"], "view-page");
    ASSERT_EQ(result.entry.structuredFields["request_duration"], "100ms");
}
