// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 eserlxl

#include <gtest/gtest.h>
#include <sstream>
#include <vector>
#include <nlohmann/json.hpp>
#include "export/Exporter.h"
#include "core/LogTypes.h"
#include "utils/UtilsCore.h"
#include <limits> // Required for std::numeric_limits

using json = nlohmann::json;

// Helper function to create a LogEntry
LogEntry createLogEntry(size_t id, LogLevel level, const std::string& message,
                        std::optional<std::chrono::system_clock::time_point> timestamp = std::nullopt,
                        const std::map<std::string, std::string>& customFields = {},
                        const std::string& sourceFile = "", size_t sourceLineNumber = 0) {
    LogEntry entry;
    entry.id = id;
    entry.level = level;
    entry.message = message;
    entry.timestamp = timestamp;
    for (const auto& field : customFields) {
        entry.customFields[field.first] = field.second;
    }
    entry.sourceFile = sourceFile;
    entry.sourceLineNumber = sourceLineNumber;
    return entry;
}

// =============================================================================================================
// ExportFieldMapping JSON Serialization/Deserialization Tests
// =============================================================================================================
TEST(ExportFieldMappingTest, ToJson) {
    ExportFieldMapping efm(LogEntryField::TIMESTAMP, "Time", "%Y-%m-%d");
    json j = efm;
    ASSERT_EQ(j["field"], "TIMESTAMP");
    ASSERT_EQ(j["customHeader"], "Time");
    ASSERT_EQ(j["datetimeFormat"], "%Y-%m-%d");
}

TEST(ExportFieldMappingTest, FromJsonValid) {
    json j = {{"field", "LEVEL"}, {"customHeader", "Lvl"}};
    ExportFieldMapping efm = j;
    ASSERT_TRUE(std::holds_alternative<LogEntryField>(efm.field));
    ASSERT_EQ(std::get<LogEntryField>(efm.field), LogEntryField::LEVEL);
    ASSERT_EQ(efm.customHeader, "Lvl");
    ASSERT_FALSE(efm.datetimeFormat.has_value());

    json j2 = {{"field", "TIMESTAMP"}, {"datetimeFormat", "%H:%M:%S"}};
    ExportFieldMapping efm2 = j2;
    ASSERT_TRUE(std::holds_alternative<LogEntryField>(efm2.field));
    ASSERT_EQ(std::get<LogEntryField>(efm2.field), LogEntryField::TIMESTAMP);
    ASSERT_EQ(efm2.customHeader, "");
    ASSERT_TRUE(efm2.datetimeFormat.has_value());
    ASSERT_EQ(efm2.datetimeFormat.value(), "%H:%M:%S");
}

TEST(ExportFieldMappingTest, FromJsonInvalidField) {
    json j = {{"field", "INVALID_FIELD"}};
    // This should now be handled by from_json and store as a string, not throw.
    ExportFieldMapping efm;
    ASSERT_NO_THROW(efm = j.get<ExportFieldMapping>());
    ASSERT_TRUE(std::holds_alternative<std::string>(efm.field));
    ASSERT_EQ(std::get<std::string>(efm.field), "INVALID_FIELD");
}

TEST(ExportFieldMappingTest, FromJsonMissingField) {
    json j = {{"customHeader", "Header"}};
    ASSERT_THROW(j.get<ExportFieldMapping>(), ExportException);
}

TEST(ExportFieldMappingTest, FromJsonInvalidFieldType) {
    json j = {{"field", 123}};
    ASSERT_THROW(j.get<ExportFieldMapping>(), ExportException);
}

// =============================================================================================================
// ExportSettings JSON Serialization/Deserialization Tests
// =============================================================================================================
TEST(ExportSettingsTest, DefaultConstructor) {
    ExportSettings settings;
    ASSERT_EQ(settings.outputPath, "output.log");
    ASSERT_EQ(settings.format, ExportFormat::PLAINTEXT);
    ASSERT_TRUE(settings.fieldsToExport.empty()); // Should be empty by default now
    ASSERT_TRUE(settings.includeHeader);
    ASSERT_FALSE(settings.jsonIndent.has_value());
    ASSERT_EQ(settings.separator, ',');
    ASSERT_EQ(settings.textFormatString, "{timestamp} [{level}] {message}");
    ASSERT_FALSE(settings.useAnsiColors);
}

