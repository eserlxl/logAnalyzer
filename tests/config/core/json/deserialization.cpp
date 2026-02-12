// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "fixture.h"

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
            {"field": "LEVEL", "op": "EQUALS", "value": "ERROR", "value_type": "STRING", "caseSensitive": false}
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

TEST_F(LogAnalyzerConfigTest, FromJsonRootFilterExpression) {
    std::string jsonContent = R"({
        "lineParsePattern": ".*",
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]},
        "rootFilterExpression": {
            "operator": "AND",
            "operands": [
                {"condition": {"field": "LEVEL", "op": "EQUALS", "value": "ERROR", "value_type": 1}},
                {
                    "operator": "OR",
                    "operands": [
                        {"condition": {"field": "MESSAGE", "op": "CONTAINS", "value": "fatal", "value_type": 1}},
                        {"condition": {"field": "MESSAGE", "op": "CONTAINS", "value": "critical", "value_type": 1}}
                    ]
                }
            ]
        }
    })";
    auto result = LogAnalyzerSettings::fromJson(jsonContent);
    ASSERT_TRUE(result.has_value()) << "Errors: " << (result.has_value() ? "" : result.error()[0]);
    LogAnalyzerSettings settings = result.value();
    ASSERT_TRUE(settings.rootFilterExpression.has_value());
    ASSERT_TRUE(settings.rootFilterExpression->isLogical());
    ASSERT_EQ(settings.rootFilterExpression->getLogicalOperator(), FilterLogicalOperator::AND);
    ASSERT_EQ(settings.rootFilterExpression->getExpressions().size(), 2);

    ASSERT_TRUE(settings.rootFilterExpression->getExpressions()[0].isCondition());
    ASSERT_EQ(settings.rootFilterExpression->getExpressions()[0].getCondition()->field, LogEntryField::LEVEL);
    ASSERT_EQ(settings.rootFilterExpression->getExpressions()[0].getCondition()->op, FilterOperator::EQUALS);
    ASSERT_EQ(std::get<std::string>(settings.rootFilterExpression->getExpressions()[0].getCondition()->value), "ERROR");

    ASSERT_TRUE(settings.rootFilterExpression->getExpressions()[1].isLogical());
    ASSERT_EQ(settings.rootFilterExpression->getExpressions()[1].getLogicalOperator(), FilterLogicalOperator::OR);
    ASSERT_EQ(settings.rootFilterExpression->getExpressions()[1].getExpressions().size(), 2);
    ASSERT_TRUE(settings.rootFilterExpression->getExpressions()[1].getExpressions()[0].isCondition());
    ASSERT_EQ(settings.rootFilterExpression->getExpressions()[1].getExpressions()[0].getCondition()->field, LogEntryField::MESSAGE);
    ASSERT_EQ(settings.rootFilterExpression->getExpressions()[1].getExpressions()[0].getCondition()->op, FilterOperator::CONTAINS);
    ASSERT_EQ(std::get<std::string>(settings.rootFilterExpression->getExpressions()[1].getExpressions()[0].getCondition()->value), "fatal");
    ASSERT_TRUE(settings.rootFilterExpression->getExpressions()[1].getExpressions()[1].isCondition()); 
    ASSERT_EQ(settings.rootFilterExpression->getExpressions()[1].getExpressions()[1].getCondition()->field, LogEntryField::MESSAGE);
    ASSERT_EQ(settings.rootFilterExpression->getExpressions()[1].getExpressions()[1].getCondition()->op, FilterOperator::CONTAINS);
    ASSERT_EQ(std::get<std::string>(settings.rootFilterExpression->getExpressions()[1].getExpressions()[1].getCondition()->value), "critical");
}

TEST_F(LogAnalyzerConfigTest, FromJsonOptionalFields) {
    std::string jsonContent1 = R"({
        "lineParsePattern": ".*",
        "logEntryStartPattern": "^START",
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result1 = LogAnalyzerSettings::fromJson(jsonContent1);
    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result1.value().logEntryStartPattern.has_value());
    ASSERT_EQ(result1.value().logEntryStartPattern.value(), "^START");

    std::string jsonContent2 = R"({
        "lineParsePattern": ".*",
        "logEntryStartPattern": null,
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result2 = LogAnalyzerSettings::fromJson(jsonContent2);
    ASSERT_TRUE(result2.has_value());
    ASSERT_FALSE(result2.value().logEntryStartPattern.has_value());

    std::string jsonContent3 = R"({
        "lineParsePattern": ".*",
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result3 = LogAnalyzerSettings::fromJson(jsonContent3);
    ASSERT_TRUE(result3.has_value());
    ASSERT_FALSE(result3.value().logEntryStartPattern.has_value());

    std::string jsonContent4 = R"({
        "lineParsePattern": ".*",
        "rootFilterExpression": null,
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result4 = LogAnalyzerSettings::fromJson(jsonContent4);
    ASSERT_TRUE(result4.has_value());
    ASSERT_FALSE(result4.value().rootFilterExpression.has_value());

    std::string jsonContent5 = R"({
        "lineParsePattern": ".*",
        "statisticConfigs": null,
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result5 = LogAnalyzerSettings::fromJson(jsonContent5);
    ASSERT_TRUE(result5.has_value());
    ASSERT_TRUE(result5.value().statisticConfigs.empty());
}

