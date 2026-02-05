// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 eserlxl

#include "gtest/gtest.h"
#include "config/Settings.h"
#include "core/LogTypes.h"
#include "export/Exporter.h" // For ExportFieldMapping
#include <string>
#include <algorithm> // For std::find_if
#include <vector>

struct LogAnalyzerConfigTest : public ::testing::Test {
    LogAnalyzerSettings settings;
};

TEST_F(LogAnalyzerConfigTest, DefaultConstructorInitializesCorrectly) {
    ASSERT_EQ(settings.lineParsePattern, std::string(DEFAULT_LOG_REGEX_PATTERN_INTERNAL));
    ASSERT_FALSE(settings.fieldMappings.empty());
    ASSERT_EQ(settings.fieldMappings.size(), 3); // Default mappings
    ASSERT_TRUE(settings.customLogLevelMappings.empty());
    ASSERT_FALSE(settings.logEntryStartPattern.has_value());
    ASSERT_FALSE(settings.caseSensitiveParsing);
    ASSERT_TRUE(settings.filterRules.empty());
    ASSERT_EQ(settings.exportSettings.outputPath, "output.log");
    ASSERT_EQ(settings.exportSettings.format, ExportFormat::PLAINTEXT);
}

TEST_F(LogAnalyzerConfigTest, CustomPatternConstructorInitializesCorrectly) {
    std::string customPattern = R"(^(\d{2}-\d{2}-\d{4}) (.*)$)";
    LogAnalyzerSettings customSettings(customPattern);
    ASSERT_EQ(customSettings.lineParsePattern, customPattern);
    // Default mappings should still be present
    ASSERT_FALSE(customSettings.fieldMappings.empty());
    ASSERT_EQ(customSettings.fieldMappings.size(), 3);
}

TEST_F(LogAnalyzerConfigTest, FluentApiForCoreParsingSettings) {
    settings.setLineParsePattern("new_pattern")
            .setCaseSensitiveParsing(true)
            .setLogEntryStartPattern("start_line_pattern");

    ASSERT_EQ(settings.lineParsePattern, "new_pattern");
    ASSERT_TRUE(settings.caseSensitiveParsing);
    ASSERT_TRUE(settings.logEntryStartPattern.has_value());
    ASSERT_EQ(settings.logEntryStartPattern.value(), "start_line_pattern");
}

TEST_F(LogAnalyzerConfigTest, SetEmptyLogEntryStartPattern) {
    settings.setLogEntryStartPattern("");
    ASSERT_FALSE(settings.logEntryStartPattern.has_value());
}

TEST_F(LogAnalyzerConfigTest, FieldMappingFluentApi) {
    settings.clearFieldMappings()
            .addFieldMapping(LogEntryField::MESSAGE, 1)
            .addFieldMapping(LogEntryField::SOURCE_FILE, 2, "my_format")
            .addFieldMapping("CustomField", 3, "custom_format");

    ASSERT_EQ(settings.fieldMappings.size(), 3);

    // Verify MESSAGE mapping (using std::find_if for robustness)
    auto msgIt = std::find_if(settings.fieldMappings.begin(), settings.fieldMappings.end(),
        [](const auto& mapping) {
            return std::holds_alternative<LogEntryField>(mapping.field) &&
                   std::get<LogEntryField>(mapping.field) == LogEntryField::MESSAGE;
        });
    ASSERT_NE(msgIt, settings.fieldMappings.end());
    ASSERT_TRUE(msgIt->groupIndex.has_value());
    EXPECT_EQ(msgIt->groupIndex.value(), 1);
    EXPECT_TRUE(msgIt->formats.empty()); // No format provided for MESSAGE

    // Verify SOURCE_FILE mapping
    auto fileIt = std::find_if(settings.fieldMappings.begin(), settings.fieldMappings.end(),
        [](const auto& mapping) {
            return std::holds_alternative<LogEntryField>(mapping.field) &&
                   std::get<LogEntryField>(mapping.field) == LogEntryField::SOURCE_FILE;
        });
    ASSERT_NE(fileIt, settings.fieldMappings.end());
    ASSERT_TRUE(fileIt->groupIndex.has_value());
    EXPECT_EQ(fileIt->groupIndex.value(), 2);
    ASSERT_FALSE(fileIt->formats.empty());
    EXPECT_EQ(fileIt->formats[0], "my_format");

    // Verify CustomField mapping
    auto customIt = std::find_if(settings.fieldMappings.begin(), settings.fieldMappings.end(),
        [](const auto& mapping) {
            return std::holds_alternative<std::string>(mapping.field) &&
                   std::get<std::string>(mapping.field) == "CustomField";
        });
    ASSERT_NE(customIt, settings.fieldMappings.end());
    ASSERT_TRUE(customIt->groupIndex.has_value());
    EXPECT_EQ(customIt->groupIndex.value(), 3);
    ASSERT_FALSE(customIt->formats.empty());
    EXPECT_EQ(customIt->formats[0], "custom_format");
}

TEST_F(LogAnalyzerConfigTest, ClearFieldMappings) {
    ASSERT_FALSE(settings.fieldMappings.empty());
    settings.clearFieldMappings();
    ASSERT_TRUE(settings.fieldMappings.empty());
    // Test clearing an already empty collection
    settings.clearFieldMappings();
    ASSERT_TRUE(settings.fieldMappings.empty());
}

