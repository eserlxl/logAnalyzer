// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "fixture.h"

TEST_F(LogAnalyzerConfigTest, ToJsonComprehensive) {
    LogAnalyzerSettings settings;
    settings.lineParsePattern = "^TEST REGEX (.*)";
    settings.fieldMappings = {
        FieldMapping{LogEntryField::TIMESTAMP, std::make_optional(1), {}},
        FieldMapping{LogEntryField::MESSAGE, std::make_optional(2), {}}
    };
    settings.customLogLevelMappings["VERB"] = LogLevel::TRACE;
    settings.setLogEntryStartPattern("^START LOG"); 
    settings.caseSensitiveParsing = true;
    settings.filterRules = {
        FilterRule{LogEntryField::LEVEL, FilterOperator::EQUALS, "DEBUG", false}
    };
    settings.exportSettings.outputPath = "custom_output.csv";
    settings.exportSettings.format = ExportFormat::CSV;
    settings.exportSettings.fieldsToExport = {
        ExportFieldMapping{LogEntryField::MESSAGE, "My Message"}
    };
    settings.exportSettings.includeHeader = true;
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::LOG_LEVEL_COUNT, {}},
    };
    settings.rootFilterExpression = FilterExpression{
        FilterLogicalOperator::OR,
        {
            FilterExpression{FilterCondition::createString(LogEntryField::LEVEL, FilterOperator::GREATER_THAN, "WARN").value()},
            FilterExpression{FilterCondition::createString(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "error").value()}
        }
    };

    std::string jsonString = settings.toJson();
    nlohmann::json j = nlohmann::json::parse(jsonString);

    ASSERT_EQ(j["lineParsePattern"], "^TEST REGEX (.*)");
    ASSERT_EQ(j["fieldMappings"].size(), 2);
    ASSERT_EQ(j["fieldMappings"][0]["field"], "TIMESTAMP");
    ASSERT_EQ(j["customLogLevelMappings"]["VERB"], "TRACE");
    ASSERT_EQ(j["logEntryStartPattern"], "^START LOG");
    ASSERT_EQ(j["caseSensitiveParsing"], true);
    ASSERT_EQ(j["filterRules"].size(), 1);
    ASSERT_EQ(j["filterRules"][0]["op"], "EQUALS");
    ASSERT_EQ(j["exportSettings"]["outputPath"], "custom_output.csv");
    ASSERT_EQ(j["exportSettings"]["format"], "CSV");
    ASSERT_EQ(j["exportSettings"]["fieldsToExport"][0]["customHeader"], "My Message");
    ASSERT_EQ(j["statisticConfigs"].size(), 1);
    ASSERT_EQ(j["statisticConfigs"][0]["type"], "COUNT_BY_LEVEL");
    
    ASSERT_TRUE(j.contains("rootFilterExpression"));
    ASSERT_EQ(j["rootFilterExpression"]["operator"], "OR");
    ASSERT_EQ(j["rootFilterExpression"]["operands"].size(), 2);

    settings.logEntryStartPattern = std::nullopt;
    jsonString = settings.toJson();
    j = nlohmann::json::parse(jsonString);
    ASSERT_TRUE(j["logEntryStartPattern"].is_null());
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

TEST_F(LogAnalyzerConfigTest, StatisticConfigJsonRoundTrip) {
    LogAnalyzerSettings settings;
    settings.statisticConfigs = {
        StatisticConfig{StatisticType::TIME_BUCKET_HISTOGRAM, {{"bucket", "30"}}},
        StatisticConfig{StatisticType::PERCENTILE_STATS, {{"field", "latency"}}},
        StatisticConfig{StatisticType::MOVING_AVERAGE_RATE, {{"bucket", "60"}}},
        StatisticConfig{StatisticType::FIND_GAPS, {{"threshold_ms", "500"}}},
    };

    const std::string jsonStr = settings.toJson();
    auto result = LogAnalyzerSettings::fromJson(jsonStr);
    ASSERT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error()[0]);

    const auto& loaded = result.value();
    ASSERT_EQ(loaded.statisticConfigs.size(), 4u);
    ASSERT_EQ(loaded.statisticConfigs[0].type, StatisticType::TIME_BUCKET_HISTOGRAM);
    ASSERT_EQ(loaded.statisticConfigs[0].params.at("bucket"), "30");
    ASSERT_EQ(loaded.statisticConfigs[1].type, StatisticType::PERCENTILE_STATS);
    ASSERT_EQ(loaded.statisticConfigs[1].params.at("field"), "latency");
    ASSERT_EQ(loaded.statisticConfigs[2].type, StatisticType::MOVING_AVERAGE_RATE);
    ASSERT_EQ(loaded.statisticConfigs[2].params.at("bucket"), "60");
    ASSERT_EQ(loaded.statisticConfigs[3].type, StatisticType::FIND_GAPS);
    ASSERT_EQ(loaded.statisticConfigs[3].params.at("threshold_ms"), "500");
}