TEST_F(LogAnalyzerConfigTest, FromJsonMalformedInternalStructures) {
    std::string jsonContent1 = R"({
        "lineParsePattern": ".*",
        "fieldMappings": [
            {"groupIndex": 1}
        ],
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result1 = LogAnalyzerSettings::fromJson(jsonContent1);
    ASSERT_FALSE(result1.has_value());
    ASSERT_THAT(result1.error()[0], testing::HasSubstr("Error parsing 'fieldMappings':"));

    std::string jsonContent2 = R"({
        "lineParsePattern": ".*",
        "filterRules": [
            {"field": "LEVEL", "value": "ERROR"}
        ],
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result2 = LogAnalyzerSettings::fromJson(jsonContent2);
    ASSERT_FALSE(result2.has_value());
    ASSERT_THAT(result2.error()[0], testing::HasSubstr("Error parsing 'filterRules':"));

    std::string jsonContent3 = R"({
        "lineParsePattern": ".*",
        "exportSettings": {
            "outputPath": "test.log",
            "fieldsToExport": "MESSAGE"
        }
    })";
    auto result3 = LogAnalyzerSettings::fromJson(jsonContent3);
    ASSERT_FALSE(result3.has_value());
    ASSERT_THAT(result3.error()[0], testing::HasSubstr("Error parsing 'exportSettings':"));

    std::string jsonContent4 = R"({
        "lineParsePattern": ".*",
        "statisticConfigs": [
            {}
        ],
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result4 = LogAnalyzerSettings::fromJson(jsonContent4);
    ASSERT_FALSE(result4.has_value());
    ASSERT_THAT(result4.error()[0], testing::AnyOf(
        testing::HasSubstr("Error parsing 'statisticConfigs':"),
        testing::HasSubstr("StatisticConfig has an unrecognized type.")
    ));

    std::string jsonContent4b = R"({
        "lineParsePattern": ".*",
        "statisticConfigs": [
            {"type": "COUNT_BY_LEVEL", "params": {"top_n": 5}}
        ],
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result4b = LogAnalyzerSettings::fromJson(jsonContent4b);
    ASSERT_FALSE(result4b.has_value());
    ASSERT_THAT(result4b.error(), testing::Contains(testing::HasSubstr("StatisticConfig has an unrecognized type.")));

    std::string jsonContent4c = R"({
        "lineParsePattern": ".*",
        "statisticConfigs": [
            {"type": "COUNT_BY_LEVEL", "params": [1, 2, 3]}
        ],
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result4c = LogAnalyzerSettings::fromJson(jsonContent4c);
    ASSERT_FALSE(result4c.has_value());
    ASSERT_THAT(result4c.error(), testing::Contains(testing::HasSubstr("StatisticConfig has an unrecognized type.")));

    std::string jsonContent4d = R"({
        "lineParsePattern": ".*",
        "statisticConfigs": [
            {"type": "COUNT_BY_LEVEL", "params": {"top_n": {"value": "5"}}}
        ],
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result4d = LogAnalyzerSettings::fromJson(jsonContent4d);
    ASSERT_FALSE(result4d.has_value());
    ASSERT_THAT(result4d.error(), testing::Contains(testing::HasSubstr("StatisticConfig has an unrecognized type.")));

    std::string jsonContent5 = R"({
        "lineParsePattern": ".*",
        "rootFilterExpression": {
            "operator": "INVALID_OP"
        },
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result5 = LogAnalyzerSettings::fromJson(jsonContent5);
    ASSERT_FALSE(result5.has_value());
    ASSERT_THAT(result5.error()[0], testing::HasSubstr("Error parsing 'rootFilterExpression':"));
}

