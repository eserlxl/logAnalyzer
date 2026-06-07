// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "gtest/gtest.h"
#include <gmock/gmock.h>
#include "config/settings.h" // Use direct include
#include "core/log/types.h"
#include "filter/types.h"
#include "filter/legacy.h"
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
    // Missing top_n is now valid for TOP_MESSAGES — factory defaults to 10.
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_MESSAGES, {}}
    };
    errors = settings.validate();
    ASSERT_TRUE(errors.empty()) << "TOP_MESSAGES without top_n should use factory default";
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

TEST_F(ConfigValidationTest, ValidateStatisticConfig_TopNRejectsTrailingCharacters) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_MESSAGES, {{std::string(config_keys::TOP_N), "5abc"}}}
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

TEST_F(ConfigValidationTest, ValidateStatisticConfig_DeprecatedPidAliasRejected) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::FIELD_VALUE_COUNT, {{std::string(config_keys::TARGET_FIELD), "pid"}}}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0], testing::HasSubstr("Invalid 'target_field' value 'pid'"));
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

TEST_F(ConfigValidationTest, ValidateStatisticConfig_TargetFieldWithOuterWhitespace) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::FIELD_VALUE_COUNT, {{std::string(config_keys::TARGET_FIELD), "  source_file  "}}}
    };
    errors = settings.validate();
    EXPECT_TRUE(errors.empty()) << "Validation should trim target_field whitespace. Error: " << (errors.empty() ? "" : errors[0]);
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

TEST_F(ConfigValidationTest, ValidateFilterRule_ValueTypeMismatch_TrailingCharacters) {
    settings.filterRules.push_back(
        {LogEntryField::ID, FilterOperator::GREATER_THAN, "12abc"}
    );
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_THAT(errors[0], testing::HasSubstr("requires a numeric value, but got '12abc'"));
}

TEST_F(ConfigValidationTest, ValidateFilterRule_ThreadIdWithStringEqualityAccepted) {
    settings.filterRules.push_back(
        {LogEntryField::THREAD_ID, FilterOperator::EQUALS, "main-thread"}
    );
    errors = settings.validate();
    EXPECT_TRUE(errors.empty()) << "THREAD_ID with string value should pass validation";
}

TEST_F(ConfigValidationTest, ValidateFilterRule_ThreadIdGreaterThanStringAccepted) {
    settings.filterRules.push_back(
        {LogEntryField::THREAD_ID, FilterOperator::GREATER_THAN, "non-numeric-tid"}
    );
    errors = settings.validate();
    EXPECT_TRUE(errors.empty()) << "THREAD_ID is a string field; non-numeric values must not be rejected";
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

TEST_F(ConfigValidationTest, ValidateStatisticConfig_PercentileStats_MissingField) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::PERCENTILE_STATS, {}}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_THAT(errors[0], testing::HasSubstr("requires a 'field' parameter"));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_PercentileStats_EmptyField) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::PERCENTILE_STATS, {{"field", ""}}}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_THAT(errors[0], testing::HasSubstr("requires a 'field' parameter"));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_PercentileStats_WhitespaceFieldRejected) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::PERCENTILE_STATS, {{"field", "  "}}}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_THAT(errors[0], testing::HasSubstr("requires a 'field' parameter"));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_PercentileStats_Valid) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::PERCENTILE_STATS, {{"field", "latency_ms"}}}
    };
    errors = settings.validate();
    ASSERT_TRUE(errors.empty());
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_PercentileStats_CustomPercentilesValid) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::PERCENTILE_STATS,
                        {{"field", "latency_ms"}, {"percentiles", "50,90,99.9"}}}
    };
    errors = settings.validate();
    ASSERT_TRUE(errors.empty());
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_PercentileStats_InvalidPercentileRejected) {
    for (const char* bad : {"150", "0", "-5", "abc", "50,,90", "50;200", ""}) {
        settings.statisticConfigs = {
            StatisticConfig{StatisticType::PERCENTILE_STATS,
                            {{"field", "latency_ms"}, {"percentiles", bad}}}
        };
        errors = settings.validate();
        EXPECT_FALSE(errors.empty()) << "percentiles='" << bad << "' should be rejected";
    }
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_PercentileStats_LeadingTrailingWhitespaceAccepted) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::PERCENTILE_STATS, {{"field", " latency_ms "}}}
    };
    errors = settings.validate();
    ASSERT_TRUE(errors.empty()) << "Field with leading/trailing whitespace should pass (trim semantics)";
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_PercentileStats_StandardFieldAccepted) {
    for (const auto& fieldName : {"lineNumber", "id", "line_number"}) {
        settings.statisticConfigs = {
            StatisticConfig{StatisticType::PERCENTILE_STATS, {{"field", fieldName}}}
        };
        errors = settings.validate();
        EXPECT_TRUE(errors.empty()) << "Standard field '" << fieldName << "' should pass PERCENTILE_STATS validation";
    }
}

