#include <gtest/gtest.h>
#include "core/LogParser.h"
#include "core/LogTypes.h"
#include "core/Error.h"
#include "config/CLIConfig.h"
#include <sstream>
#include <memory>
#include <iostream> // Already present.

using namespace ErrorCode;

// Test fixture for LogParser, if needed. For now, a simple TEST is enough.

TEST(LogParserErrorHandling, VariousActions) {
    std::string pattern = R"(^(\d{4}-\d{2}-\d{2})\s+\[(\w+)\]\s+(.*)$)"; // Pattern for date only, time part will fail
    std::vector<FieldMapping> mappings = {
        {LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S"}, // Expecting full datetime, but only date captured
        {LogEntryField::LEVEL, 2},
        {LogEntryField::MESSAGE, 3}
    };
    std::string logLine = "2023-10-27 [INFO] This log has a bad timestamp format.";
    size_t lineNumber = 1;
    std::string sourceFile = "test_timestamp.log";

    // --- Test Warn action ---
    auto warnParserResult = DefaultLogParser::create(pattern, mappings, {}, std::nullopt, CLIConfig::ParserErrorAction::Warn);
    ASSERT_TRUE(warnParserResult.has_value()) << warnParserResult.error().message; // Fixed .message()
    std::unique_ptr<ILogParser> warnParser = std::move(warnParserResult.value());

    std::stringstream cerrBufferWarn;
    std::streambuf* oldCerrWarn = std::cerr.rdbuf(cerrBufferWarn.rdbuf());

    Result<LogEntry> warnEntryResult = warnParser->parseLine(logLine, lineNumber, sourceFile);
    ASSERT_TRUE(warnEntryResult.has_value()); // Should return a default entry
    ASSERT_FALSE(warnEntryResult.value().timestamp.has_value()); // Timestamp should be empty
    ASSERT_EQ(warnEntryResult.value().level, LogLevel::INFO); // Other fields should still be parsed
    ASSERT_EQ(warnEntryResult.value().message, "This log has a bad timestamp format."); // Message field was captured
    ASSERT_NE(cerrBufferWarn.str().find("Warning (LogParser): Failed to parse timestamp"), std::string::npos); // Warning should be logged

    std::cerr.rdbuf(oldCerrWarn); // Restore cerr

    // --- Test Throw action ---
    auto throwParserResult = DefaultLogParser::create(pattern, mappings, {}, std::nullopt, CLIConfig::ParserErrorAction::Throw);
    ASSERT_TRUE(throwParserResult.has_value()) << throwParserResult.error().message; // Fixed .message()
    std::unique_ptr<ILogParser> throwParser = std::move(throwParserResult.value());

    ASSERT_THROW(throwParser->parseLine(logLine, lineNumber, sourceFile), Error); // Should throw Error

    // --- Test Ignore action ---
    auto ignoreParserResult = DefaultLogParser::create(pattern, mappings, {}, std::nullopt, CLIConfig::ParserErrorAction::Ignore);
    ASSERT_TRUE(ignoreParserResult.has_value()) << ignoreParserResult.error().message; // Fixed .message()
    std::unique_ptr<ILogParser> ignoreParser = std::move(ignoreParserResult.value());

    std::stringstream cerrBufferIgnore;
    std::streambuf* oldCerrIgnore = std::cerr.rdbuf(cerrBufferIgnore.rdbuf());

    Result<LogEntry> ignoreEntryResult = ignoreParser->parseLine(logLine, lineNumber, sourceFile);
    ASSERT_TRUE(ignoreEntryResult.has_value()); // Should return a default entry
    ASSERT_FALSE(ignoreEntryResult.value().timestamp.has_value()); // Timestamp should be empty
    ASSERT_EQ(ignoreEntryResult.value().level, LogLevel::INFO);
    ASSERT_EQ(ignoreEntryResult.value().message, "Parse ignored: " + logLine); // Message indicates ignore action
    ASSERT_EQ(cerrBufferIgnore.str().find("Warning (LogParser): Failed to parse timestamp"), std::string::npos); // No warning should be logged

    std::cerr.rdbuf(oldCerrIgnore); // Restore cerr
}

// Test invalid main regex pattern
TEST(LogParserTest, InvalidMainRegexPattern) {
    std::string invalidPattern = R"([)"; // Invalid regex pattern
    std::vector<FieldMapping> mappings = {};

    auto parserResult = DefaultLogParser::create(invalidPattern, mappings);
    ASSERT_FALSE(parserResult.has_value());
    ASSERT_EQ(parserResult.error().code, Code::InvalidRegex); // Fixed: .code() -> .code
    ASSERT_NE(parserResult.error().message.find("Invalid log pattern"), std::string::npos);
}

// Test invalid logEntryStartPattern regex
TEST(LogParserTest, InvalidLogEntryStartRegexPattern) {
    std::string pattern = R"(^(\d+) (.*)$)";
    std::vector<FieldMapping> mappings = {};
    std::string invalidLogEntryStartPattern = R"([)"; // Invalid regex

    auto parserResult = DefaultLogParser::create(pattern, mappings, {}, invalidLogEntryStartPattern);
    ASSERT_FALSE(parserResult.has_value());
    ASSERT_EQ(parserResult.error().code, Code::InvalidRegex); // Fixed: .code() -> .code
    ASSERT_NE(parserResult.error().message.find("Invalid log entry start pattern"), std::string::npos);
}

// Test structured field parsing with custom delimiters
TEST(LogParserTest, StructuredFieldCustomDelimiters) {
    std::string pattern = R"(^LOG: (.*)$)";
    std::vector<FieldMapping> mappings = {
        FieldMapping(LogEntryField::STRUCTURED_FIELD, 1, std::vector<std::string>{":"}), // Colon delimiter
    };
    // FIX: Declare and initialize parserResult, and ensure parser is declared.
    auto parserResult = DefaultLogParser::create(pattern, mappings); // Empty mappings for this test
    ASSERT_TRUE(parserResult.has_value()) << parserResult.error().message;
    std::unique_ptr<ILogParser> parser = std::move(parserResult.value());

    std::string logLine = "LOG: user_id:123, status:active, task:do_something_important";
    Result<LogEntry> result = parser->parseLine(logLine, 1, "test.log");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().customFields.size(), 3);
    ASSERT_EQ(result.value().customFields["user_id"], "123");
    ASSERT_EQ(result.value().customFields["status"], "active");
    ASSERT_EQ(result.value().customFields["task"], "do_something_important");
}