TEST_F(LogAnalyzerConfigTest, FromJsonIncorrectFieldType) {
    std::string jsonContent1 = R"({
        "lineParsePattern": 123,
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result1 = LogAnalyzerSettings::fromJson(jsonContent1);
    ASSERT_FALSE(result1.has_value());
    ASSERT_THAT(result1.error()[0], testing::HasSubstr("Invalid type for 'lineParsePattern'. Expected string."));

    std::string jsonContent2 = R"({
        "lineParsePattern": ".*",
        "caseSensitiveParsing": "true",
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result2 = LogAnalyzerSettings::fromJson(jsonContent2);
    ASSERT_FALSE(result2.has_value());
    ASSERT_THAT(result2.error()[0], testing::HasSubstr("Invalid type for 'caseSensitiveParsing'. Expected boolean."));

    std::string jsonContent3 = R"({
        "lineParsePattern": ".*",
        "fieldMappings": {"field": "TIMESTAMP", "groupIndex": 1},
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result3 = LogAnalyzerSettings::fromJson(jsonContent3);
    ASSERT_FALSE(result3.has_value());
    ASSERT_THAT(result3.error()[0], testing::HasSubstr("Invalid type for 'fieldMappings'. Expected array."));

    std::string jsonContent4 = R"({
        "lineParsePattern": ".*",
        "customLogLevelMappings": [{"DEBUG": "DBG"}],
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result4 = LogAnalyzerSettings::fromJson(jsonContent4);
    ASSERT_FALSE(result4.has_value());
    ASSERT_THAT(result4.error()[0], testing::HasSubstr("Invalid type for 'customLogLevelMappings'. Expected object."));

    std::string jsonContent5 = R"({
        "lineParsePattern": ".*",
        "filterRules": "some_rule",
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result5 = LogAnalyzerSettings::fromJson(jsonContent5);
    ASSERT_FALSE(result5.has_value());
    ASSERT_THAT(result5.error()[0], testing::HasSubstr("Invalid type for 'filterRules'. Expected array."));

    std::string jsonContent6 = R"({
        "lineParsePattern": ".*",
        "exportSettings": [{"outputPath": "output.log", "fieldsToExport": [{"field": "MESSAGE"}]}]
    })";
    auto result6 = LogAnalyzerSettings::fromJson(jsonContent6);
    ASSERT_FALSE(result6.has_value());
    ASSERT_THAT(result6.error()[0], testing::HasSubstr("Invalid type for 'exportSettings'. Expected object."));

    std::string jsonContent7 = R"({
        "lineParsePattern": ".*",
        "statisticConfigs": {"type": "COUNT_BY_LEVEL"},
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result7 = LogAnalyzerSettings::fromJson(jsonContent7);
    ASSERT_FALSE(result7.has_value());
    ASSERT_THAT(result7.error()[0], testing::HasSubstr("Invalid type for 'statisticConfigs'. Expected array."));

    std::string jsonContent8 = R"({
        "lineParsePattern": ".*",
        "rootFilterExpression": "invalid_expression",
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result8 = LogAnalyzerSettings::fromJson(jsonContent8);
    ASSERT_FALSE(result8.has_value());
    ASSERT_THAT(result8.error()[0], testing::HasSubstr("Invalid type for 'rootFilterExpression'. Expected object."));
}

TEST_F(LogAnalyzerConfigTest, FromJsonMalformedJson) {
    std::string malformedJson = R"({
        "lineParsePattern": ".*",
        "fieldMappings": [
            {"field": "TIMESTAMP", "groupIndex": 1}
        ],
        "exportSettings": {
            "outputPath": "test.log",
            "fieldsToExport": [ {"field": "MESSAGE"} ]
        }
    ,})"; 
    auto result = LogAnalyzerSettings::fromJson(malformedJson);
    ASSERT_FALSE(result.has_value());
    ASSERT_THAT(result.error()[0], testing::HasSubstr("JSON parsing error:"));
}

TEST_F(LogAnalyzerConfigTest, FromJsonRejectsNonObjectRoot) {
    std::string jsonContent = R"([1, 2, 3])";
    auto result = LogAnalyzerSettings::fromJson(jsonContent);
    ASSERT_FALSE(result.has_value());
    ASSERT_THAT(result.error()[0], testing::HasSubstr("Invalid top-level JSON type. Expected object."));
}

TEST_F(LogAnalyzerConfigTest, FromJsonInvalidCustomLogLevelMapping) {
    std::string jsonContent = R"({
        "lineParsePattern": ".*",
        "customLogLevelMappings": {"BAD": "INVALID_LEVEL_STRING"},
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result = LogAnalyzerSettings::fromJson(jsonContent);
    ASSERT_FALSE(result.has_value());
    ASSERT_THAT(result.error()[0], testing::HasSubstr("Invalid custom log level string 'INVALID_LEVEL_STRING' for key 'BAD'."));
}

TEST_F(LogAnalyzerConfigTest, FromJsonParserErrorActionValueIsTrimmed) {
    std::string jsonContent = R"({
        "lineParsePattern": ".*",
        "parserErrorAction": "  warn  ",
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result = LogAnalyzerSettings::fromJson(jsonContent);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().parserErrorAction.value(), ParserErrorAction::Warn);
}