TEST(ExportSettingsTest, ToJson) {
    ExportSettings es;
    es.outputPath = "test.json";
    es.format = ExportFormat::JSON;
    es.includeHeader = false;
    es.jsonIndent = 2;
    es.separator = ';';
    es.textFormatString = "[{level}] {message}";
    es.useAnsiColors = true;
    es.fieldsToExport.emplace_back(LogEntryField::MESSAGE, "LogMessage");

    json j = es;
    ASSERT_EQ(j["outputPath"], "test.json");
    ASSERT_EQ(j["format"], "JSON");
    ASSERT_EQ(j["includeHeader"], false);
    ASSERT_EQ(j["jsonIndent"], 2);
    ASSERT_EQ(j["separator"], ";");
    ASSERT_EQ(j["textFormatString"], "[{level}] {message}");
    ASSERT_EQ(j["useAnsiColors"], true);
    ASSERT_EQ(j["fieldsToExport"].size(), 1);
    ASSERT_EQ(j["fieldsToExport"][0]["field"], "MESSAGE");
}

TEST(ExportSettingsTest, FromJsonValid) {
    json j = {
        {"outputPath", "another.csv"},
        {"format", "CSV"},
        {"includeHeader", false},
        {"jsonIndent", 3},
        {"separator", "|"},
        {"textFormatString", "{message}"},
        {"useAnsiColors", true},
        {"fieldsToExport", {
            {{"field", "ID"}, {"customHeader", "EntryID"}},
            {{"field", "MESSAGE"}}
        }}
    };
    ExportSettings es = j.get<ExportSettings>();
    ASSERT_EQ(es.outputPath, "another.csv");
    ASSERT_EQ(es.format, ExportFormat::CSV);
    ASSERT_FALSE(es.includeHeader);
    ASSERT_TRUE(es.jsonIndent.has_value());
    ASSERT_EQ(es.jsonIndent.value(), 3);
    ASSERT_EQ(es.separator, '|');
    ASSERT_EQ(es.textFormatString, "{message}");
    ASSERT_TRUE(es.useAnsiColors);
    ASSERT_EQ(es.fieldsToExport.size(), 2);
    ASSERT_TRUE(std::holds_alternative<LogEntryField>(es.fieldsToExport[0].field));
    ASSERT_EQ(std::get<LogEntryField>(es.fieldsToExport[0].field), LogEntryField::ID);
    ASSERT_EQ(es.fieldsToExport[0].customHeader, "EntryID");
    ASSERT_TRUE(std::holds_alternative<LogEntryField>(es.fieldsToExport[1].field));
    ASSERT_EQ(std::get<LogEntryField>(es.fieldsToExport[1].field), LogEntryField::MESSAGE);
}

TEST(ExportSettingsTest, FromJsonInvalidOutputPathType) {
    json j = {{"outputPath", 123}};
    ASSERT_THROW(j.get<ExportSettings>(), json::exception);
}

TEST(ExportSettingsTest, FromJsonInvalidFormatType) {
    json j = {{"format", 123}};
    ASSERT_THROW(j.get<ExportSettings>(), json::exception);
}

TEST(ExportSettingsTest, FromJsonInvalidFormatValue) {
    json j = {{"format", "UNKNOWN_FORMAT"}};
    ASSERT_THROW(j.get<ExportSettings>(), ExportException);
}

TEST(ExportSettingsTest, FromJsonInvalidFieldsToExportType) {
    json j = {{"fieldsToExport", "not_an_array"}};
    ASSERT_THROW(j.get<ExportSettings>(), json::exception);
}

TEST(ExportSettingsTest, FromJsonInvalidIncludeHeaderType) {
    json j = {{"includeHeader", "not_a_boolean"}};
    ASSERT_THROW(j.get<ExportSettings>(), json::exception);
}

TEST(ExportSettingsTest, FromJsonInvalidJsonIndentType) {
    json j = {{"jsonIndent", "not_an_int"}};
    ASSERT_THROW(j.get<ExportSettings>(), json::exception);
}

TEST(ExportSettingsTest, FromJsonInvalidSeparatorType) {
    json j = {{"separator", 123}};
    ASSERT_THROW(j.get<ExportSettings>(), json::exception);
}