// Test structured field parsing with quoted values and special characters
TEST(LogParserTest, StructuredFieldQuotedValuesAndSpecialChars) {
    std::string pattern = R"(^DATA: (.*)$)";
    std::vector<FieldMapping> mappings = {
        FieldMapping(LogEntryField::STRUCTURED_FIELD, 1, std::vector<std::string>{"="}),
    };

    // FIX: Declare and initialize parserResult, and ensure parser is declared.
    auto parserResult = DefaultLogParser::create(pattern, mappings);
    ASSERT_TRUE(parserResult.has_value()) << parserResult.error().message;
    std::unique_ptr<ILogParser> parser = std::move(parserResult.value());

    std::string logLine = R"(DATA: item="Apples & Pears", price=1.99, desc='fresh fruit (seasonal)', qty=10)";
    Result<LogEntry> result = parser->parseLine(logLine, 1, "test.log");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().customFields.size(), 4);
    ASSERT_EQ(result.value().customFields["item"], "Apples & Pears");
    ASSERT_EQ(result.value().customFields["price"], "1.99");
    ASSERT_EQ(result.value().customFields["desc"], "fresh fruit (seasonal)");
    ASSERT_EQ(result.value().customFields["qty"], "10");

    // Test with values containing spaces without quotes (should capture only up to next delimiter/space)
    std::string logLine2 = R"(DATA: item=Banana Split, topping="Chocolate Sauce")";
    Result<LogEntry> result2 = parser->parseLine(logLine2, 2, "test.log");
    ASSERT_TRUE(result2.has_value());
    ASSERT_EQ(result2.value().customFields.size(), 2);
    ASSERT_EQ(result2.value().customFields["item"], "Banana"); // Expected behavior for unquoted spaces
    ASSERT_EQ(result2.value().customFields["topping"], "Chocolate Sauce");
}

