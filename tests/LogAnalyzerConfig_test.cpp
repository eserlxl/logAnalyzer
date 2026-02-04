#include "gtest/gtest.h"
#include "LogAnalyzerConfig.h"
#include "LogTypes.h"
#include "Filter.h"
#include "Exporter.h"
#include "Statistics.h"
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <fstream>      // Required for file operations
#include <filesystem>   // Required for std::filesystem::remove
#include <nlohmann/json.hpp> // Required for JSON parsing in tests

// Test fixture for LogAnalyzerSettings
struct LogAnalyzerConfigTest : public ::testing::Test {
    // No common setup needed for now, individual tests will create settings objects
};

TEST_F(LogAnalyzerConfigTest, DefaultConstructorInitializesCorrectly) {
    LogAnalyzerSettings settings;

    // Verify parsing configuration defaults
    ASSERT_EQ(settings.lineParsePattern, DEFAULT_LOG_REGEX_PATTERN);
    ASSERT_FALSE(settings.fieldMappings.empty()); // Should have default mappings
    ASSERT_EQ(settings.fieldMappings.size(), 3);
    ASSERT_TRUE(settings.customLogLevelMappings.empty());
    ASSERT_FALSE(settings.logEntryStartPattern.has_value());
    ASSERT_FALSE(settings.caseSensitiveParsing);

    // Verify filtering configuration defaults
    ASSERT_TRUE(settings.filterRules.empty());

    // Verify export configuration defaults
    ASSERT_EQ(settings.exportSettings.outputPath, "output.log");
    ASSERT_EQ(settings.exportSettings.format, ExportFormat::PLAINTEXT);
    ASSERT_FALSE(settings.exportSettings.fieldsToExport.empty()); // Should have default fields
    ASSERT_EQ(settings.exportSettings.fieldsToExport.size(), 3);
    ASSERT_TRUE(settings.exportSettings.includeHeader);

    // Verify statistics configuration defaults
    ASSERT_TRUE(settings.statisticConfigs.empty());
}

TEST_F(LogAnalyzerConfigTest, CustomPatternConstructorInitializesCorrectly) {
    std::string customPattern = R"(^(\d{2}-\d{2}-\d{4}) (.*)$)";
    LogAnalyzerSettings settings(customPattern);

    ASSERT_EQ(settings.lineParsePattern, customPattern);
    ASSERT_TRUE(settings.fieldMappings.empty()); // Should be cleared by this constructor
    ASSERT_TRUE(settings.customLogLevelMappings.empty());
    ASSERT_FALSE(settings.logEntryStartPattern.has_value());
    ASSERT_FALSE(settings.caseSensitiveParsing);
    ASSERT_TRUE(settings.filterRules.empty());
    // Export and stats should still have their defaults
    ASSERT_EQ(settings.exportSettings.outputPath, "output.log");
    ASSERT_TRUE(settings.statisticConfigs.empty());
}

TEST_F(LogAnalyzerConfigTest, FluentApiForParsing) {
    LogAnalyzerSettings settings;

    // Chain calls
    settings.setLineParsePattern("new_pattern")
            .setCaseSensitiveParsing(true)
            .setLogEntryStartPattern("start_line_pattern");

    ASSERT_EQ(settings.lineParsePattern, "new_pattern");
    ASSERT_TRUE(settings.caseSensitiveParsing);
    ASSERT_TRUE(settings.logEntryStartPattern.has_value());
    ASSERT_EQ(settings.logEntryStartPattern.value(), "start_line_pattern");

    settings.clearFieldMappings()
            .addFieldMapping(LogEntryField::MESSAGE, 1)
            .addFieldMapping(LogEntryField::SOURCE_FILE, 2, "my_format");

    ASSERT_EQ(settings.fieldMappings.size(), 2);
    ASSERT_EQ(settings.fieldMappings[0].field, LogEntryField::MESSAGE);
    ASSERT_EQ(settings.fieldMappings[0].groupIndex.value(), 1);
    ASSERT_EQ(settings.fieldMappings[1].field, LogEntryField::SOURCE_FILE);
    ASSERT_EQ(settings.fieldMappings[1].groupIndex.value(), 2);
    ASSERT_EQ(settings.fieldMappings[1].formats[0], "my_format");

    settings.addCustomLogLevelMapping("TRC", LogLevel::TRACE)
            .addCustomLogLevelMapping("DBG", LogLevel::DEBUG);

    ASSERT_EQ(settings.customLogLevelMappings.size(), 2);
    ASSERT_EQ(settings.customLogLevelMappings.at("TRC"), LogLevel::TRACE);
    ASSERT_EQ(settings.customLogLevelMappings.at("DBG"), LogLevel::DEBUG);

    settings.clearCustomLogLevelMappings();
    ASSERT_TRUE(settings.customLogLevelMappings.empty());
}

