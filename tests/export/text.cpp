// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "helper.h"
#include "export/core.h"

// =============================================================================================================
// Exporter::exportAsText and formatEntryForText Tests
// =============================================================================================================
TEST(ExporterTextTest, BasicExport) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    auto now = std::chrono::system_clock::now();
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Message 1", now));
    entries.push_back(createLogEntry(2, LogLevel::WARNING, "Message 2"));

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::PLAINTEXT;
    settings.textFormatString = "[{id}] {timestamp} {level}: {message}";
    settings.useAnsiColors = false;

    exporter.exportLogEntries(ss, entries, settings);
    std::string expectedOutput =
        "[1] " + Utils::formatTimestamp(now) + " INFO: Message 1\n"
        "[2]  WARNING: Message 2\n"; // Empty timestamp for second entry
    ASSERT_EQ(ss.str(), expectedOutput);
}

TEST(ExporterTextTest, ColorFormatting) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::FATAL, "Fatal error"));
    entries.push_back(createLogEntry(2, LogLevel::ERROR, "Error occurred"));
    entries.push_back(createLogEntry(3, LogLevel::WARNING, "Warning message"));
    entries.push_back(createLogEntry(4, LogLevel::INFO, "Info event"));
    entries.push_back(createLogEntry(5, LogLevel::DEBUG, "Debug trace"));
    entries.push_back(createLogEntry(6, LogLevel::TRACE, "Trace detail"));
    entries.push_back(createLogEntry(7, LogLevel::UNKNOWN, "Unknown level"));


    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::PLAINTEXT;
    settings.textFormatString = "{level}: {message}";
    settings.useAnsiColors = true;

    exporter.exportLogEntries(ss, entries, settings);
    std::string output = ss.str();

    // Check for RED color on FATAL/ERROR
    ASSERT_TRUE(output.find(Utils::AnsiColor::RED.data() + std::string("FATAL") + Utils::AnsiColor::RESET.data()) != std::string::npos);
    ASSERT_TRUE(output.find(Utils::AnsiColor::RED.data() + std::string("ERROR") + Utils::AnsiColor::RESET.data()) != std::string::npos);
    
    // Check for YELLOW color on WARN
    ASSERT_TRUE(output.find(Utils::AnsiColor::YELLOW.data() + std::string("WARNING") + Utils::AnsiColor::RESET.data()) != std::string::npos);

    // Check for CYAN color on INFO
    ASSERT_TRUE(output.find(Utils::AnsiColor::CYAN.data() + std::string("INFO") + Utils::AnsiColor::RESET.data()) != std::string::npos);

    // Check for GREEN color on DEBUG/TRACE
    ASSERT_TRUE(output.find(Utils::AnsiColor::GREEN.data() + std::string("DEBUG") + Utils::AnsiColor::RESET.data()) != std::string::npos);
    ASSERT_TRUE(output.find(Utils::AnsiColor::GREEN.data() + std::string("TRACE") + Utils::AnsiColor::RESET.data()) != std::string::npos);

    // UNKNOWN should not have color
    ASSERT_TRUE(output.find(std::string("UNKNOWN")) != std::string::npos); // Should not contain AnsiColor codes around UNKNOWN
    ASSERT_FALSE(output.find(Utils::AnsiColor::RED.data() + std::string("UNKNOWN")) != std::string::npos);
    ASSERT_FALSE(output.find(Utils::AnsiColor::YELLOW.data() + std::string("UNKNOWN")) != std::string::npos);
    ASSERT_FALSE(output.find(Utils::AnsiColor::CYAN.data() + std::string("UNKNOWN")) != std::string::npos);
    ASSERT_FALSE(output.find(Utils::AnsiColor::GREEN.data() + std::string("UNKNOWN")) != std::string::npos);
}

TEST(ExporterTextTest, ExtendedPlaceholderSubstitution) {
    Exporter exporter;
    LogEntry entry = createLogEntry(
        42,
        LogLevel::INFO,
        "Ready",
        std::nullopt,
        {},
        "service.log",
        77
    );
    entry.threadId = "thr-1";
    entry.module = "auth";
    entry.host = "node-a";

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::PLAINTEXT;
    settings.textFormatString =
        "{sourceFile}:{lineNumber} [{threadId}] ({module}@{host}) {message}";
    settings.useAnsiColors = false;

    exporter.exportLogEntries(ss, {entry}, settings);
    EXPECT_EQ(
        ss.str(),
        "service.log:77 [thr-1] (auth@node-a) Ready\n"
    );
}

TEST(ExporterTextTest, CustomFieldsPlaceholder) {
    Exporter exporter;
    LogEntry entry = createLogEntry(
        7,
        LogLevel::INFO,
        "msg",
        std::nullopt,
        {{"k1", "v1"}, {"k2", "v2"}}
    );

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::PLAINTEXT;
    settings.textFormatString = "{customFields}";
    settings.useAnsiColors = false;

    exporter.exportLogEntries(ss, {entry}, settings);
    EXPECT_EQ(ss.str(), "k1:v1;k2:v2\n");
}