// Comprehensive Multi-line Log Entry Scenarios
// Test 1: A log file with only a single log entry (no logEntryStartPattern encountered after the first line).
TEST(LogParserTest, MultiLineSingleEntryFile) {
    std::string pattern = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) (\w+): ([\s\S]*)$)";
    std::vector<FieldMapping> mappings = {
        {LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S"},
        {LogEntryField::LEVEL, 2},
        {LogEntryField::MESSAGE, 3}
    };
    std::optional<std::string> logEntryStartPattern = R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})";

    auto parserResult = DefaultLogParser::create(pattern, mappings, {}, logEntryStartPattern);
    ASSERT_TRUE(parserResult.has_value()) << parserResult.error().message;
    std::unique_ptr<ILogParser> parser = std::move(parserResult.value());

    std::optional<Result<LogEntry>> res1 = parser->processLine("2023-01-01 10:00:00 INFO: First line", 1, "single_entry.log");
    ASSERT_FALSE(res1.has_value()); // Should buffer

    std::optional<Result<LogEntry>> res2 = parser->processLine("  Second line, part of first entry.", 2, "single_entry.log");
    ASSERT_FALSE(res2.has_value()); // Should buffer

    std::vector<Result<LogEntry>> flushed = parser->flushRemaining(); // No new entry starts, so flush
    ASSERT_EQ(flushed.size(), 1);
    ASSERT_TRUE(flushed[0].has_value());
    ASSERT_EQ(flushed[0].value().level, LogLevel::INFO);
    ASSERT_EQ(flushed[0].value().message, "First line\n  Second line, part of first entry.");
    ASSERT_EQ(flushed[0].value().sourceLineNumber, 1);
    ASSERT_EQ(flushed[0].value().sourceFile, "single_entry.log");
}

// Test 2: A log file where the last entry requires flushRemaining. (Covered by MultiLineSingleEntryFile and MultiLineBasic)

// Test 3: Multi-line entries containing blank lines or lines that might look like a logEntryStartPattern but aren't
TEST(LogParserTest, MultiLineWithBlankAndAmbiguousLines) {
    std::string pattern = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) (\w+): ([\s\S]*)$)";
    std::vector<FieldMapping> mappings = {
        {LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S"},
        {LogEntryField::LEVEL, 2},
        {LogEntryField::MESSAGE, 3}
    };
    std::optional<std::string> logEntryStartPattern = R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} \w+:)"; // More specific start pattern

    auto parserResult = DefaultLogParser::create(pattern, mappings, {}, logEntryStartPattern);
    ASSERT_TRUE(parserResult.has_value()) << parserResult.error().message;
    std::unique_ptr<ILogParser> parser = std::move(parserResult.value());

    std::optional<Result<LogEntry>> res;

    // Line 1: Starts a new entry
    res = parser->processLine("2023-01-01 10:00:00 INFO: First actual entry.", 1, "multi_line_complex.log");
    ASSERT_FALSE(res.has_value());

    // Line 2: Blank line, should be part of the current entry
    res = parser->processLine("", 2, "multi_line_complex.log");
    ASSERT_FALSE(res.has_value());

    // Line 3: Looks like a timestamp but no level, so not a new entry
    res = parser->processLine("2023-01-01 10:00:00 This is a continuation.", 3, "multi_line_complex.log");
    ASSERT_FALSE(res.has_value());

    // Line 4: New entry starts
    res = parser->processLine("2023-01-01 10:00:01 DEBUG: Second entry starts here.", 4, "multi_line_complex.log");
    ASSERT_TRUE(res.has_value());
    ASSERT_TRUE(res.value().has_value());
    ASSERT_EQ(res.value().value().level, LogLevel::INFO);
    ASSERT_EQ(res.value().value().message, "First actual entry.\n\n2023-01-01 10:00:00 This is a continuation.");
    ASSERT_EQ(res.value().value().sourceLineNumber, 1);
    ASSERT_EQ(res.value().value().sourceFile, "multi_line_complex.log");

    // Line 5: Another continuation
    res = parser->processLine("  Further details for second entry.", 5, "multi_line_complex.log");
    ASSERT_FALSE(res.has_value());

    // Flush remaining
    std::vector<Result<LogEntry>> flushed = parser->flushRemaining();
    ASSERT_EQ(flushed.size(), 1);
    ASSERT_TRUE(flushed[0].has_value());
    ASSERT_EQ(flushed[0].value().level, LogLevel::DEBUG);
    ASSERT_EQ(flushed[0].value().message, "Second entry starts here.\n  Further details for second entry.");
    ASSERT_EQ(flushed[0].value().sourceLineNumber, 4);
    ASSERT_EQ(flushed[0].value().sourceFile, "multi_line_complex.log");
}

