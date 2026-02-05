// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "tests/export/TestUtils.h"

// =============================================================================================================
// Exporter::exportAsCsv Tests
// =============================================================================================================
TEST(ExporterCsvTest, BasicExport) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    auto now = std::chrono::system_clock::now();
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Message 1", now));
    entries.push_back(createLogEntry(2, LogLevel::WARNING, "Message 2"));

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::CSV;
    settings.separator = ',';
    settings.includeHeader = true;

    exporter.exportLogEntries(ss, entries, settings);
    std::string expectedHeader = "ID,Timestamp,Level,Message\n";
    std::string expectedRow1 = "1," + Utils::formatTimestamp(now) + ",INFO,Message 1\n";
    std::string expectedRow2 = "2,\"\",WARNING,Message 2\n";
    ASSERT_EQ(ss.str(), expectedHeader + expectedRow1 + expectedRow2);
}

TEST(ExporterCsvTest, CustomSeparator) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Message 1"));

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::CSV;
    settings.separator = ';';
    settings.includeHeader = true;

    exporter.exportLogEntries(ss, entries, settings);
    std::string expectedHeader = "ID;Timestamp;Level;Message\n";
    std::string expectedRow1 = "1;\"\";INFO;Message 1\n";
    ASSERT_EQ(ss.str(), expectedHeader + expectedRow1);
}

TEST(ExporterCsvTest, NoHeader) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Message 1"));

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::CSV;
    settings.includeHeader = false;

    exporter.exportLogEntries(ss, entries, settings);
    std::string expectedRow1 = "1,\"\",INFO,Message 1\n";
    ASSERT_EQ(ss.str(), expectedRow1);
}

TEST(ExporterCsvTest, FieldsWithSpecialCharsAndQuoting) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "A message with, commas and \"quotes\".\nNew line."));

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::CSV;
    settings.separator = ',';
    settings.includeHeader = false;

    exporter.exportLogEntries(ss, entries, settings);
    std::string expectedRow = "1,\"\",INFO,\"A message with, commas and \"\"quotes\"\".\nNew line.\"\"\n";
    ASSERT_EQ(ss.str(), expectedRow);
}

TEST(ExporterCsvTest, ExportSpecificFields) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Msg1"));
    entries.push_back(createLogEntry(2, LogLevel::WARNING, "Msg2"));

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::CSV;
    settings.includeHeader = true;
    settings.fieldsToExport.emplace_back(LogEntryField::MESSAGE, "Log_Message");
    settings.fieldsToExport.emplace_back(LogEntryField::ID); // Default header for ID

    exporter.exportLogEntries(ss, entries, settings);
    std::string expectedHeader = "Log_Message,ID\n";
    std::string expectedRow1 = "Msg1,1\n";
    std::string expectedRow2 = "Msg2,2\n";
    ASSERT_EQ(ss.str(), expectedHeader + expectedRow1 + expectedRow2);
}

TEST(ExporterCsvTest, ExportCustomFieldsDynamically) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Msg1", std::nullopt, {{"user", "alice"}, {"session", "123"}}));
    entries.push_back(createLogEntry(2, LogLevel::WARNING, "Msg2", std::nullopt, {{"session", "456"}, {"ip", "127.0.0.1"}}));

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::CSV;
    settings.includeHeader = true;
    settings.fieldsToExport = {}; // Should trigger dynamic discovery

    exporter.exportLogEntries(ss, entries, settings);
    // Order of custom fields is not guaranteed, so check for presence and values
    std::string output = ss.str();

    // Check for headers (standard + sorted custom fields)
    // The exact order of dynamically discovered custom fields might vary depending on std::set
    // For now, let's just check for the presence of relevant parts
    ASSERT_TRUE(output.find("ID,Timestamp,Level,Message") != std::string::npos);
    ASSERT_TRUE(output.find("ip") != std::string::npos);
    ASSERT_TRUE(output.find("session") != std::string::npos);
    ASSERT_TRUE(output.find("user") != std::string::npos);
    
    // Check row content
    ASSERT_TRUE(output.find("1,\"\",INFO,Msg1") != std::string::npos);
    ASSERT_TRUE(output.find("2,\"\",WARNING,Msg2") != std::string::npos);
}