TEST_F(LogAnalyzerConfigTest, FluentApiForFiltering) {
    LogAnalyzerSettings settings;

    FilterRule rule1(LogEntryField::LEVEL, FilterOperator::EQUALS, "ERROR");
    FilterRule rule2(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "failure", true);

    settings.addFilterRule(rule1)
            .addFilterRule(rule2);

    ASSERT_EQ(settings.filterRules.size(), 2);
    ASSERT_EQ(settings.filterRules[0].field, LogEntryField::LEVEL);
    ASSERT_EQ(settings.filterRules[0].op, FilterOperator::EQUALS);
    ASSERT_EQ(settings.filterRules[0].value, "ERROR");
    ASSERT_FALSE(settings.filterRules[0].caseSensitive);

    ASSERT_EQ(settings.filterRules[1].field, LogEntryField::MESSAGE);
    ASSERT_EQ(settings.filterRules[1].op, FilterOperator::CONTAINS);
    ASSERT_EQ(settings.filterRules[1].value, "failure");
    ASSERT_TRUE(settings.filterRules[1].caseSensitive);

    settings.clearFilterRules();
    ASSERT_TRUE(settings.filterRules.empty());
}

TEST_F(LogAnalyzerConfigTest, FluentApiForExport) {
    LogAnalyzerSettings settings;

    ExportSettings customExportSettings;
    customExportSettings.outputPath = "custom_output.json";
    customExportSettings.format = ExportFormat::JSON;
    customExportSettings.fieldsToExport.clear();
    customExportSettings.fieldsToExport.emplace_back(LogEntryField::TIMESTAMP);
    customExportSettings.fieldsToExport.emplace_back(LogEntryField::MESSAGE, "LogMessage");
    customExportSettings.includeHeader = false;

    settings.setExportSettings(customExportSettings);

    ASSERT_EQ(settings.exportSettings.outputPath, "custom_output.json");
    ASSERT_EQ(settings.exportSettings.format, ExportFormat::JSON);
    ASSERT_EQ(settings.exportSettings.fieldsToExport.size(), 2);
    ASSERT_EQ(settings.exportSettings.fieldsToExport[0].field, LogEntryField::TIMESTAMP);
    ASSERT_EQ(settings.exportSettings.fieldsToExport[0].customHeader, "");
    ASSERT_EQ(settings.exportSettings.fieldsToExport[1].field, LogEntryField::MESSAGE);
    ASSERT_EQ(settings.exportSettings.fieldsToExport[1].customHeader, "LogMessage");
    ASSERT_FALSE(settings.exportSettings.includeHeader);
}