TEST_F(LogAnalyzerConfigTest, AddDuplicateFieldMapping) {
    // Behavior: Last one wins (overwrite)
    settings.clearFieldMappings()
            .addFieldMapping(LogEntryField::MESSAGE, 1, "old_format")
            .addFieldMapping(LogEntryField::MESSAGE, 2, "new_format");

    ASSERT_EQ(settings.fieldMappings.size(), 1);
    auto it = std::find_if(settings.fieldMappings.begin(), settings.fieldMappings.end(),
        [](const auto& mapping) {
            return std::holds_alternative<LogEntryField>(mapping.field) &&
                   std::get<LogEntryField>(mapping.field) == LogEntryField::MESSAGE;
        });
    ASSERT_NE(it, settings.fieldMappings.end());
    ASSERT_TRUE(it->groupIndex.has_value());
    EXPECT_EQ(it->groupIndex.value(), 2); // Assert it was updated
    ASSERT_FALSE(it->formats.empty());
    EXPECT_EQ(it->formats[0], "new_format");

    settings.clearFieldMappings()
            .addFieldMapping("CustomField", 10, "old_custom")
            .addFieldMapping("CustomField", 20, "new_custom");

    ASSERT_EQ(settings.fieldMappings.size(), 1);
    auto customIt = std::find_if(settings.fieldMappings.begin(), settings.fieldMappings.end(),
        [](const auto& mapping) {
            return std::holds_alternative<std::string>(mapping.field) &&
                   std::get<std::string>(mapping.field) == "CustomField";
        });
    ASSERT_NE(customIt, settings.fieldMappings.end());
    ASSERT_TRUE(customIt->groupIndex.has_value());
    EXPECT_EQ(customIt->groupIndex.value(), 20); // Assert it was updated
    ASSERT_FALSE(customIt->formats.empty());
    EXPECT_EQ(customIt->formats[0], "new_custom");
}


TEST_F(LogAnalyzerConfigTest, CustomLogLevelMappingApi) {
    settings.addCustomLogLevelMapping("WRN", LogLevel::WARNING)
            .addCustomLogLevelMapping("INF", LogLevel::INFO);

    ASSERT_EQ(settings.customLogLevelMappings.size(), 2);
    EXPECT_EQ(settings.customLogLevelMappings["WRN"], LogLevel::WARNING);
    EXPECT_EQ(settings.customLogLevelMappings["INF"], LogLevel::INFO);

    settings.clearCustomLogLevelMappings();
    ASSERT_TRUE(settings.customLogLevelMappings.empty());
}

TEST_F(LogAnalyzerConfigTest, FluentApiForFiltering) {
    FilterRule rule1{LogEntryField::LEVEL, FilterOperator::EQUALS, "ERROR"};
    FilterRule rule2{LogEntryField::MESSAGE, FilterOperator::CONTAINS, "critical"};
    settings.addFilterRule(rule1)
            .addFilterRule(rule2);

    ASSERT_EQ(settings.filterRules.size(), 2);

    // Verify rule1 exists
    auto rule1It = std::find_if(settings.filterRules.begin(), settings.filterRules.end(),
        [](const auto& rule) { return rule.field == LogEntryField::LEVEL; });
    ASSERT_NE(rule1It, settings.filterRules.end());
    EXPECT_EQ(rule1It->op, FilterOperator::EQUALS);
    EXPECT_EQ(rule1It->value, "ERROR");

    // Verify rule2 exists
    auto rule2It = std::find_if(settings.filterRules.begin(), settings.filterRules.end(),
        [](const auto& rule) { return rule.field == LogEntryField::MESSAGE; });
    ASSERT_NE(rule2It, settings.filterRules.end());
    EXPECT_EQ(rule2It->op, FilterOperator::CONTAINS);
    EXPECT_EQ(rule2It->value, "critical");
}

TEST_F(LogAnalyzerConfigTest, ClearFilterRules) {
    settings.addFilterRule({LogEntryField::LEVEL, FilterOperator::EQUALS, "ERROR"});
    ASSERT_FALSE(settings.filterRules.empty());
    settings.clearFilterRules();
    ASSERT_TRUE(settings.filterRules.empty());
}

TEST_F(LogAnalyzerConfigTest, ExportSettingsFluentApi) {
    std::vector<ExportFieldMapping> exportFields = {
        {LogEntryField::TIMESTAMP, "Time", "%Y-%m-%d %H:%M:%S"},
        {LogEntryField::MESSAGE, "LogMessage"}
    };

    settings.setExportPath("results.json")
            .setExportFormat(ExportFormat::JSON)
            .setExportFieldsToExport(exportFields);

    const auto& exportSettings = settings.exportSettings;
    EXPECT_EQ(exportSettings.outputPath, "results.json");
    EXPECT_EQ(exportSettings.format, ExportFormat::JSON);
    ASSERT_EQ(exportSettings.fieldsToExport.size(), 2);
    
    // Verify the first exported field
    EXPECT_EQ(exportSettings.fieldsToExport[0].field, LogEntryField::TIMESTAMP);
    EXPECT_EQ(exportSettings.fieldsToExport[0].customHeader, "Time");
    ASSERT_TRUE(exportSettings.fieldsToExport[0].datetimeFormat.has_value());
    EXPECT_EQ(exportSettings.fieldsToExport[0].datetimeFormat.value(), "%Y-%m-%d %H:%M:%S");

    // Verify the second exported field
    EXPECT_EQ(exportSettings.fieldsToExport[1].field, LogEntryField::MESSAGE);
    EXPECT_EQ(exportSettings.fieldsToExport[1].customHeader, "LogMessage");
    ASSERT_FALSE(exportSettings.fieldsToExport[1].datetimeFormat.has_value());
}