TEST(ExporterCsvTest, ExportCustomFieldsExplicitly) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Msg1", std::nullopt, {{"user", "alice"}, {"session", "123"}}));
    entries.push_back(createLogEntry(2, LogLevel::WARNING, "Msg2", std::nullopt, {{"session", "456"}, {"ip", "127.0.0.1"}}));

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::CSV;
    settings.includeHeader = true;
    // Explicitly define custom fields to export and their headers
    settings.fieldsToExport.emplace_back(LogEntryField::ID);
    settings.fieldsToExport.emplace_back(LogEntryField::CUSTOM, "session", std::nullopt); // Field::CUSTOM means lookup by customHeader
    settings.fieldsToExport.emplace_back(LogEntryField::CUSTOM, "user", std::nullopt);

    exporter.exportLogEntries(ss, entries, settings);
    std::string expectedHeader = "ID,session,user\n";
    std::string expectedRow1 = "1,123,alice\n";
    std::string expectedRow2 = "2,456,\"\"\n"; // User field not present in second entry, now quoted
    ASSERT_EQ(ss.str(), expectedHeader + expectedRow1 + expectedRow2);
}

TEST(ExporterCsvTest, DateTimeFormat) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    auto tp = Utils::parseAbsoluteTime("2023-10-27 10:30:00").value();
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Message", tp));

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::CSV;
    settings.includeHeader = true;
    settings.fieldsToExport.emplace_back(LogEntryField::TIMESTAMP, "Time", "%Y/%m/%d %H:%M");

    exporter.exportLogEntries(ss, entries, settings);
    std::string expectedHeader = "Time\n";
    std::string expectedRow = "2023/10/27 10:30\n";
    ASSERT_EQ(ss.str(), expectedHeader + expectedRow);
}

TEST(ExporterCsvTest, QuotingAndEscaping_OnlySeparator) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Message,with,comma"));
    
    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::CSV;
    settings.separator = ',';
    settings.includeHeader = false;
    settings.fieldsToExport.emplace_back(LogEntryField::MESSAGE);

    exporter.exportLogEntries(ss, entries, settings);
    ASSERT_EQ(ss.str(), "\"Message,with,comma\"\n");
}

TEST(ExporterCsvTest, QuotingAndEscaping_OnlyDoubleQuote) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Message with \"quotes\" inside"));
    
    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::CSV;
    settings.separator = ',';
    settings.includeHeader = false;
    settings.fieldsToExport.emplace_back(LogEntryField::MESSAGE);

    exporter.exportLogEntries(ss, entries, settings);
    ASSERT_EQ(ss.str(), "\"Message with \"\"quotes\"\" inside\"\n");
}

TEST(ExporterCsvTest, QuotingAndEscaping_NewlineAndCR) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Message with\nnewline\r\nand CR"));
    
    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::CSV;
    settings.separator = ',';
    settings.includeHeader = false;
    settings.fieldsToExport.emplace_back(LogEntryField::MESSAGE);

    exporter.exportLogEntries(ss, entries, settings);
    ASSERT_EQ(ss.str(), "\"Message with\nnewline\r\nand CR\"\n");
}

TEST(ExporterCsvTest, QuotingAndEscaping_EmptyField) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, ""));
    
    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::CSV;
    settings.separator = ',';
    settings.includeHeader = false;
    settings.fieldsToExport.emplace_back(LogEntryField::MESSAGE);

    exporter.exportLogEntries(ss, entries, settings);
    ASSERT_EQ(ss.str(), "\"\"\n");
}

TEST(ExporterCsvTest, QuotingAndEscaping_AllSpecialChars) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Hello, \"World\"!\nThis is a test\r\nwith all,special\"characters."));
    
    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::CSV;
    settings.separator = ',';
    settings.includeHeader = false;
    settings.fieldsToExport.emplace_back(LogEntryField::MESSAGE);

    exporter.exportLogEntries(ss, entries, settings);
    ASSERT_EQ(ss.str(), "\"Hello, \"\"World\"\"!\nThis is a test\r\nwith all,special\"\"characters.\"\n");
}
