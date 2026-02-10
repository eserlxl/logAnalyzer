// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include <chrono>
#include <optional>
#include <map>
#include "utils.h"
#include "core/log/types.h" // Assuming LogEntry and LogLevel are defined here

// Helper to create a time point for testing
std::chrono::system_clock::time_point create_test_time_point(int year, int month, int day, int hour, int min, int sec) {
    std::tm tm = {};
    tm.tm_year = year - 1900; // Years since 1900
    tm.tm_mon = month - 1;    // Months since January (0-11)
    tm.tm_mday = day;         // Day of the month (1-31)
    tm.tm_hour = hour;        // Hours since midnight (0-23)
    tm.tm_min = min;          // Minutes after the hour (0-59)
    tm.tm_sec = sec;          // Seconds after the minute (0-60)
    tm.tm_isdst = -1;         // Daylight Saving Time flag
    std::time_t t = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(t);
}

TEST(ExportTestUtilsTest, CreateLogEntry_AllParametersProvided) {
    auto test_time = create_test_time_point(2023, 10, 27, 10, 30, 0);
    std::map<std::string, std::string> custom_fields = {{"user_id", "123"}, {"request_id", "abc"}};
    size_t id = 1;
    LogLevel level = LogLevel::INFO;
    std::string message = "Test message";
    std::string source_file = "TestSource.cpp";
    size_t source_line = 42;

    LogEntry entry = createLogEntry(id, level, message, test_time, custom_fields, source_file, source_line);

    EXPECT_EQ(entry.id, id);
    EXPECT_EQ(entry.level, level);
    EXPECT_EQ(entry.message, message);
    ASSERT_TRUE(entry.timestamp.has_value());
    EXPECT_EQ(entry.timestamp.value(), test_time);
    ASSERT_EQ(entry.customFields.size(), custom_fields.size());
    EXPECT_EQ(entry.customFields.at("user_id"), "123");
    EXPECT_EQ(entry.customFields.at("request_id"), "abc");
    EXPECT_EQ(entry.sourceFile, source_file);
    EXPECT_EQ(entry.sourceLineNumber, source_line);
}

TEST(ExportTestUtilsTest, CreateLogEntry_DefaultParameters) {
    size_t id = 1;
    LogLevel level = LogLevel::INFO;
    std::string message = "Default test message";

    LogEntry entry = createLogEntry(id, level, message);

    EXPECT_EQ(entry.id, id);
    EXPECT_EQ(entry.level, level);
    EXPECT_EQ(entry.message, message);
    EXPECT_FALSE(entry.timestamp.has_value()); // Default should be nullopt
    EXPECT_TRUE(entry.customFields.empty());   // Default should be empty map
    EXPECT_TRUE(entry.sourceFile.empty());     // Default should be empty string
    EXPECT_EQ(entry.sourceLineNumber, 0);      // Default should be 0
}

TEST(ExportTestUtilsTest, CreateLogEntry_EmptyStringsAndMap) {
    size_t id = 2;
    LogLevel level = LogLevel::DEBUG;
    std::string message = ""; // Empty message
    std::map<std::string, std::string> custom_fields = {}; // Empty custom fields
    std::string source_file = ""; // Empty source file
    size_t source_line = 0;

    LogEntry entry = createLogEntry(id, level, message, std::nullopt, custom_fields, source_file, source_line);

    EXPECT_EQ(entry.id, id);
    EXPECT_EQ(entry.level, level);
    EXPECT_TRUE(entry.message.empty());
    EXPECT_FALSE(entry.timestamp.has_value());
    EXPECT_TRUE(entry.customFields.empty());
    EXPECT_TRUE(entry.sourceFile.empty());
    EXPECT_EQ(entry.sourceLineNumber, source_line);
}

TEST(ExportTestUtilsTest, CreateLogEntry_TimestampNullOpt) {
    size_t id = 3;
    LogLevel level = LogLevel::WARNING;
    std::string message = "Timestamp nullopt test";

    LogEntry entry = createLogEntry(id, level, message, std::nullopt);

    EXPECT_EQ(entry.id, id);
    EXPECT_EQ(entry.level, level);
    EXPECT_EQ(entry.message, message);
    EXPECT_FALSE(entry.timestamp.has_value());
}

TEST(ExportTestUtilsTest, CreateLogEntry_TimestampProvided) {
    auto test_time = create_test_time_point(2024, 1, 1, 12, 0, 0);
    size_t id = 4;
    LogLevel level = LogLevel::ERROR;
    std::string message = "Timestamp provided test";

    LogEntry entry = createLogEntry(id, level, message, test_time);

    EXPECT_EQ(entry.id, id);
    EXPECT_EQ(entry.level, level);
    EXPECT_EQ(entry.message, message);
    ASSERT_TRUE(entry.timestamp.has_value());
    EXPECT_EQ(entry.timestamp.value(), test_time);
}

TEST(ExportTestUtilsTest, CreateLogEntry_DifferentLogLevels) {
    // Test with INFO (already used)
    LogEntry entry_info = createLogEntry(5, LogLevel::INFO, "Info level");
    EXPECT_EQ(entry_info.level, LogLevel::INFO);

    // Test with DEBUG
    LogEntry entry_debug = createLogEntry(6, LogLevel::DEBUG, "Debug level");
    EXPECT_EQ(entry_debug.level, LogLevel::DEBUG);

    // Test with WARNING
    LogEntry entry_warning = createLogEntry(7, LogLevel::WARNING, "Warning level");
    EXPECT_EQ(entry_warning.level, LogLevel::WARNING);

    // Test with ERROR
    LogEntry entry_error = createLogEntry(8, LogLevel::ERROR, "Error level");
    EXPECT_EQ(entry_error.level, LogLevel::ERROR);

    // Test with CRITICAL
    LogEntry entry_critical = createLogEntry(9, LogLevel::CRITICAL, "Critical level");
    EXPECT_EQ(entry_critical.level, LogLevel::CRITICAL);

    // Test with FATAL
    LogEntry entry_fatal = createLogEntry(10, LogLevel::FATAL, "Fatal level");
    EXPECT_EQ(entry_fatal.level, LogLevel::FATAL);
}

// Test to ensure [[nodiscard]] works (compiler warning, not runtime check)
// This test case is more conceptual. A compiler will warn if the return value is ignored.
// We can't directly test [[nodiscard]] with a runtime assertion easily.
// However, we can ensure the function signature has it. The previous replace operation handled this.
// This test could be a placeholder to document that the [[nodiscard]] attribute was added.
TEST(ExportTestUtilsTest, CreateLogEntry_NodiscardAttributeUsage) {
    // This test does not require any runtime assertions for [[nodiscard]] itself.
    // The presence of [[nodiscard]] in the function signature (applied in ExportTestUtils.h)
    // instructs the compiler to issue a warning if the return value is ignored.
    // We demonstrate using the return value here.
    LogEntry entry = createLogEntry(11, LogLevel::INFO, "Nodiscard test");
    EXPECT_EQ(entry.id, 11);

    // To demonstrate a potential ignored return (which would cause a compiler warning):
    // createLogEntry(12, LogLevel::DEBUG, "Ignored return"); // This line would trigger a [[nodiscard]] warning if uncommented.
}
