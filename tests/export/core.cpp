// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "helper.h"
#include "export/core.h"

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
    ASSERT_EQ(ss.str(), "ID,TIMESTAMP,LEVEL,MESSAGE\n"); // Default fields
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
    ASSERT_EQ(j["entries"][0]["MESSAGE"], "Dispatch test");
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

// Every field closing tag must include its '>' so the XML is well-formed.
TEST(ExporterXmlTest, FieldClosingTagsAreWellFormed) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "msg"));

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::XML;

    exporter.exportLogEntries(ss, entries, settings);
    const std::string output = ss.str();
    SCOPED_TRACE(output);

    ASSERT_NE(output.find("<MESSAGE>msg</MESSAGE>"), std::string::npos);
    ASSERT_NE(output.find("<LEVEL>INFO</LEVEL>"), std::string::npos);
    // A closing tag immediately followed by a newline (no '>') must not occur.
    ASSERT_EQ(output.find("</MESSAGE\n"), std::string::npos);
}

// XML metacharacters in field values are escaped to entity references.
TEST(ExporterXmlTest, EscapesSpecialCharactersInValues) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "a<b>&\"'"));

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::XML;

    exporter.exportLogEntries(ss, entries, settings);
    const std::string output = ss.str();
    SCOPED_TRACE(output);

    ASSERT_NE(output.find("<MESSAGE>a&lt;b&gt;&amp;&quot;&apos;</MESSAGE>"), std::string::npos);
}

// Custom-field keys become element names, so arbitrary keys (spaces, leading
// digits, '&') must be coerced into well-formed XML Names.
TEST(ExporterXmlTest, SanitizesCustomFieldKeys) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    LogEntry entry = createLogEntry(1, LogLevel::INFO, "msg");
    entry.customFields = {{"1bad&key", "v1"}, {"a b", "v2"}};
    entries.push_back(entry);

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::XML;
    // Emit every custom field as its own element.
    settings.fieldsToExport = {
        ExportFieldMapping(std::string("1bad&key")),
        ExportFieldMapping(std::string("a b")),
    };

    exporter.exportLogEntries(ss, entries, settings);
    const std::string output = ss.str();
    SCOPED_TRACE(output);

    ASSERT_NE(output.find("<_1bad_key>v1</_1bad_key>"), std::string::npos);
    ASSERT_NE(output.find("<a_b>v2</a_b>"), std::string::npos);
    ASSERT_EQ(output.find("<1bad&key>"), std::string::npos);
    ASSERT_EQ(output.find("<a b>"), std::string::npos);
}