// Test 4: Interleaving of single-line and multi-line entries (partially covered by MultiLineBasic, adding more explicit case)
TEST(LogParserTest, MultiLineInterleaving) {
    std::string pattern = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) (\w+): ([\s\S]*)$)";
    std::vector<FieldMapping> mappings = {
        {LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S"},
        {LogEntryField::LEVEL, 2},
        {LogEntryField::MESSAGE, 3}
    };
    std::optional<std::string> logEntryStartPattern = R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} \w+:)";

    auto parserResult = DefaultLogParser::create(pattern, mappings, {}, logEntryStartPattern);
    ASSERT_TRUE(parserResult.has_value()) << parserResult.error().message;
    std::unique_ptr<ILogParser> parser = std::move(parserResult.value());

    std::optional<Result<LogEntry>> res;
    std::vector<Result<LogEntry>> parsedEntries;

    // Entry 1 (multi-line)
    res = parser->processLine("2023-01-01 09:00:00 INFO: Multi-line entry 1, line 1", 1, "interleaving.log");
    ASSERT_FALSE(res.has_value());
    res = parser->processLine("  Multi-line entry 1, line 2", 2, "interleaving.log");
    ASSERT_FALSE(res.has_value());

    // Entry 2 (single-line, ends multi-line entry 1)
    res = parser->processLine("2023-01-01 09:00:01 DEBUG: Single-line entry 2.", 3, "interleaving.log");
    ASSERT_TRUE(res.has_value());
    parsedEntries.push_back(std::move(res.value())); // Entry 1 is now available
    ASSERT_EQ(parsedEntries.back().value().message, "Multi-line entry 1, line 1\n  Multi-line entry 1, line 2");

    // Entry 3 (multi-line)
    res = parser->processLine("2023-01-01 09:00:02 WARN: Multi-line entry 3, line 1", 4, "interleaving.log");
    ASSERT_TRUE(res.has_value());
    parsedEntries.push_back(std::move(res.value())); // Entry 2 is now available
    ASSERT_EQ(parsedEntries.back().value().message, "Single-line entry 2.");

    res = parser->processLine("  Multi-line entry 3, line 2", 5, "interleaving.log");
    ASSERT_FALSE(res.has_value());

    // Flush remaining
    std::vector<Result<LogEntry>> flushed = parser->flushRemaining();
    ASSERT_EQ(flushed.size(), 1);
    parsedEntries.push_back(std::move(flushed[0])); // Entry 3 is now available

    ASSERT_EQ(parsedEntries.size(), 3);

    ASSERT_EQ(parsedEntries[0].value().level, LogLevel::INFO);
    ASSERT_EQ(parsedEntries[0].value().sourceLineNumber, 1);

    ASSERT_EQ(parsedEntries[1].value().level, LogLevel::DEBUG);
    ASSERT_EQ(parsedEntries[1].value().sourceLineNumber, 3);

    ASSERT_EQ(parsedEntries[2].value().level, LogLevel::WARNING);
    ASSERT_EQ(parsedEntries[2].value().message, "Multi-line entry 3, line 1\n  Multi-line entry 3, line 2");
    ASSERT_EQ(parsedEntries[2].value().sourceLineNumber, 4);
}

// Test 5: Buffer Limit Exceeded
TEST(LogParserTest, BufferLimitExceeded) {
    std::string pattern = R"(^(\d{4}-\d{2}-\d{2}) (.*)$)";
    std::vector<FieldMapping> mappings = {
        {LogEntryField::TIMESTAMP, 1, "%Y-%m-%d"},
        {LogEntryField::MESSAGE, 2}
    };
    std::optional<std::string> logEntryStartPattern = R"(^\d{4}-\d{2}-\d{2})";

    // Create parser with very small buffer (e.g., 20 bytes)
    auto parserResult = DefaultLogParser::create(
        pattern, 
        mappings, 
        {}, 
        logEntryStartPattern, 
        CLIConfig::ParserErrorAction::Warn, 
        20 // Small buffer size
    );
    ASSERT_TRUE(parserResult.has_value());
    std::unique_ptr<ILogParser> parser = std::move(parserResult.value());

    // Line 1: 10 chars. Buffer = 10. OK.
    auto res1 = parser->processLine("2023-01-01", 1, "buffer_test.log");
    ASSERT_FALSE(res1.has_value());

    // Line 2: 12 chars. Buffer would be 10 + 1 (\n) + 12 = 23 > 20. Limit Exceeded.
    auto res2 = parser->processLine("Limit Exceed", 2, "buffer_test.log");
    ASSERT_TRUE(res2.has_value());
    ASSERT_TRUE(res2.value().has_value()); // It returns a LogEntry, but with parsing errors
    
    LogEntry errorEntry = res2.value().value();
    ASSERT_TRUE(errorEntry.hasParsingErrors());
    ASSERT_EQ(errorEntry.parsingErrors[0].code, Code::BufferLimitExceeded);
    ASSERT_EQ(errorEntry.sourceLineNumber, 1); // Error refers to the start of the buffered entry

    // Verify recovery: parser should now have the second line buffered
    ASSERT_EQ(parser->getCurrentBufferedLineCount(), 1);
    ASSERT_EQ(parser->getCurrentBufferedContent(), "Limit Exceed");

    // Flush remaining
    auto flushed = parser->flushRemaining();
    ASSERT_EQ(flushed.size(), 1);
    // The flushed entry "Limit Exceed" won't match the pattern (no date), so it will be a parse error (or raw message depending on error action)
    // In this case, since it's "Limit Exceed" and pattern expects date, it's a regex mismatch.
    // DefaultLogParser::parseLine returns a default entry with message "Parse failed (warn)..." on mismatch if Warn
    ASSERT_TRUE(flushed[0].has_value());
    ASSERT_NE(flushed[0].value().message.find("Parse failed"), std::string::npos);
}

