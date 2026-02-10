// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "gtest/gtest.h"
#include "analyzer/log/writer.h"
#include "core/log/types.h"
#include "filter/types.h"
#include "filter/expression.h"
#include "analyzer/core.h"
#include "utils/time.h" // For Utils::formatTimestamp if needed in tests

#include <sstream>
#include <vector>
#include <optional>
#include <memory>

class MockLogAnalyzer : public LogAnalyzer {
public:
    // Override getFilteredEntries to return predefined results
    ErrorCode::Result<std::vector<LogEntry>> getFilteredEntries(const filter::FilterExpression& /*expression*/) const override {
        // Use the expression to decide what to return, or return a fixed set for simplicity in tests
        if (returnError_) {
            return std::unexpected(ErrorCode::Error(Code::Unknown, "Mock induced error"));
        }
        if (mockEntries_.empty()) {
            // Simulate no entries found
            return std::vector<LogEntry>();
        }
        return mockEntries_;
    }

    // Override createFilterExpressionFromCriteria
    filter::FilterExpression createFilterExpressionFromCriteria(const filter::FilterCriteria& /*criteria*/) const override {
        // For testing LogWriter, the exact FilterExpression doesn't matter as much as the output of getFilteredEntries.
        // Return a dummy expression.
        return filter::FilterExpression();
    }

    void setMockEntries(const std::vector<LogEntry>& entries) {
        mockEntries_ = entries;
        returnError_ = false;
    }

    void setErrorMode(bool setError) {
        returnError_ = setError;
        mockEntries_.clear(); // Clear entries if setting error mode
    }

private:
    std::vector<LogEntry> mockEntries_;
    bool returnError_ = false;
};

// Helper to create a default FormattingOptions
FormattingOptions getDefaultFormattingOptions() {
    FormattingOptions options;
    options.dateTimeFormat = "%Y-%m-%d %H:%M:%S";
    options.useColor = false;
    options.overallFormat = "{timestamp} [{level}] {message}";
    options.includeStructuredFields = false;
    options.structuredFieldDelimiter = ", ";
    options.structuredFieldKvDelimiter = "=";
    return options;
}

// Helper to create a default LogEntry
LogEntry getDefaultLogEntry() {
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = LogLevel::INFO;
    entry.message = "This is a test message.";
    entry.id = 123;
    entry.sourceFile = "LogWriter.cpp";
    entry.sourceLineNumber = 42;
    entry.threadId = "Thread-1";
    entry.module = "Analyzer";
    entry.host = "localhost";
    entry.customFields = {{"key1", "value1"}, {"key2", "value2"}};
    return entry;
}

TEST(LogWriterTest, FormatEntry_Basic) {
    MockLogAnalyzer analyzer;
    LogWriter writer(analyzer);
    LogEntry entry = getDefaultLogEntry();
    FormattingOptions options = getDefaultFormattingOptions();

    // Expected format: "{timestamp} [{level}] {message}"
    std::string formatted = writer.formatEntry(entry, options);

    // Check if timestamp is formatted correctly
    ASSERT_NE(formatted.find(Utils::formatTimestamp(entry.timestamp.value(), options.dateTimeFormat)), std::string::npos);
    ASSERT_NE(formatted.find("[INFO]"), std::string::npos);
    ASSERT_NE(formatted.find("This is a test message."), std::string::npos);
}

TEST(LogWriterTest, FormatEntry_WithColor) {
    MockLogAnalyzer analyzer;
    LogWriter writer(analyzer);
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = LogLevel::ERROR;
    entry.message = "Error message.";
    FormattingOptions options = getDefaultFormattingOptions();
    options.useColor = true;
    options.overallFormat = "{level}: {message}";

    std::string formatted = writer.formatEntry(entry, options);

    // Check for ANSI color codes for RED
    ASSERT_NE(formatted.find("\033[31m"), std::string::npos);
    ASSERT_NE(formatted.find("ERROR"), std::string::npos);
    ASSERT_NE(formatted.find("\033[0m"), std::string::npos);
    ASSERT_NE(formatted.find(": Error message."), std::string::npos);
}

