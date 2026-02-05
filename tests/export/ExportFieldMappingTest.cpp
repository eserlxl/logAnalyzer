// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 eserlxl

#include "tests/export/ExportTestUtils.h"

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