TEST(ExportSettingsTest, FromJsonInvalidSeparatorLength) {
    json j = {{"separator", "ab"}};
    ASSERT_THROW(j.get<ExportSettings>(), ExportException);
}

TEST(ExportSettingsTest, FromJsonInvalidTextFormatStringType) {
    json j = {{"textFormatString", 123}};
    ASSERT_THROW(j.get<ExportSettings>(), json::exception);
}

TEST(ExportSettingsTest, FromJsonInvalidUseAnsiColorsType) {
    json j = {{"useAnsiColors", "false"}}; // String "false" is not boolean false
    ASSERT_THROW(j.get<ExportSettings>(), json::exception);
}

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

TEST(ExporterJsonTest, FieldsToExportConfig) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    auto now = std::chrono::system_clock::now();
    // Entry 1: Standard fields, custom fields, timestamp
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Message One", now,
                                     {{"user", "alice"}, {"session_id", "abc"}}));
    // Entry 2: Different custom fields, no timestamp
    entries.push_back(createLogEntry(2, LogLevel::DEBUG, "Message Two", std::nullopt,
                                     {{"component", "network"}, {"request_id", "123"}}));
    // Entry 3: Only custom fields relevant to export
    entries.push_back(createLogEntry(3, LogLevel::WARNING, "Message Three", std::nullopt,
                                     {{"user", "bob"}, {"status_code", "200"}}));

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::JSON;
    settings.jsonIndent = -1; // No pretty print

    // Configure specific fields to export
    settings.fieldsToExport.emplace_back(LogEntryField::ID, "EntryID"); // Standard field with custom header
    settings.fieldsToExport.emplace_back(LogEntryField::MESSAGE); // Standard field with default header
    settings.fieldsToExport.emplace_back(LogEntryField::CUSTOM, "user"); // Custom field, header is field name
    settings.fieldsToExport.emplace_back(LogEntryField::CUSTOM, "session_id", std::nullopt); // Custom field with explicit (same) header
    settings.fieldsToExport.emplace_back(LogEntryField::TIMESTAMP, "EventTime", "%Y-%m-%d"); // Standard field with custom header and datetime format

    exporter.exportLogEntries(ss, entries, settings);
    json j = json::parse(ss.str());

    ASSERT_EQ(j["entries"].size(), 3);

    // Verify Entry 1
    ASSERT_TRUE(j["entries"][0].contains("EntryID"));
    ASSERT_EQ(j["entries"][0]["EntryID"], 1);
    ASSERT_TRUE(j["entries"][0].contains("MESSAGE"));
    ASSERT_EQ(j["entries"][0]["MESSAGE"], "Message One");
    ASSERT_TRUE(j["entries"][0].contains("user"));
    ASSERT_EQ(j["entries"][0]["user"], "alice");
    ASSERT_TRUE(j["entries"][0].contains("session_id"));
    ASSERT_EQ(j["entries"][0]["session_id"], "abc");
    ASSERT_TRUE(j["entries"][0].contains("EventTime"));
    ASSERT_EQ(j["entries"][0]["EventTime"], Utils::formatTimestamp(now, "%Y-%m-%d"));
    // Check for fields NOT requested
    ASSERT_FALSE(j["entries"][0].contains("level"));
    ASSERT_FALSE(j["entries"][0].contains("component"));
    ASSERT_FALSE(j["entries"][0].contains("request_id"));
    ASSERT_FALSE(j["entries"][0].contains("status_code"));

    // Verify Entry 2
    ASSERT_TRUE(j["entries"][1].contains("EntryID"));
    ASSERT_EQ(j["entries"][1]["EntryID"], 2);
    ASSERT_TRUE(j["entries"][1].contains("MESSAGE"));
    ASSERT_EQ(j["entries"][1]["MESSAGE"], "Message Two");
    ASSERT_FALSE(j["entries"][1].contains("user")); // Not present in this entry
    ASSERT_FALSE(j["entries"][1].contains("session_id")); // Not present in this entry
    ASSERT_TRUE(j["entries"][1].contains("EventTime")); // Field should be present
    ASSERT_TRUE(j["entries"][1]["EventTime"].is_null()); // Value should be null
    // Custom fields 'component' and 'request_id' were NOT requested in fieldsToExport, so they should NOT be present.
    ASSERT_FALSE(j["entries"][1].contains("component"));
    ASSERT_FALSE(j["entries"][1].contains("request_id"));

    // Verify Entry 3
    ASSERT_TRUE(j["entries"][2].contains("EntryID"));
    ASSERT_EQ(j["entries"][2]["EntryID"], 3);
    ASSERT_TRUE(j["entries"][2].contains("MESSAGE"));
    ASSERT_EQ(j["entries"][2]["MESSAGE"], "Message Three");
    ASSERT_TRUE(j["entries"][2].contains("user"));
    ASSERT_EQ(j["entries"][2]["user"], "bob");
    ASSERT_FALSE(j["entries"][2].contains("session_id")); // Not present in this entry
    ASSERT_FALSE(j["entries"][2].contains("status_code")); // Custom field not requested
    ASSERT_TRUE(j["entries"][2].contains("EventTime")); // Field should be present
    ASSERT_TRUE(j["entries"][2]["EventTime"].is_null()); // Value should be null
}