TEST(LogWriterTest, FormatEntry_EmptyOptionalFields) {
    MockLogAnalyzer analyzer;
    LogWriter writer(analyzer);
    LogEntry entry; // Default constructed, many fields will be empty/nullopt
    entry.level = LogLevel::DEBUG;
    entry.message = "Minimal entry.";
    FormattingOptions options = getDefaultFormattingOptions();
    options.overallFormat = "{timestamp} [{level}] {message} {id} {sourceFile} {lineNumber} {threadId} {module} {host} {customFields}";
    options.includeStructuredFields = true; // Ensure custom fields logic is tested even if empty

    std::string formatted = writer.formatEntry(entry, options);

    // Check that missing optional fields are represented by empty strings or "N/A" where applicable
    ASSERT_NE(formatted.find("N/A"), std::string::npos); // for timestamp
    ASSERT_NE(formatted.find("[DEBUG]"), std::string::npos);
    ASSERT_NE(formatted.find("Minimal entry."), std::string::npos);
    ASSERT_NE(formatted.find("  "), std::string::npos); // Empty string for id, sourceLineNumber, threadId, module, host
    ASSERT_NE(formatted.find(""), std::string::npos); // customFields will be empty
}

TEST(LogWriterTest, FormatEntry_WithCustomFields) {
    MockLogAnalyzer analyzer;
    LogWriter writer(analyzer);
    LogEntry entry = getDefaultLogEntry();
    FormattingOptions options = getDefaultFormattingOptions();
    options.overallFormat = "{message} [{customFields}]";
    options.includeStructuredFields = true;
    options.structuredFieldDelimiter = " | ";
    options.structuredFieldKvDelimiter = ": ";

    std::string formatted = writer.formatEntry(entry, options);

    // Expected: "This is a test message. [key1: value1 | key2: value2]"
    ASSERT_NE(formatted.find("This is a test message. [key1: value1 | key2: value2]"), std::string::npos);
}

TEST(LogWriterTest, FormatEntry_EmptyFormatString) {
    MockLogAnalyzer analyzer;
    LogWriter writer(analyzer);
    LogEntry entry = getDefaultLogEntry();
    FormattingOptions options = getDefaultFormattingOptions();
    options.overallFormat = ""; // Empty format string

    std::string formatted = writer.formatEntry(entry, options);
    ASSERT_EQ(formatted, "");
}

TEST(LogWriterTest, FormatEntry_NoPlaceholdersInFormat) {
    MockLogAnalyzer analyzer;
    LogWriter writer(analyzer);
    LogEntry entry = getDefaultLogEntry();
    FormattingOptions options = getDefaultFormattingOptions();
    options.overallFormat = "Just a plain string."; // No placeholders

    std::string formatted = writer.formatEntry(entry, options);
    ASSERT_EQ(formatted, "Just a plain string.");
}

TEST(LogWriterTest, FormatEntry_RepeatedPlaceholders) {
    MockLogAnalyzer analyzer;
    LogWriter writer(analyzer);
    LogEntry entry = getDefaultLogEntry();
    FormattingOptions options = getDefaultFormattingOptions();
    options.overallFormat = "{message} {message} {level}";

    std::string formatted = writer.formatEntry(entry, options);
    ASSERT_EQ(formatted, "This is a test message. This is a test message. INFO");
}

TEST(LogWriterTest, PrintFilteredEntries_Success) {
    MockLogAnalyzer analyzer;
    LogWriter writer(analyzer);
    filter::FilterCriteria criteria; // Dummy criteria

    LogEntry entry1 = getDefaultLogEntry();
    entry1.message = "First entry.";
    LogEntry entry2 = getDefaultLogEntry();
    entry2.level = LogLevel::WARNING;
    entry2.message = "Second entry.";

    std::vector<LogEntry> entries = {entry1, entry2};
    analyzer.setMockEntries(entries);

    FormattingOptions options = getDefaultFormattingOptions();
    options.overallFormat = "[{level}] {message}";

    std::stringstream ss;
    writer.printFilteredEntries(ss, criteria, options);
    std::string output = ss.str();

    // Expected: "[INFO] First entry.\n[WARNING] Second entry.\n"
    ASSERT_NE(output.find("[INFO] First entry."), std::string::npos);
    ASSERT_NE(output.find("[WARNING] Second entry."), std::string::npos);
    ASSERT_EQ(output.find("Error:"), std::string::npos); // Ensure no error message
}

