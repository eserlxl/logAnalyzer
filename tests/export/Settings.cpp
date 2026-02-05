// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "tests/export/TestUtils.h"

using json = nlohmann::json;

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
