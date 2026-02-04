#include "gtest/gtest.h"
#include <gmock/gmock.h>
#include "config/Config.h"
#include "core/LogTypes.h"
#include <string>

struct LogAnalyzerConfigTest : public ::testing::Test {};

TEST_F(LogAnalyzerConfigTest, DefaultConstructorInitializesCorrectly) {
    LogAnalyzerSettings settings;
    ASSERT_EQ(settings.lineParsePattern, std::string(DEFAULT_LOG_REGEX_PATTERN_SV));
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
    ASSERT_FALSE(settings.fieldMappings.empty());
    ASSERT_EQ(settings.fieldMappings.size(), 3); // Default mappings are always added
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
