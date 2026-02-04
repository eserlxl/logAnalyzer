#include "gtest/gtest.h"
#include <gmock/gmock.h>
#include "Config.h"
#include "LogTypes.h"
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <regex> // For std::regex

struct LogAnalyzerConfigTest : public ::testing::Test {};

TEST_F(LogAnalyzerConfigTest, DefaultRegexPatternCompiles) {
    ASSERT_NO_THROW({
        std::regex re(DEFAULT_LOG_REGEX_PATTERN_SV.data(), DEFAULT_LOG_REGEX_PATTERN_SV.size());
    });
}

TEST_F(LogAnalyzerConfigTest, ValidateErrorHandling) {
    LogAnalyzerSettings settings;
    std::vector<std::string> errors;

    // 1. Invalid regex pattern
    settings.lineParsePattern = "[invalid regex";
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    ASSERT_THAT(errors[0], testing::HasSubstr("Invalid regex pattern for 'lineParsePattern'"));
    settings.lineParsePattern = std::string(DEFAULT_LOG_REGEX_PATTERN_SV); // Reset to valid

    // 2. FieldMapping missing groupIndex
    settings.fieldMappings = {
        FieldMapping{LogEntryField::TIMESTAMP, std::nullopt} // Missing groupIndex
    };
    settings.exportSettings.fieldsToExport = { ExportFieldMapping{LogEntryField::MESSAGE} }; // Ensure exportSettings is valid for standalone validation
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    ASSERT_THAT(errors[0], testing::HasSubstr("FieldMapping is missing groupIndex."));
    settings.fieldMappings = {}; // Clear for next test


    // 3. FilterRule with UNKNOWN field or operator
    settings.filterRules = {
        FilterRule{LogEntryField::UNKNOWN, FilterOperator::EQUALS, "ERROR"}
    };
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    ASSERT_THAT(errors[0], testing::HasSubstr("FilterRule has an unrecognized field."));
    settings.filterRules = {
        FilterRule{LogEntryField::LEVEL, FilterOperator::UNKNOWN, "ERROR"}
    };
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    ASSERT_THAT(errors[0], testing::HasSubstr("FilterRule has an unrecognized operator."));
    settings.filterRules = {}; // Clear for next test

    // 4. ExportSettings with empty fieldsToExport
    settings.exportSettings.fieldsToExport = {};
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    ASSERT_THAT(errors[0], testing::HasSubstr("ExportSettings 'fieldsToExport' cannot be empty."));
    settings.exportSettings.fieldsToExport = { ExportFieldMapping{LogEntryField::MESSAGE} }; // Reset to valid

    // 5. StatisticConfig with UNKNOWN type
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::UNKNOWN}
    };
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    ASSERT_THAT(errors[0], testing::HasSubstr("StatisticConfig has an unrecognized type."));
    settings.statisticConfigs = {}; // Clear for next test

    // Test multiple errors
    settings.lineParsePattern = "[invalid regex"; // Invalid regex
    settings.fieldMappings = { FieldMapping{LogEntryField::TIMESTAMP, std::nullopt} }; // Missing groupIndex
    settings.exportSettings.fieldsToExport = {}; // Empty fieldsToExport
    settings.filterRules = { FilterRule{LogEntryField::UNKNOWN, FilterOperator::UNKNOWN, "VAL"} }; // Unrecognized field and op
    settings.statisticConfigs = { StatisticConfig{StatisticType::UNKNOWN} }; // Unknown statistic type
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 6); // 1 regex, 1 fieldMapping, 2 filterRule, 1 exportSettings, 1 statisticConfigs
    ASSERT_THAT(errors[0], testing::HasSubstr("Invalid regex pattern"));
    ASSERT_THAT(errors[1], testing::HasSubstr("FieldMapping is missing groupIndex."));
    ASSERT_THAT(errors[2], testing::HasSubstr("FilterRule has an unrecognized field."));
    ASSERT_THAT(errors[3], testing::HasSubstr("FilterRule has an unrecognized operator."));
    ASSERT_THAT(errors[4], testing::HasSubstr("ExportSettings 'fieldsToExport' cannot be empty."));
    ASSERT_THAT(errors[5], testing::HasSubstr("StatisticConfig has an unrecognized type."));
}

TEST_F(LogAnalyzerConfigTest, ValidateValidSettings) {
    LogAnalyzerSettings settings;
    // Default constructor already sets a valid regex (std::string(DEFAULT_LOG_REGEX_PATTERN_SV))
    // and valid field mappings.

    settings.exportSettings.fieldsToExport = {
        ExportFieldMapping{LogEntryField::TIMESTAMP},
        ExportFieldMapping{LogEntryField::MESSAGE}
    };
    settings.filterRules = {
        FilterRule{LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO"}
    };
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::UNIQUE_MESSAGES},
        StatisticConfig{StatisticType::TOP_MESSAGES, {{"top_n", "5"}}},
        StatisticConfig{StatisticType::ENTRY_RATE},
        StatisticConfig{StatisticType::LOG_LEVEL_COUNT},
        StatisticConfig{StatisticType::FIELD_VALUE_COUNT, {{"target_field", "level"}}},
        StatisticConfig{StatisticType::TOP_N_FIELD_VALUES, {{"target_field", "message"}, {"top_n", "10"}}},
        StatisticConfig{StatisticType::FIELD_VALUE_COUNT, {{"target_field", "customFields"}, {"custom_field_key", "session"}}},
        StatisticConfig{StatisticType::TOP_N_FIELD_VALUES, {{"target_field", "customFields"}, {"custom_field_key", "session"}, {"top_n", "3"}}}
    };

    std::vector<std::string> errors = settings.validate();
    ASSERT_TRUE(errors.empty()) << "Validation errors: " << (errors.empty() ? "" : errors[0]);
}
