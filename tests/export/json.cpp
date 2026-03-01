// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "helper.h"
#include "export/core.h"

using json = nlohmann::json;

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
    settings.fieldsToExport.emplace_back("user"); // Custom field, header is field name
    settings.fieldsToExport.emplace_back("session_id"); // Custom field, header is field name
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
    std::string expected_pretty_json_no_indent = "{\n\"entries\": [\n{\n\"ID\": 1,\n\"LEVEL\": \"INFO\",\n\"MESSAGE\": \"Test Message\",\n\"TIMESTAMP\": null\n}\n],\n\"summary\": {\n\"count\": 1\n}\n}\n";
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
    std::string expected_compact_json = "{\"entries\":[{\"ID\":1,\"LEVEL\":\"INFO\",\"MESSAGE\":\"Test Message\",\"TIMESTAMP\":null}],\"summary\":{\"count\":1}}\n"; // Added newline as per Exporter.cpp
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
    ASSERT_TRUE(j["entries"][0].contains("TIMESTAMP"));
    ASSERT_TRUE(j["entries"][0].contains("LEVEL"));
    ASSERT_TRUE(j["entries"][0].contains("MESSAGE"));
    ASSERT_TRUE(j["entries"][0].contains("custom_key"));
    ASSERT_FALSE(j["entries"][0].contains("another_key")); // Not in this entry

    // Entry 2 should have standard fields + another_key + custom_key
    ASSERT_TRUE(j["entries"][1].contains("ID"));
    ASSERT_TRUE(j["entries"][1].contains("TIMESTAMP")); // Field should be present
    ASSERT_TRUE(j["entries"][1]["TIMESTAMP"].is_null()); // Value should be null
    ASSERT_TRUE(j["entries"][1].contains("LEVEL"));
    ASSERT_TRUE(j["entries"][1].contains("MESSAGE"));
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
