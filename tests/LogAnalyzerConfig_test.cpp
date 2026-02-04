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
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

struct LogAnalyzerConfigTest : public ::testing::Test {};

TEST_F(LogAnalyzerConfigTest, DefaultConstructorInitializesCorrectly) {
    LogAnalyzerSettings settings;
    ASSERT_EQ(settings.lineParsePattern, DEFAULT_LOG_REGEX_PATTERN);
    ASSERT_FALSE(settings.fieldMappings.empty());
    ASSERT_EQ(settings.fieldMappings.size(), 3);
    ASSERT_TRUE(settings.customLogLevelMappings.empty());
    ASSERT_FALSE(settings.logEntryStartPattern.has_value());
    ASSERT_FALSE(settings.caseSensitiveParsing);
    ASSERT_TRUE(settings.filterRules.empty());
    ASSERT_EQ(settings.exportSettings.outputPath, "output.log");
    ASSERT_EQ(settings.exportSettings.format, ExportFormat::PLAINTEXT);
}

TEST_F(LogAnalyzerConfigTest, CustomPatternConstructorInitializesCorrectly) {
    std::string customPattern = R"(^(\d{2}-\d{2}-\d{4}) (.*)$)";
    LogAnalyzerSettings settings(customPattern);
    ASSERT_EQ(settings.lineParsePattern, customPattern);
    ASSERT_TRUE(settings.fieldMappings.empty());
}

TEST_F(LogAnalyzerConfigTest, FluentApiForParsing) {
    LogAnalyzerSettings settings;
    settings.setLineParsePattern("new_pattern")
            .setCaseSensitiveParsing(true)
            .setLogEntryStartPattern("start_line_pattern");
    ASSERT_EQ(settings.lineParsePattern, "new_pattern");
    ASSERT_TRUE(settings.caseSensitiveParsing);
    ASSERT_EQ(settings.logEntryStartPattern.value(), "start_line_pattern");

    settings.clearFieldMappings()
            .addFieldMapping(LogEntryField::MESSAGE, 1)
            .addFieldMapping(LogEntryField::SOURCE_FILE, 2, "my_format");
    ASSERT_EQ(settings.fieldMappings.size(), 2);
    ASSERT_EQ(std::get<LogEntryField>(settings.fieldMappings[0].field), LogEntryField::MESSAGE);
    ASSERT_EQ(std::get<LogEntryField>(settings.fieldMappings[1].field), LogEntryField::SOURCE_FILE);
}

TEST_F(LogAnalyzerConfigTest, FluentApiForFiltering) {
    LogAnalyzerSettings settings;
    FilterRule rule1{LogEntryField::LEVEL, FilterOperator::EQUALS, "ERROR"};
    settings.addFilterRule(rule1);
    ASSERT_EQ(settings.filterRules.size(), 1);
    ASSERT_EQ(settings.filterRules[0].field, LogEntryField::LEVEL);
}

TEST_F(LogAnalyzerConfigTest, FromJsonValid) {
    std::string jsonContent = R"({ 
        "lineParsePattern": "^(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}) ([A-Z]+): (.*)$",
        "fieldMappings": [
            {"field": "TIMESTAMP", "groupIndex": 1, "formats": ["%Y-%m-%d %H:%M:%S"]},
            {"field": "LEVEL", "groupIndex": 2},
            {"field": "MESSAGE", "groupIndex": 3}
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
    ASSERT_EQ(std::get<LogEntryField>(settings.fieldMappings[0].field), LogEntryField::TIMESTAMP);
}

TEST_F(LogAnalyzerConfigTest, ToJsonSerialization) {
    LogAnalyzerSettings settings;
    settings.setLineParsePattern("new_test_pattern")
            .setCaseSensitiveParsing(true)
            .clearFieldMappings()
            .addFieldMapping(LogEntryField::MESSAGE, 1);
    
    std::string jsonString = settings.toJson();
    nlohmann::json j = nlohmann::json::parse(jsonString);
    ASSERT_EQ(j["lineParsePattern"], "new_test_pattern");
    ASSERT_TRUE(j["caseSensitiveParsing"]);
}
