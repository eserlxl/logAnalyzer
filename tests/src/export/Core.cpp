// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "TestUtils.h"

using json = nlohmann::json;

// =============================================================================================================
// Exporter Tests
// =============================================================================================================
TEST(ExporterTest, ExportEmptyEntries) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    std::stringstream ss;
    ExportSettings settings;

    // Test JSON export with empty entries
    settings.format = ExportFormat::JSON;
    exporter.exportLogEntries(ss, entries, settings);
    json j = json::parse(ss.str());
    ASSERT_TRUE(j.contains("entries"));
    ASSERT_TRUE(j["entries"].is_array());
    ASSERT_TRUE(j["entries"].empty());
    ASSERT_TRUE(j.contains("summary"));
    ASSERT_TRUE(j["summary"].contains("count"));
    ASSERT_EQ(j["summary"]["count"], 0);
    ss.str(""); // Clear stringstream

    // Test CSV export with empty entries
    settings.format = ExportFormat::CSV;
    settings.includeHeader = true;
    exporter.exportLogEntries(ss, entries, settings);
    // Only header should be present
    ASSERT_EQ(ss.str(), "ID,Timestamp,Level,Message\n"); // Default fields
    ss.str("");

    settings.includeHeader = false;
    exporter.exportLogEntries(ss, entries, settings);
    ASSERT_TRUE(ss.str().empty());
    ss.str("");

    // Test PLAINTEXT export with empty entries
    settings.format = ExportFormat::PLAINTEXT;
    exporter.exportLogEntries(ss, entries, settings);
    ASSERT_TRUE(ss.str().empty());
    ss.str("");
}

// =============================================================================================================
// Exporter::exportLogEntries Dispatching Tests
// =============================================================================================================
TEST(ExporterDispatchTest, JsonDispatch) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Dispatch test"));

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::JSON;
    settings.jsonIndent = -1; // No pretty print

    exporter.exportLogEntries(ss, entries, settings);
    json j = json::parse(ss.str());
    ASSERT_EQ(j["entries"].size(), 1);
    ASSERT_EQ(j["entries"][0]["Message"], "Dispatch test");
}

TEST(ExporterDispatchTest, CsvDispatch) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Dispatch test"));

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::CSV;
    settings.includeHeader = false;

    exporter.exportLogEntries(ss, entries, settings);
    ASSERT_EQ(ss.str(), "1,\"\",INFO,Dispatch test\n");
}

TEST(ExporterDispatchTest, PlaintextDispatch) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Dispatch test"));

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::PLAINTEXT;
    settings.textFormatString = "{message}";
    settings.useAnsiColors = false;

    exporter.exportLogEntries(ss, entries, settings);
    ASSERT_EQ(ss.str(), "Dispatch test\n");
}

TEST(ExporterErrorHandlingTest, UnknownFormatThrowsException) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Message"));

    std::stringstream ss_out;
    ExportSettings settings;
    settings.format = ExportFormat::UNKNOWN;

    ASSERT_THROW(exporter.exportLogEntries(ss_out, entries, settings), ExportException);
}

TEST(ExporterErrorHandlingTest, XmlFormatDoesNotThrowExceptionAndExports) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Message"));

    std::stringstream ss_out;
    ExportSettings settings;
    settings.format = ExportFormat::XML;

    ASSERT_NO_THROW(exporter.exportLogEntries(ss_out, entries, settings));
    std::string output = ss_out.str();
    ASSERT_FALSE(output.empty());
    ASSERT_TRUE(output.rfind("<?xml version=\"1.0\" encoding=\"UTF-8\"?>", 0) == 0);
    ASSERT_TRUE(output.find("<log>") != std::string::npos);
    ASSERT_TRUE(output.find("<entry>") != std::string::npos);
    ASSERT_TRUE(output.find("</entry>") != std::string::npos);
    ASSERT_TRUE(output.find("</log>") != std::string::npos);
}