TEST(ExporterJsonTest, JsonIndentBehavior) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Test Message"));

    // Test with jsonIndent = 0 (no indent, but pretty-printed with newlines)
    std::stringstream ss0;
    ExportSettings settings0;
    settings0.format = ExportFormat::JSON;
    settings0.jsonIndent = 0;
    exporter.exportLogEntries(ss0, entries, settings0);
    std::string expected_pretty_json_no_indent = "{\n\"entries\": [\n{\n\"ID\": 1,\n\"Level\": \"INFO\",\n\"Message\": \"Test Message\",\n\"Timestamp\": null\n}\n],\n\"summary\": {\n\"count\": 1\n}\n}\n";
    ASSERT_EQ(ss0.str(), expected_pretty_json_no_indent);

    // Test with jsonIndent = 4
    std::stringstream ss4;
    ExportSettings settings4;
    settings4.format = ExportFormat::JSON;
    settings4.jsonIndent = 4;
    exporter.exportLogEntries(ss4, entries, settings4);
    json j4 = json::parse(ss4.str());
    ASSERT_TRUE(ss4.str().find("    \"entries\"") != std::string::npos); // Check for 4 spaces indent
    ASSERT_TRUE(ss4.str().find('\n') != std::string::npos); // Should have newlines
    
    // Test with jsonIndent = negative (should result in compact output with no newlines)
    std::stringstream ssNeg;
    ExportSettings settingsNeg;
    settingsNeg.format = ExportFormat::JSON;
    settingsNeg.jsonIndent = -1; // Any negative value
    exporter.exportLogEntries(ssNeg, entries, settingsNeg);
    std::string expected_compact_json = "{\"entries\":[{\"ID\":1,\"Level\":\"INFO\",\"Message\":\"Test Message\",\"Timestamp\":null}],\"summary\":{\"count\":1}}\n"; // Added newline as per Exporter.cpp
    ASSERT_EQ(ssNeg.str(), expected_compact_json);
}

TEST(ExporterJsonTest, DefaultFieldDiscoveryLogic) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    auto now = std::chrono::system_clock::now();
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Msg1", now, {{"custom_key", "custom_val"}}));
    entries.push_back(createLogEntry(2, LogLevel::WARNING, "Msg2", std::nullopt, {{"another_key", "another_val"}, {"custom_key", "new_val"}}));

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::JSON;
    settings.fieldsToExport = {}; // Trigger default discovery
    settings.jsonIndent = -1; // Compact output for easier parsing

    exporter.exportLogEntries(ss, entries, settings);
    json j = json::parse(ss.str());

    ASSERT_EQ(j["entries"].size(), 2);

    // Entry 1 should have standard fields + custom_key
    ASSERT_TRUE(j["entries"][0].contains("ID"));
    ASSERT_TRUE(j["entries"][0].contains("Timestamp"));
    ASSERT_TRUE(j["entries"][0].contains("Level"));
    ASSERT_TRUE(j["entries"][0].contains("Message"));
    ASSERT_TRUE(j["entries"][0].contains("custom_key"));
    ASSERT_FALSE(j["entries"][0].contains("another_key")); // Not in this entry

    // Entry 2 should have standard fields + another_key + custom_key
    ASSERT_TRUE(j["entries"][1].contains("ID"));
    ASSERT_TRUE(j["entries"][1].contains("Timestamp")); // Field should be present
    ASSERT_TRUE(j["entries"][1]["Timestamp"].is_null()); // Value should be null
    ASSERT_TRUE(j["entries"][1].contains("Level"));
    ASSERT_TRUE(j["entries"][1].contains("Message"));
    ASSERT_TRUE(j["entries"][1].contains("custom_key"));
    ASSERT_TRUE(j["entries"][1].contains("another_key"));
    
    // Verify values
    ASSERT_EQ(j["entries"][0]["custom_key"], "custom_val");
    ASSERT_EQ(j["entries"][1]["custom_key"], "new_val");
    ASSERT_EQ(j["entries"][1]["another_key"], "another_val");
}