// Test 6: Stream Processing
TEST(LogParserTest, StreamProcessing) {
    std::string pattern = R"(^(\d{4}-\d{2}-\d{2}) ([\s\S]*)$)";
    std::vector<FieldMapping> mappings = {
        {LogEntryField::TIMESTAMP, 1, "%Y-%m-%d"},
        {LogEntryField::MESSAGE, 2}
    };
    std::optional<std::string> logEntryStartPattern = R"(^\d{4}-\d{2}-\d{2})";

    auto parserResult = DefaultLogParser::create(pattern, mappings, {}, logEntryStartPattern);
    ASSERT_TRUE(parserResult.has_value());
    std::unique_ptr<ILogParser> parser = std::move(parserResult.value());

    std::stringstream ss;
    ss << "2023-01-01 Message 1\n";
    ss << "2023-01-02 Message 2 line 1\n";
    ss << "  Message 2 line 2\n";
    ss << "2023-01-03 Message 3";

    std::vector<LogEntry> entries;
    parser->processStream(ss, [&](Result<LogEntry> result) {
        if (result.has_value()) {
            entries.push_back(result.value());
        }
    });

    ASSERT_EQ(entries.size(), 3);
    ASSERT_EQ(entries[0].message, "Message 1");
    ASSERT_EQ(entries[1].message, "Message 2 line 1\n  Message 2 line 2");
    ASSERT_EQ(entries[2].message, "Message 3");
}

// Test 7: State Introspection
TEST(LogParserTest, StateIntrospectionDuringMultiLine) {
    std::string pattern = R"(^(\d{4}-\d{2}-\d{2}) (.*)$)";
    std::vector<FieldMapping> mappings = {
        {LogEntryField::TIMESTAMP, 1, "%Y-%m-%d"},
        {LogEntryField::MESSAGE, 2}
    };
    std::optional<std::string> logEntryStartPattern = R"(^\d{4}-\d{2}-\d{2})";

    auto parserResult = DefaultLogParser::create(pattern, mappings, {}, logEntryStartPattern);
    ASSERT_TRUE(parserResult.has_value());
    std::unique_ptr<ILogParser> parser = std::move(parserResult.value());

    // Verify initial state
    ASSERT_EQ(parser->getCurrentBufferedLineCount(), 0);
    ASSERT_TRUE(parser->getCurrentBufferedContent().empty());
    ASSERT_EQ(parser->getPatternString(), pattern);
    ASSERT_EQ(parser->getFieldMappings().size(), 2);
    ASSERT_EQ(parser->getLogEntryStartPatternString().value(), logEntryStartPattern.value());

    // Start multi-line entry
    parser->processLine("2023-01-01 Line 1", 1, "intro.log");
    ASSERT_EQ(parser->getCurrentBufferedLineCount(), 1);
    ASSERT_EQ(parser->getCurrentBufferedContent(), "2023-01-01 Line 1");

    // Add continuation line
    parser->processLine("  Line 2", 2, "intro.log");
    ASSERT_EQ(parser->getCurrentBufferedLineCount(), 2);
    ASSERT_EQ(parser->getCurrentBufferedContent(), "2023-01-01 Line 1\n  Line 2");

    // Finish entry (start new one)
    auto res = parser->processLine("2023-01-02 Next Entry", 3, "intro.log");
    ASSERT_TRUE(res.has_value());
    
    // Check state after processing (now contains new entry)
    ASSERT_EQ(parser->getCurrentBufferedLineCount(), 1);
    ASSERT_EQ(parser->getCurrentBufferedContent(), "2023-01-02 Next Entry");
}