TEST_F(LogAnalyzerConfigTest, FluentApiForStatistics) {
    LogAnalyzerSettings settings;

    StatisticConfig stat1(StatisticType::COUNT_BY_LEVEL);
    StatisticConfig stat2(StatisticType::TOP_N_OCCURRENCES, LogEntryField::MESSAGE, 10);
    StatisticConfig stat3(StatisticType::OCCURRENCE_COUNT, LogEntryField::LEVEL, "ERROR");

    settings.addStatisticConfig(stat1)
            .addStatisticConfig(stat2)
            .addStatisticConfig(stat3);

    ASSERT_EQ(settings.statisticConfigs.size(), 3);
    ASSERT_EQ(settings.statisticConfigs[0].type, StatisticType::COUNT_BY_LEVEL);
    ASSERT_FALSE(settings.statisticConfigs[0].field.has_value());

    ASSERT_EQ(settings.statisticConfigs[1].type, StatisticType::TOP_N_OCCURRENCES);
    ASSERT_TRUE(settings.statisticConfigs[1].field.has_value());
    ASSERT_EQ(settings.statisticConfigs[1].field.value(), LogEntryField::MESSAGE);
    ASSERT_TRUE(settings.statisticConfigs[1].topN.has_value());
    ASSERT_EQ(settings.statisticConfigs[1].topN.value(), 10);

    ASSERT_EQ(settings.statisticConfigs[2].type, StatisticType::OCCURRENCE_COUNT);
    ASSERT_TRUE(settings.statisticConfigs[2].field.has_value());
    ASSERT_EQ(settings.statisticConfigs[2].field.value(), LogEntryField::LEVEL);
    ASSERT_TRUE(settings.statisticConfigs[2].pattern.has_value());
    ASSERT_EQ(settings.statisticConfigs[2].pattern.value(), "ERROR");

    settings.clearStatisticConfigs();
    ASSERT_TRUE(settings.statisticConfigs.empty());
}

// --- New Tests for Iteration 5 Features ---

// Test for fromJson with valid JSON content
TEST_F(LogAnalyzerConfigTest, FromJsonValid) {
    std::string jsonContent = R"({
        "lineParsePattern": "^(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}) ([A-Z]+): (.*)$",
        "fieldMappings": [
            {"field": "TIMESTAMP", "captureGroupIndex": 1, "formats": ["%Y-%m-%d %H:%M:%S"]},
            {"field": "LEVEL", "captureGroupIndex": 2},
            {"field": "MESSAGE", "captureGroupIndex": 3}
        ],
        "customLogLevelMappings": {"TRC": "TRACE", "DBG": "DEBUG"},
        "filterRules": [
            {"field": "LEVEL", "op": "EQUALS", "value": "ERROR", "caseSensitive": false}
        ],
        "exportSettings": {
            "outputPath": "exports/filtered.json",
            "format": "JSON",
            "fieldsToExport": [
                {"field": "TIMESTAMP"},
                {"field": "MESSAGE", "customHeader": "Msg"}
            ],
            "includeHeader": true
        },
        "statisticConfigs": [
            {"type": "COUNT_BY_LEVEL"}
        ]
    })";
    auto result = LogAnalyzerSettings::fromJson(jsonContent);
    ASSERT_TRUE(result.has_value());
    LogAnalyzerSettings settings = result.value();

    ASSERT_EQ(settings.lineParsePattern, "^(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}) ([A-Z]+): (.*)$");
    ASSERT_EQ(settings.fieldMappings.size(), 3);
    ASSERT_EQ(settings.fieldMappings[0].field, LogEntryField::TIMESTAMP);
    ASSERT_EQ(settings.fieldMappings[0].formats.size(), 1);
    ASSERT_EQ(settings.fieldMappings[0].formats[0], "%Y-%m-%d %H:%M:%S");
    ASSERT_EQ(settings.customLogLevelMappings.size(), 2);
    ASSERT_EQ(settings.customLogLevelMappings.at("TRC"), LogLevel::TRACE);
    ASSERT_EQ(settings.filterRules.size(), 1);
    ASSERT_EQ(settings.filterRules[0].field, LogEntryField::LEVEL);
    ASSERT_EQ(settings.exportSettings.outputPath, "exports/filtered.json");
    ASSERT_EQ(settings.exportSettings.format, ExportFormat::JSON);
    ASSERT_EQ(settings.exportSettings.fieldsToExport.size(), 2);
    ASSERT_EQ(settings.exportSettings.fieldsToExport[1].customHeader, "Msg");
    ASSERT_EQ(settings.statisticConfigs.size(), 1);
    ASSERT_EQ(settings.statisticConfigs[0].type, StatisticType::COUNT_BY_LEVEL);
}