TEST(ExporterJsonTest, EmptySourceFileAndMessage) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    LogEntry entry = createLogEntry(1, LogLevel::INFO, "");
    entry.sourceFile = "";
    entries.push_back(entry);

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::JSON;
    settings.jsonIndent = -1;
    settings.fieldsToExport.emplace_back(LogEntryField::ID);
    settings.fieldsToExport.emplace_back(LogEntryField::MESSAGE);
    settings.fieldsToExport.emplace_back(LogEntryField::SOURCE_FILE);

    exporter.exportLogEntries(ss, entries, settings);
    json j = json::parse(ss.str());

    ASSERT_EQ(j["entries"].size(), 1);
    ASSERT_TRUE(j["entries"][0].contains("ID")); // This one works, keep it.
    ASSERT_EQ(j["entries"][0]["ID"], 1);

    ASSERT_NO_THROW(j["entries"][0].at("MESSAGE"));
    ASSERT_TRUE(j["entries"][0].at("MESSAGE").is_string()); // Ensure it's a string
    ASSERT_EQ(j["entries"][0].at("MESSAGE").get<std::string>(), ""); // Empty message should be included

    ASSERT_NO_THROW(j["entries"][0].at("SOURCE_FILE"));
    ASSERT_TRUE(j["entries"][0].at("SOURCE_FILE").is_string()); // Ensure it's a string
    ASSERT_EQ(j["entries"][0].at("SOURCE_FILE").get<std::string>(), "");
}

TEST(ExporterJsonTest, BoundaryValuesForIdAndLineNumber) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    LogEntry entry1 = createLogEntry(0, LogLevel::INFO, "ID Zero", std::nullopt, std::map<std::string, std::string>{}, "", 0);
    entries.push_back(entry1);

    LogEntry entry2 = createLogEntry(std::numeric_limits<size_t>::max(), LogLevel::INFO, "Max ID", std::nullopt, std::map<std::string, std::string>{}, "", std::numeric_limits<size_t>::max());
    entries.push_back(entry2);

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::JSON;
    settings.jsonIndent = -1;
    settings.fieldsToExport.emplace_back(LogEntryField::ID);
    settings.fieldsToExport.emplace_back(LogEntryField::LINE_NUMBER);
    settings.fieldsToExport.emplace_back(LogEntryField::MESSAGE);

    exporter.exportLogEntries(ss, entries, settings);
    json j = json::parse(ss.str());

    ASSERT_EQ(j["entries"].size(), 2);
    // Entry 1
    ASSERT_TRUE(j["entries"][0].contains("ID"));
    ASSERT_EQ(j["entries"][0]["ID"], 0);
    
    ASSERT_NO_THROW(j["entries"][0].at("LINE_NUMBER"));
    ASSERT_TRUE(j["entries"][0].at("LINE_NUMBER").is_number_integer()); // Ensure it's an integer
    ASSERT_EQ(j["entries"][0].at("LINE_NUMBER").get<size_t>(), 0);
    // Entry 2
    ASSERT_TRUE(j["entries"][1].contains("ID"));
    ASSERT_EQ(j["entries"][1]["ID"], std::numeric_limits<size_t>::max());
    
    ASSERT_NO_THROW(j["entries"][1].at("LINE_NUMBER"));
    ASSERT_TRUE(j["entries"][1].at("LINE_NUMBER").is_number_integer()); // Ensure it's an integer
    ASSERT_EQ(j["entries"][1].at("LINE_NUMBER").get<size_t>(), std::numeric_limits<size_t>::max());
}

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
    std::string expectedRow = "1,\"\",INFO,\"A message with, commas and \"\"quotes\"\".\nNew line.\"\n";
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
