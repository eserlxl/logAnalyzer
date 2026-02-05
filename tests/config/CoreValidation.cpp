#include "gtest/gtest.h"
#include <gmock/gmock.h>
#include "config/Core.h"
#include "core/LogTypes.h"
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
        FieldMapping(LogEntryField::TIMESTAMP) // Missing groupIndex
    };
    settings.exportSettings.fieldsToExport = { ExportFieldMapping{LogEntryField::MESSAGE} }; // Ensure exportSettings is valid for standalone validation
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    ASSERT_THAT(errors[0], testing::HasSubstr("groupIndex"));
    settings.fieldMappings = { FieldMapping{LogEntryField::TIMESTAMP, 1} }; // Valid fieldMappings for next test


    // 3. FilterRule with UNKNOWN field (operator must be valid now)
    settings.filterRules = {
        FilterRule{LogEntryField::UNKNOWN, FilterOperator::EQUALS, "ERROR"}
    };
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    ASSERT_THAT(errors[0], testing::HasSubstr("FilterRule has an unrecognized field."));
    settings.filterRules = {}; // Clear for next test

    // 4. ExportSettings with empty fieldsToExport (this is a valid state)
    settings.exportSettings.fieldsToExport = {};
    errors = settings.validate();
    ASSERT_TRUE(errors.empty()); // Should be valid now
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
    // exportSettings is now valid when empty
    settings.filterRules = { FilterRule{LogEntryField::UNKNOWN, FilterOperator::EQUALS, "VAL"} }; // Unrecognized field. Operator will be valid.
    settings.statisticConfigs = { StatisticConfig{StatisticType::UNKNOWN} }; // Unknown statistic type
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 4); // 1 regex, 1 fieldMapping, 1 filterRule, 1 statisticConfigs
    ASSERT_THAT(errors[0], testing::HasSubstr("Invalid regex pattern"));
    ASSERT_THAT(errors[1], testing::HasSubstr("FieldMapping is missing required 'groupIndex'."));
    ASSERT_THAT(errors[2], testing::HasSubstr("FilterRule has an unrecognized field."));
    ASSERT_THAT(errors[3], testing::HasSubstr("StatisticConfig has an unrecognized type."));
}


struct ConfigValidationTest : public ::testing::Test {
    LogAnalyzerSettings settings;
    std::vector<std::string> errors;
};

TEST_F(ConfigValidationTest, ValidateStatisticConfig_TopMessages_MissingTopN) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_MESSAGES} // Missing "top_n"
    };
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    EXPECT_THAT(errors[0], testing::HasSubstr("Statistic 'TOP_MESSAGES' requires a 'top_n' parameter."));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_TopNFieldValues_MissingTopN) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_N_FIELD_VALUES, {{"target_field", "level"}}} // Missing "top_n"
    };
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    EXPECT_THAT(errors[0], testing::HasSubstr("Statistic 'TOP_N_FIELD_VALUES' requires a 'top_n' parameter."));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_InvalidTopN) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_MESSAGES, {{"top_n", "abc"}}} // invalid integer
    };
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    EXPECT_THAT(errors[0], testing::HasSubstr("Parameter 'top_n' for statistic 'TOP_MESSAGES' must be a positive integer."));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_ZeroTopN) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_MESSAGES, {{"top_n", "0"}}} // Zero
    };
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    EXPECT_THAT(errors[0], testing::HasSubstr("Parameter 'top_n' for statistic 'TOP_MESSAGES' must be a positive integer."));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_NegativeTopN) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_MESSAGES, {{"top_n", "-1"}}} // Negative
    };
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    EXPECT_THAT(errors[0], testing::HasSubstr("Parameter 'top_n' for statistic 'TOP_MESSAGES' must be a positive integer."));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_FieldValueCount_MissingTargetField) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::FIELD_VALUE_COUNT} // Missing "target_field"
    };
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    EXPECT_THAT(errors[0], testing::HasSubstr("Statistic 'FIELD_VALUE_COUNT' requires a 'target_field' parameter."));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_TopNFieldValues_MissingTargetField) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_N_FIELD_VALUES, {{"top_n", "5"}}} // Missing "target_field"
    };
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    EXPECT_THAT(errors[0], testing::HasSubstr("Statistic 'TOP_N_FIELD_VALUES' requires a 'target_field' parameter."));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_FieldValueCount_MissingCustomFieldKey) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::FIELD_VALUE_COUNT, {{"target_field", "customFields"}}} // Missing "custom_field_key"
    };
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    EXPECT_THAT(errors[0], testing::HasSubstr("Statistic 'FIELD_VALUE_COUNT' with target_field 'customFields' requires a 'custom_field_key' parameter."));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_TopNFieldValues_MissingCustomFieldKey) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_N_FIELD_VALUES, {{"target_field", "customFields"}, {"top_n", "5"}}} // Missing "custom_field_key"
    };
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    EXPECT_THAT(errors[0], testing::HasSubstr("Statistic 'TOP_N_FIELD_VALUES' with target_field 'customFields' requires a 'custom_field_key' parameter."));
}

TEST_F(ConfigValidationTest, ValidateFilterRule_InvalidRegex) {
    settings.filterRules = {
        {LogEntryField::MESSAGE, FilterOperator::REGEX_MATCH, "[invalid regex"}
    };
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    EXPECT_THAT(errors[0], testing::HasSubstr("Invalid regex pattern in FilterRule"));
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