// Test for fromJson with invalid/malformed JSON content
TEST_F(LogAnalyzerConfigTest, FromJsonInvalid) {
    std::string invalidJson = R"({"lineParsePattern": "^(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}) ([A-Z]+): (.*)$", )"; // Malformed JSON
    auto result = LogAnalyzerSettings::fromJson(invalidJson);
    ASSERT_FALSE(result.has_value());
    ASSERT_FALSE(result.error().empty()); // Expect error messages
}

// Test for validate with default settings (should be valid)
TEST_F(LogAnalyzerConfigTest, ValidateValidSettings) {
    LogAnalyzerSettings settings; // Default settings should be valid
    auto errors = settings.validate();
    ASSERT_TRUE(errors.empty()) << "Default settings should be valid.";
}

// Test for validate with invalid settings.
// NOTE: The implementation for validate() is not yet present. This test is a placeholder.
// It is written to ensure the test structure exists and to provide a basis for future
// implementation once validate() is actually coded.
TEST_F(LogAnalyzerConfigTest, ValidateInvalidSettingsPlaceholder) {
    LogAnalyzerSettings settings;
    // Example: To make settings invalid, we might try to set an invalid regex pattern or
    // provide a field mapping that doesn't correspond to a capture group.
    // However, without the actual implementation of validate(), we can only
    // assert that calling it doesn't crash and returns an empty error list for now.

    // settings.lineParsePattern = "[invalid regex"; // Example of potential invalid setting

    auto errors = settings.validate();
    ASSERT_TRUE(errors.empty()) << "Placeholder for invalid settings validation. Actual errors would be expected here after implementation.";
}

TEST_F(LogAnalyzerConfigTest, FromFileInputValidFile) {
    std::string filePath = "/tmp/test_config.json";
    // The content for this file was written in the previous step.
    
    auto result = LogAnalyzerSettings::fromFile(filePath);
    ASSERT_TRUE(result.has_value()) << "Expected valid settings from file, but got error: " << (result.has_value() ? "" : result.error()[0]);
    LogAnalyzerSettings settings = result.value();

    ASSERT_EQ(settings.lineParsePattern, "^(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}) ([A-Z]+): (.*)$");
    ASSERT_EQ(settings.fieldMappings.size(), 3);
    ASSERT_EQ(settings.fieldMappings[0].field, LogEntryField::TIMESTAMP);
    ASSERT_EQ(settings.fieldMappings[0].formats.size(), 1);
    ASSERT_EQ(settings.fieldMappings[0].formats[0], "%Y-%m-%d %H:%M:%S");
    ASSERT_EQ(settings.customLogLevelMappings.size(), 2);
    ASSERT_EQ(settings.customLogLevelMappings.at("TRC"), LogLevel::TRACE);
    ASSERT_EQ(settings.filterRules.size(), 1);
    ASSERT_EQ(settings.filterRules[0].field, LogEntryField::LEVEL);
    ASSERT_EQ(settings.exportSettings.outputPath, "exports/filtered.json");
    ASSERT_EQ(settings.exportSettings.format, ExportFormat::JSON);
    ASSERT_EQ(settings.exportSettings.fieldsToExport.size(), 2);
    ASSERT_EQ(settings.exportSettings.fieldsToExport[1].customHeader, "Msg");
    ASSERT_EQ(settings.statisticConfigs.size(), 1);
    ASSERT_EQ(settings.statisticConfigs[0].type, StatisticType::COUNT_BY_LEVEL);

    // Clean up the temporary file
    std::filesystem::remove(filePath);
}

TEST_F(LogAnalyzerConfigTest, FromFileNonExistent) {
    std::string nonExistentPath = "/tmp/non_existent_config.json";
    auto result = LogAnalyzerSettings::fromFile(nonExistentPath);
    ASSERT_FALSE(result.has_value());
    ASSERT_FALSE(result.error().empty());
    ASSERT_EQ(result.error()[0], "Failed to open configuration file: " + nonExistentPath);
}