TEST_F(ConfigValidationTest, ValidateTopMessagesNoTopNAccepted) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_MESSAGES, {}}
    };
    errors = settings.validate();
    ASSERT_TRUE(errors.empty()) << "TOP_MESSAGES without top_n should use factory default of 10";
}

TEST_F(ConfigValidationTest, ValidateTopMessagesInvalidTopNStillRejected) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_MESSAGES, {{"top_n", "abc"}}}
    };
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    EXPECT_THAT(errors[0], testing::HasSubstr("top_n"));
}

TEST_F(ConfigValidationTest, ValidateTopNFieldValuesWithoutTopNRejected) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_N_FIELD_VALUES, {{std::string(config_keys::TARGET_FIELD), "level"}}}
    };
    errors = settings.validate();
    ASSERT_FALSE(errors.empty());
    EXPECT_THAT(errors[0], testing::HasSubstr("top_n"));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_FieldValueCountIdAccepted) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::FIELD_VALUE_COUNT, {{std::string(config_keys::TARGET_FIELD), "id"}}}
    };
    errors = settings.validate();
    EXPECT_TRUE(errors.empty()) << "target_field=id should be valid. Error: " << (errors.empty() ? "" : errors[0]);
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_FieldValueCountIdCaseInsensitive) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::FIELD_VALUE_COUNT, {{std::string(config_keys::TARGET_FIELD), "ID"}}}
    };
    errors = settings.validate();
    EXPECT_TRUE(errors.empty()) << "target_field=ID (uppercase) should be valid. Error: " << (errors.empty() ? "" : errors[0]);

    settings.statisticConfigs = {
        StatisticConfig{StatisticType::FIELD_VALUE_COUNT, {{std::string(config_keys::TARGET_FIELD), "Id"}}}
    };
    errors = settings.validate();
    EXPECT_TRUE(errors.empty()) << "target_field=Id (mixed case) should be valid. Error: " << (errors.empty() ? "" : errors[0]);
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_TopNFieldValuesIdAccepted) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_N_FIELD_VALUES, {
            {std::string(config_keys::TARGET_FIELD), "id"},
            {std::string(config_keys::TOP_N), "5"}
        }}
    };
    errors = settings.validate();
    EXPECT_TRUE(errors.empty()) << "TOP_N_FIELD_VALUES target_field=id should be valid. Error: " << (errors.empty() ? "" : errors[0]);
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_TopNFieldValues_NonIntegerTopNRejected) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_N_FIELD_VALUES, {
            {std::string(config_keys::TARGET_FIELD), "level"},
            {std::string(config_keys::TOP_N), "abc"}
        }}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_THAT(errors[0], testing::HasSubstr("must be a positive integer."));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_TopNFieldValues_ZeroTopNRejected) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_N_FIELD_VALUES, {
            {std::string(config_keys::TARGET_FIELD), "level"},
            {std::string(config_keys::TOP_N), "0"}
        }}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_THAT(errors[0], testing::HasSubstr("must be a positive integer."));
}

TEST_F(ConfigValidationTest, ValidateStatisticConfig_TopNFieldValues_NegativeTopNRejected) {
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TOP_N_FIELD_VALUES, {
            {std::string(config_keys::TARGET_FIELD), "level"},
            {std::string(config_keys::TOP_N), "-1"}
        }}
    };
    errors = settings.validate();
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_THAT(errors[0], testing::HasSubstr("must be a positive integer."));
}