TEST_F(LogAnalyzerConfigTest, FromJsonMaxMultilineBufferSizeAcceptsInteger) {
    std::string jsonContent = R"({
        "lineParsePattern": ".*",
        "maxMultilineBufferSize": 4096,
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result = LogAnalyzerSettings::fromJson(jsonContent);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().maxMultilineBufferSize.value(), 4096u);
}

TEST_F(LogAnalyzerConfigTest, FromJsonMaxMultilineBufferSizeRejectsNegativeInteger) {
    std::string jsonContent = R"({
        "lineParsePattern": ".*",
        "maxMultilineBufferSize": -1,
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result = LogAnalyzerSettings::fromJson(jsonContent);
    ASSERT_FALSE(result.has_value());
    ASSERT_THAT(result.error(), testing::Contains(testing::HasSubstr("Invalid value for 'maxMultilineBufferSize'. Expected non-negative integer or size string.")));
}

TEST_F(LogAnalyzerConfigTest, FromJsonMaxMultilineBufferSizeAcceptsNull) {
    std::string jsonContent = R"({
        "lineParsePattern": ".*",
        "maxMultilineBufferSize": null,
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result = LogAnalyzerSettings::fromJson(jsonContent);
    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result.value().maxMultilineBufferSize.has_value());
}

TEST_F(LogAnalyzerConfigTest, FromJsonRejectsEmptyCustomLogLevelMappingKey) {
    std::string jsonContent = R"({
        "lineParsePattern": ".*",
        "customLogLevelMappings": {"": "DEBUG"},
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result = LogAnalyzerSettings::fromJson(jsonContent);
    ASSERT_FALSE(result.has_value());
    ASSERT_THAT(result.error(), testing::Contains(testing::HasSubstr("Invalid customLogLevelMappings key: key cannot be empty.")));
}

TEST_F(LogAnalyzerConfigTest, FromJsonCustomLogLevelMappingValueIsTrimmed) {
    std::string jsonContent = R"({
        "lineParsePattern": ".*",
        "customLogLevelMappings": {"DBG": "  DEBUG  "},
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result = LogAnalyzerSettings::fromJson(jsonContent);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().customLogLevelMappings.at("DBG"), LogLevel::DEBUG);
}

TEST_F(LogAnalyzerConfigTest, FromJsonInvalidVersionType) {
    std::string jsonContent = R"({
        "version": 2,
        "lineParsePattern": ".*",
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result = LogAnalyzerSettings::fromJson(jsonContent);
    ASSERT_FALSE(result.has_value());
    ASSERT_THAT(result.error()[0], testing::HasSubstr("Invalid type for 'version'. Expected string."));
}

TEST_F(LogAnalyzerConfigTest, FromJsonRejectsEmptyVersionString) {
    std::string jsonContent = R"({
        "version": "   ",
        "lineParsePattern": ".*",
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    auto result = LogAnalyzerSettings::fromJson(jsonContent);
    ASSERT_FALSE(result.has_value());
    ASSERT_THAT(result.error(), testing::Contains(testing::HasSubstr("Invalid value for 'version'. Expected non-empty string.")));
}

TEST_F(LogAnalyzerConfigTest, FromFileErrorHandling) {
    std::string nonExistentFilePath = "non_existent_config.json";
    auto result1 = LogAnalyzerSettings::fromFile(nonExistentFilePath);
    ASSERT_FALSE(result1.has_value());
    ASSERT_THAT(result1.error()[0], testing::HasSubstr("Configuration file does not exist:"));

    std::string malformedJsonContent = R"({
        "lineParsePattern": ".*",
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    ,})"; 
    std::string malformedFilePath = "temp_config_malformed.json";
    std::ofstream ofs(malformedFilePath);
    ofs << malformedJsonContent;
    ofs.close();

    auto result2 = LogAnalyzerSettings::fromFile(malformedFilePath);
    ASSERT_FALSE(result2.has_value());
    ASSERT_THAT(result2.error()[0], testing::HasSubstr("JSON parsing error"));
    std::filesystem::remove(malformedFilePath);
}

TEST_F(LogAnalyzerConfigTest, FromFileValid) {
    std::string jsonContent = R"({
        "lineParsePattern": "^(\\d+) (.*)$",
        "fieldMappings": [
            {"field": "TIMESTAMP", "groupIndex": 1}
        ],
        "exportSettings": {"fieldsToExport": [{"field": "MESSAGE"}]}
    })";
    
    std::string tempFilePath = "temp_config_valid.json";
    std::ofstream ofs(tempFilePath);
    ofs << jsonContent;
    ofs.close();

    auto result = LogAnalyzerSettings::fromFile(tempFilePath);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().lineParsePattern, R"(^(\d+) (.*)$)");
    std::filesystem::remove(tempFilePath);
}
