// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "gtest/gtest.h"
#include <gmock/gmock.h>
#include "config/Settings.h" // Use direct include
#include "core/LogTypes.h"
#include "filter/Types.h"
#include "filter/Legacy.h"
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <regex> // For std::regex

using namespace filter;

// Consolidated test fixture for all validation tests
struct ConfigValidationTest : public ::testing::Test {
    LogAnalyzerSettings settings;
    std::vector<std::string> errors;

    void SetUp() override {
        // Start with a known valid state for each test.
        settings = LogAnalyzerSettings::createDefault();
    }
};

TEST_F(ConfigValidationTest, DefaultRegexPatternCompiles) {
    ASSERT_NO_THROW({
        std::regex re(settings.lineParsePattern);
    });
}

TEST_F(ConfigValidationTest, Validate_InvalidRegexPattern) {
    settings.lineParsePattern = "[invalid regex";
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0], testing::HasSubstr("Invalid regex pattern for 'lineParsePattern'"));
}

TEST_F(ConfigValidationTest, Validate_FieldMappingMissingGroupIndex) {
    settings.fieldMappings = {
        FieldMapping(LogEntryField::TIMESTAMP) // Missing groupIndex
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0], testing::HasSubstr("FieldMapping is missing required 'groupIndex'."));
}

TEST_F(ConfigValidationTest, Validate_FilterRuleWithUnknownField) {
    settings.filterRules = {
        FilterRule{LogEntryField::UNKNOWN, FilterOperator::EQUALS, "ERROR"}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0], testing::HasSubstr("FilterRule has an unrecognized field."));
}

TEST_F(ConfigValidationTest, Validate_StatisticConfigWithUnknownType) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::UNKNOWN, {}}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0], testing::HasSubstr("StatisticConfig has an unrecognized type."));
}

TEST_F(ConfigValidationTest, Validate_MultipleErrors) {
    settings.lineParsePattern = "[invalid regex";
    settings.fieldMappings = { FieldMapping{LogEntryField::TIMESTAMP, std::nullopt} };
    settings.filterRules = { FilterRule{LogEntryField::UNKNOWN, FilterOperator::EQUALS, "VAL"} };
    settings.statisticConfigs = { StatisticConfig{StatisticType::UNKNOWN, {}} };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 4);
    EXPECT_THAT(errors, testing::Contains(testing::HasSubstr("Invalid regex pattern")));
    EXPECT_THAT(errors, testing::Contains(testing::HasSubstr("FieldMapping is missing required 'groupIndex'.")));
    EXPECT_THAT(errors, testing::Contains(testing::HasSubstr("FilterRule has an unrecognized field.")));
    EXPECT_THAT(errors, testing::Contains(testing::HasSubstr("StatisticConfig has an unrecognized type.")));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_TopMessages_MissingTopN) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_MESSAGES, {}}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0], testing::HasSubstr("requires a 'top_n' parameter"));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_TopNFieldValues_MissingTopN) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_N_FIELD_VALUES, {{std::string(config_keys::TARGET_FIELD), "level"}}}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0], testing::HasSubstr("requires a 'top_n' parameter."));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_InvalidTopN) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_MESSAGES, {{std::string(config_keys::TOP_N), "abc"}}}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0], testing::HasSubstr("must be a positive integer."));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_ZeroTopN) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_MESSAGES, {{std::string(config_keys::TOP_N), "0"}}}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0], testing::HasSubstr("must be a positive integer."));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_NegativeTopN) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_MESSAGES, {{std::string(config_keys::TOP_N), "-1"}}}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0], testing::HasSubstr("must be a positive integer."));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_FieldValueCount_MissingTargetField) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::FIELD_VALUE_COUNT, {}}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0], testing::HasSubstr("requires a 'target_field' parameter."));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_TopNFieldValues_MissingTargetField) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_N_FIELD_VALUES, {{std::string(config_keys::TOP_N), "5"}}}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0], testing::HasSubstr("requires a 'target_field' parameter."));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_InvalidTargetField) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::FIELD_VALUE_COUNT, {{std::string(config_keys::TARGET_FIELD), "non_existent_field"}}}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0], testing::HasSubstr("Invalid 'target_field' value 'non_existent_field'"));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_TopNFieldValues_CaseInsensitiveTargetField) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_N_FIELD_VALUES, {
            {std::string(config_keys::TARGET_FIELD), "LeVeL"},
            {std::string(config_keys::TOP_N), "5"}
        }}
    };
    errors = settings.validate();
    EXPECT_TRUE(errors.empty()) << "Validation should pass with case-insensitive target_field. Error: " << (errors.empty() ? "" : errors[0]);
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_FieldValueCount_MissingCustomFieldKey) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::FIELD_VALUE_COUNT, {{std::string(config_keys::TARGET_FIELD), "customFields"}}}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0], testing::HasSubstr("requires a 'custom_field_key' parameter."));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_TopNFieldValues_MissingCustomFieldKey) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_N_FIELD_VALUES, {
            {std::string(config_keys::TARGET_FIELD), "customFields"}, 
            {std::string(config_keys::TOP_N), "5"}
        }}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0], testing::HasSubstr("requires a 'custom_field_key' parameter."));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_FieldValueCount_EmptyCustomFieldKey) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::FIELD_VALUE_COUNT, {
            {std::string(config_keys::TARGET_FIELD), "customFields"},
            {std::string(config_keys::CUSTOM_FIELD_KEY), ""}
        }}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0], testing::HasSubstr("'custom_field_key' parameter cannot be empty."));
}