TEST(LogWriterTest, PrintFilteredEntries_EmptyResult) {
    MockLogAnalyzer analyzer;
    LogWriter writer(analyzer);
    filter::FilterCriteria criteria;

    analyzer.setMockEntries({}); // Simulate empty results
    FormattingOptions options = getDefaultFormattingOptions();
    options.overallFormat = "[{level}] {message}";

    std::stringstream ss;
    writer.printFilteredEntries(ss, criteria, options);
    std::string output = ss.str();

    ASSERT_EQ(output, ""); // Should print nothing if no entries are returned and no error
    ASSERT_EQ(output.find("Error:"), std::string::npos);
}

TEST(LogWriterTest, PrintFilteredEntries_ErrorMode) {
    MockLogAnalyzer analyzer;
    LogWriter writer(analyzer);
    filter::FilterCriteria criteria;

    analyzer.setErrorMode(true); // Simulate an error from getFilteredEntries

    FormattingOptions options = getDefaultFormattingOptions();
    options.overallFormat = "[{level}] {message}";

    std::stringstream ss;
    writer.printFilteredEntries(ss, criteria, options);
    std::string output = ss.str();

    // Expected: Error message
    ASSERT_NE(output.find("Error: Failed to retrieve filtered log entries or no entries matched the criteria."), std::string::npos);
}

TEST(LogWriterTest, PrintFilteredEntries_OverloadWithFormatString) {
    MockLogAnalyzer analyzer;
    LogWriter writer(analyzer);
    filter::FilterCriteria criteria;

    LogEntry entry1 = getDefaultLogEntry();
    entry1.message = "Entry for format string test.";
    analyzer.setMockEntries({entry1});

    std::string_view formatString = "{message}";
    std::stringstream ss;
    writer.printFilteredEntries(ss, criteria, formatString);
    std::string output = ss.str();

    // Expected: "Entry for format string test.\n"
    ASSERT_EQ(output, "Entry for format string test.\n");
    ASSERT_EQ(output.find("Error:"), std::string::npos);
}

TEST(LogWriterTest, PrintFilteredEntries_OverloadWithFormatString_NoColorByDefault) {
    MockLogAnalyzer analyzer;
    LogWriter writer(analyzer);
    filter::FilterCriteria criteria;

    LogEntry entry1;
    entry1.level = LogLevel::ERROR;
    entry1.message = "Error Entry";
    analyzer.setMockEntries({entry1});

    std::string_view formatString = "{level}: {message}";
    std::stringstream ss;
    writer.printFilteredEntries(ss, criteria, formatString);
    std::string output = ss.str();

    // By default, the overload does not enable color.
    ASSERT_EQ(output, "ERROR: Error Entry\n");
    ASSERT_NE(output.find("ERROR"), std::string::npos);
    ASSERT_EQ(output.find("\033["), std::string::npos); // No ANSI codes
}

TEST(LogWriterTest, FormatEntry_WithStructuredFieldsDisabled) {
    MockLogAnalyzer analyzer;
    LogWriter writer(analyzer);
    LogEntry entry = getDefaultLogEntry();
    FormattingOptions options = getDefaultFormattingOptions();
    options.overallFormat = "{message} [{customFields}]";
    options.includeStructuredFields = false; // Explicitly disable
    options.structuredFieldDelimiter = " | ";
    options.structuredFieldKvDelimiter = ": ";

    std::string formatted = writer.formatEntry(entry, options);

    // Expected: "This is a test message. []"
    ASSERT_NE(formatted.find("This is a test message. []"), std::string::npos);
}

TEST(LogWriterTest, FormatEntry_WithStructuredFieldsEnabledAndEmpty) {
    MockLogAnalyzer analyzer;
    LogWriter writer(analyzer);
    LogEntry entry = getDefaultLogEntry();
    entry.customFields.clear(); // Ensure custom fields are empty
    FormattingOptions options = getDefaultFormattingOptions();
    options.overallFormat = "{message} [{customFields}]";
    options.includeStructuredFields = true; // Enabled, but customFields are empty
    options.structuredFieldDelimiter = " | ";
    options.structuredFieldKvDelimiter = ": ";

    std::string formatted = writer.formatEntry(entry, options);

    // Expected: "This is a test message. []"
    ASSERT_NE(formatted.find("This is a test message. []"), std::string::npos);
}
