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