TEST_F(ConfigValidationTest, ValidateFilterRule_InvalidRegex) {
    settings.filterRules = {
        {LogEntryField::MESSAGE, FilterOperator::REGEX, "[invalid regex"}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0], testing::HasSubstr("Invalid regex pattern in FilterRule"));
}

TEST_F(ConfigValidationTest, ValidateFilterRule_ValueTypeMismatch_GreaterThanNotANumber) {
    settings.filterRules.push_back(
        {LogEntryField::ID, FilterOperator::GREATER_THAN, "abc"}
    );
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0], testing::HasSubstr("requires a numeric value, but got 'abc'"));
}

TEST_F(ConfigValidationTest, ValidateValidSettings) {
    // Settings are already created with createDefault() in SetUp()
    settings.exportSettings.fieldsToExport = {
        ExportFieldMapping{LogEntryField::TIMESTAMP},
        ExportFieldMapping{LogEntryField::MESSAGE}
    };
    settings.filterRules = {
        FilterRule{LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO"}
    };
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::UNIQUE_MESSAGES, {}},
        StatisticConfig{StatisticType::TOP_MESSAGES, {{std::string(config_keys::TOP_N), "5"}}},
        StatisticConfig{StatisticType::ENTRY_RATE, {}},
        StatisticConfig{StatisticType::LOG_LEVEL_COUNT, {}},
        StatisticConfig{StatisticType::FIELD_VALUE_COUNT, {{std::string(config_keys::TARGET_FIELD), "level"}}},
        StatisticConfig{StatisticType::TOP_N_FIELD_VALUES, {{std::string(config_keys::TARGET_FIELD), "message"}, {std::string(config_keys::TOP_N), "10"}}},
        StatisticConfig{StatisticType::FIELD_VALUE_COUNT, {{std::string(config_keys::TARGET_FIELD), "customFields"}, {std::string(config_keys::CUSTOM_FIELD_KEY), "session"}}},
        StatisticConfig{StatisticType::TOP_N_FIELD_VALUES, {{std::string(config_keys::TARGET_FIELD), "customFields"}, {std::string(config_keys::CUSTOM_FIELD_KEY), "session"}, {std::string(config_keys::TOP_N), "3"}}}
    };

    errors = settings.validate();
    ASSERT_TRUE(errors.empty()) << "Validation errors: " << (errors.empty() ? "" : errors[0]);
}
