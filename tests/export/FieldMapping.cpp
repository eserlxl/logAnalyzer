// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "Utils.h"

using json = nlohmann::json;

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
    ExportFieldMapping efm;
    ASSERT_NO_THROW(efm = j.get<ExportFieldMapping>());
    ASSERT_TRUE(std::holds_alternative<std::string>(efm.field));
    ASSERT_EQ(std::get<std::string>(efm.field), "INVALID_FIELD");
}

TEST(ExportFieldMappingTest, FromJsonMissingField) {
    json j = {{"customHeader", "Header"}};
    ASSERT_THROW(j.get<ExportFieldMapping>(), ExportException);
}

TEST(ExportFieldMappingTest, FromJsonEmptyFieldStringRejected) {
    json j = {{"field", ""}};
    ASSERT_THROW(j.get<ExportFieldMapping>(), ExportException);
}

TEST(ExportFieldMappingTest, FromJsonInvalidFieldType) {
    json j = {{"field", 123}};
    ASSERT_THROW(j.get<ExportFieldMapping>(), ExportException);
}

TEST(ExportFieldMappingTest, FromJsonInvalidCustomHeaderType) {
    json j = {{"field", "LEVEL"}, {"customHeader", 123}};
    ASSERT_THROW(j.get<ExportFieldMapping>(), ExportException);
}

TEST(ExportFieldMappingTest, FromJsonNullCustomHeaderClearsAlias) {
    json j = {{"field", "LEVEL"}, {"customHeader", nullptr}};
    ExportFieldMapping efm = j.get<ExportFieldMapping>();
    ASSERT_TRUE(efm.customHeader.empty());
}

TEST(ExportFieldMappingTest, FromJsonInvalidDatetimeFormatType) {
    json j = {{"field", "TIMESTAMP"}, {"datetimeFormat", 123}};
    ASSERT_THROW(j.get<ExportFieldMapping>(), ExportException);
}

TEST(ExportFieldMappingTest, FromJsonNullDatetimeFormatClearsOptional) {
    json j = {{"field", "TIMESTAMP"}, {"datetimeFormat", nullptr}};
    ExportFieldMapping efm = j.get<ExportFieldMapping>();
    ASSERT_FALSE(efm.datetimeFormat.has_value());
}

TEST(ExportFieldMappingTest, FromJsonGetToResetsPreviousStateWhenKeysMissing) {
    ExportFieldMapping efm(LogEntryField::TIMESTAMP, "OldHeader", "%Y-%m-%d");
    json j = {{"field", "LEVEL"}};
    j.get_to(efm);
    ASSERT_TRUE(std::holds_alternative<LogEntryField>(efm.field));
    ASSERT_EQ(std::get<LogEntryField>(efm.field), LogEntryField::LEVEL);
    ASSERT_TRUE(efm.customHeader.empty());
    ASSERT_FALSE(efm.datetimeFormat.has_value());
}
