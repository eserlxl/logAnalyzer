#include <gtest/gtest.h>
#include <sstream>
#include <vector>
#include <nlohmann/json.hpp>
#include "export/Exporter.h"
#include "core/LogTypes.h"
#include "utils/Utils.h"

using json = nlohmann::json;

// Helper function to create a LogEntry
LogEntry createLogEntry(int id, LogLevel level, const std::string& message,
                        std::optional<std::chrono::system_clock::time_point> timestamp = std::nullopt,
                        const std::map<std::string, std::string>& customFields = {}) {
    LogEntry entry;
    entry.id = id;
    entry.level = level;
    entry.message = message;
    entry.timestamp = timestamp;
    for (const auto& field : customFields) {
        entry.customFields[field.first] = field.second;
    }
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
    ASSERT_EQ(efm.field, LogEntryField::LEVEL);
    ASSERT_EQ(efm.customHeader, "Lvl");
    ASSERT_FALSE(efm.datetimeFormat.has_value());

    json j2 = {{"field", "TIMESTAMP"}, {"datetimeFormat", "%H:%M:%S"}};
    ExportFieldMapping efm2 = j2;
    ASSERT_EQ(efm2.field, LogEntryField::TIMESTAMP);
    ASSERT_EQ(efm2.customHeader, "");
    ASSERT_TRUE(efm2.datetimeFormat.has_value());
    ASSERT_EQ(efm2.datetimeFormat.value(), "%H:%M:%S");
}

TEST(ExportFieldMappingTest, FromJsonInvalidField) {
    json j = {{"field", "INVALID_FIELD"}};
    ASSERT_THROW(j.get<ExportFieldMapping>(), std::runtime_error);
}

TEST(ExportFieldMappingTest, FromJsonMissingField) {
    json j = {{"customHeader", "Header"}};
    ASSERT_THROW(j.get<ExportFieldMapping>(), std::runtime_error);
}

TEST(ExportFieldMappingTest, FromJsonInvalidFieldType) {
    json j = {{"field", 123}};
    ASSERT_THROW(j.get<ExportFieldMapping>(), std::runtime_error);
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
    ASSERT_EQ(es.fieldsToExport[0].field, LogEntryField::ID);
    ASSERT_EQ(es.fieldsToExport[0].customHeader, "EntryID");
    ASSERT_EQ(es.fieldsToExport[1].field, LogEntryField::MESSAGE);
}

TEST(ExportSettingsTest, FromJsonInvalidOutputPathType) {
    json j = {{"outputPath", 123}};
    ASSERT_THROW(j.get<ExportSettings>(), std::runtime_error);
}

TEST(ExportSettingsTest, FromJsonInvalidFormatType) {
    json j = {{"format", 123}};
    ASSERT_THROW(j.get<ExportSettings>(), std::runtime_error);
}

TEST(ExportSettingsTest, FromJsonInvalidFormatValue) {
    json j = {{"format", "UNKNOWN_FORMAT"}};
    ASSERT_THROW(j.get<ExportSettings>(), std::runtime_error);
}

TEST(ExportSettingsTest, FromJsonInvalidFieldsToExportType) {
    json j = {{"fieldsToExport", "not_an_array"}};
    ASSERT_THROW(j.get<ExportSettings>(), std::runtime_error);
}

TEST(ExportSettingsTest, FromJsonInvalidIncludeHeaderType) {
    json j = {{"includeHeader", "not_a_boolean"}};
    ASSERT_THROW(j.get<ExportSettings>(), std::runtime_error);
}

TEST(ExportSettingsTest, FromJsonInvalidJsonIndentType) {
    json j = {{"jsonIndent", "not_an_int"}};
    ASSERT_THROW(j.get<ExportSettings>(), std::runtime_error);
}

TEST(ExportSettingsTest, FromJsonInvalidSeparatorType) {
    json j = {{"separator", 123}};
    ASSERT_THROW(j.get<ExportSettings>(), std::runtime_error);
}

TEST(ExportSettingsTest, FromJsonInvalidSeparatorLength) {
    json j = {{"separator", "ab"}};
    ASSERT_THROW(j.get<ExportSettings>(), std::runtime_error);
}

TEST(ExportSettingsTest, FromJsonInvalidTextFormatStringType) {
    json j = {{"textFormatString", 123}};
    ASSERT_THROW(j.get<ExportSettings>(), std::runtime_error);
}

TEST(ExportSettingsTest, FromJsonInvalidUseAnsiColorsType) {
    json j = {{"useAnsiColors", "false"}}; // String "false" is not boolean false
    ASSERT_THROW(j.get<ExportSettings>(), std::runtime_error);
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
    ASSERT_FALSE(j["entries"][1].contains("EventTime")); // No timestamp for this entry
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
    ASSERT_FALSE(j["entries"][2].contains("EventTime")); // No timestamp for this entry
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
    ASSERT_TRUE(output.find("ID,Timestamp,Level,Message,ip,session,user\n") != std::string::npos || // sorted custom fields
                output.find("ID,Timestamp,Level,Message,session,user,ip\n") != std::string::npos ||
                output.find("ID,Timestamp,Level,Message,user,session,ip\n") != std::string::npos); // just check if it contains the fields, specific order might vary

    // Check row content
    // Row 1: ID, Timestamp, Level, Message, ip, session, user
    ASSERT_TRUE(output.find("1,\"\",INFO,Msg1,\"\",123,alice\n") != std::string::npos);
    // Row 2: ID, Timestamp, Level, Message, ip, session, user
    ASSERT_TRUE(output.find("2,\"\",WARNING,Msg2,127.0.0.1,456,\"\"\n") != std::string::npos);
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
    auto tp = Utils::parseTime("2023-10-27 10:30:00.123").value();
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

TEST(ExporterDispatchTest, UnknownFormatWarning) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Message"));

    std::stringstream ss_out;
    std::stringstream ss_err;
    // Redirect cerr to ss_err
    std::streambuf* oldCerr = std::cerr.rdbuf();
    std::cerr.rdbuf(ss_err.rdbuf());

    ExportSettings settings;
    settings.format = ExportFormat::UNKNOWN;

    exporter.exportLogEntries(ss_out, entries, settings);

    // Restore cerr
    std::cerr.rdbuf(oldCerr);

    ASSERT_TRUE(ss_out.str().empty());
    ASSERT_TRUE(ss_err.str().find("Error: Unknown export format.") != std::string::npos);
}