TEST_F(LogAnalyzerConfigTest, ToJsonSerialization) {
    LogAnalyzerSettings settings;
    settings.setLineParsePattern("new_test_pattern")
            .setCaseSensitiveParsing(true)
            .clearFieldMappings()
            .addFieldMapping(LogEntryField::MESSAGE, 1)
            .addFieldMapping(LogEntryField::THREAD_ID, 2, "myThreadFormat");
    settings.addCustomLogLevelMapping("VRB", LogLevel::TRACE);
    settings.addFilterRule({LogEntryField::LEVEL, FilterOperator::EQUALS, "WARN", false});
    
    ExportSettings es;
    es.outputPath = "output.csv";
    es.format = ExportFormat::CSV;
    es.includeHeader = false;
    es.fieldsToExport.clear();
    es.fieldsToExport.emplace_back(LogEntryField::TIMESTAMP, "Time", "%H:%M:%S");
    es.fieldsToExport.emplace_back(LogEntryField::MESSAGE);
    settings.setExportSettings(es);

    settings.addStatisticConfig({StatisticType::COUNT_BY_LEVEL});
    settings.addStatisticConfig({StatisticType::TOP_N_OCCURRENCES, LogEntryField::MESSAGE, 5});

    std::string jsonString = settings.toJson();

    // Now, parse the generated JSON and verify its content
    nlohmann::json j = nlohmann::json::parse(jsonString);

    ASSERT_EQ(j["lineParsePattern"], "new_test_pattern");
    ASSERT_TRUE(j["caseSensitiveParsing"]);
    
    ASSERT_EQ(j["fieldMappings"].size(), 2);
    ASSERT_EQ(j["fieldMappings"][0]["field"], "MESSAGE");
    ASSERT_EQ(j["fieldMappings"][0]["groupIndex"], 1);
    ASSERT_TRUE(j["fieldMappings"][0]["formats"].empty());

    ASSERT_EQ(j["fieldMappings"][1]["field"], "THREAD_ID");
    ASSERT_EQ(j["fieldMappings"][1]["groupIndex"], 2);
    ASSERT_EQ(j["fieldMappings"][1]["formats"].size(), 1);
    ASSERT_EQ(j["fieldMappings"][1]["formats"][0], "myThreadFormat");

    ASSERT_EQ(j["customLogLevelMappings"].size(), 1);
    ASSERT_EQ(j["customLogLevelMappings"]["VRB"], "TRACE");

    ASSERT_EQ(j["filterRules"].size(), 1);
    ASSERT_EQ(j["filterRules"][0]["field"], "LEVEL");
    ASSERT_EQ(j["filterRules"][0]["op"], "EQUALS");
    ASSERT_EQ(j["filterRules"][0]["value"], "WARN");
    ASSERT_FALSE(j["filterRules"][0]["caseSensitive"]);

    ASSERT_EQ(j["exportSettings"]["outputPath"], "output.csv");
    ASSERT_EQ(j["exportSettings"]["format"], "CSV");
    ASSERT_FALSE(j["exportSettings"]["includeHeader"]);
    ASSERT_EQ(j["exportSettings"]["fieldsToExport"].size(), 2);
    ASSERT_EQ(j["exportSettings"]["fieldsToExport"][0]["field"], "TIMESTAMP");
    ASSERT_EQ(j["exportSettings"]["fieldsToExport"][0]["customHeader"], "Time");
    ASSERT_EQ(j["exportSettings"]["fieldsToExport"][0]["datetimeFormat"], "%H:%M:%S");


    ASSERT_EQ(j["statisticConfigs"].size(), 2);
    ASSERT_EQ(j["statisticConfigs"][0]["type"], "COUNT_BY_LEVEL");
    ASSERT_EQ(j["statisticConfigs"][1]["type"], "TOP_N_OCCURRENCES");
    ASSERT_EQ(j["statisticConfigs"][1]["field"], "MESSAGE");
    ASSERT_EQ(j["statisticConfigs"][1]["topN"], 5);
}
