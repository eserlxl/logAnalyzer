// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "tests/include/CLI.h"

TEST_F(CLIConfigTest, DefaultValues) {
    auto result = parse({"log_analyzer", "dummy_log_file.log"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();

    // Verify CLIOptions default values
    ASSERT_TRUE(options.filterLevels.empty());
    ASSERT_FALSE(options.minLogLevel.has_value());
    ASSERT_TRUE(options.filterKeywords.empty());
    ASSERT_TRUE(options.excludeKeywords.empty());
    ASSERT_FALSE(options.keywordCaseSensitive);
    ASSERT_TRUE(options.regexPatterns.empty());
    ASSERT_TRUE(options.excludeRegexPatterns.empty());
    ASSERT_FALSE(options.filterLogic.has_value());
    ASSERT_FALSE(options.startTime.has_value());
    ASSERT_FALSE(options.endTime.has_value());
    ASSERT_FALSE(options.duration.has_value());
    ASSERT_FALSE(options.sortBy.has_value());
    ASSERT_FALSE(options.sortOrder.has_value());
    ASSERT_EQ(options.outputFormat, "text");
    ASSERT_TRUE(options.outputPath.empty());
    ASSERT_EQ(options.textOutputFormat, "{timestamp} {level}: {message}");
    ASSERT_FALSE(options.includeSummary);
    ASSERT_FALSE(options.prettyPrint);
    ASSERT_EQ(options.colorOption, CLIConfig::ColorOption::AUTO);
    ASSERT_EQ(options.csvSeparator, ',');
    ASSERT_TRUE(options.csvFields.empty());
    ASSERT_EQ(options.topMessagesCount, 10);
    ASSERT_FALSE(options.streamMode);
    ASSERT_EQ(options.parserErrorAction, CLIConfig::ParserErrorAction::Warn);
    ASSERT_FALSE(options.tailMode);
    ASSERT_EQ(options.tailInterval, std::chrono::milliseconds(1000));
    ASSERT_TRUE(options.complexFilterExpression.empty());
    ASSERT_TRUE(options.jsonFields.empty());
    ASSERT_TRUE(options.enabledStatistics.empty());
    ASSERT_FALSE(options.statsWindow.has_value());
    ASSERT_FALSE(options.findGapsDuration.has_value());
    ASSERT_FALSE(options.readFromStdin); // Should be false if file path provided

    // Verify LogAnalyzerSettings default values (based on CLIConfig defaults)
    ASSERT_EQ(settings.lineParsePattern, DEFAULT_LOG_REGEX_PATTERN_INTERNAL);
    ASSERT_TRUE(settings.filterRules.empty()); // No filters by default
    ASSERT_FALSE(settings.rootFilterExpression.has_value());
    ASSERT_FALSE(settings.exportSettings.outputPath.has_value());
    ASSERT_FALSE(settings.exportSettings.format.has_value());
    ASSERT_FALSE(settings.exportSettings.textOutputFormat.has_value());
    ASSERT_FALSE(settings.exportSettings.includeSummary.has_value());
    ASSERT_FALSE(settings.exportSettings.prettyPrint.has_value());
    ASSERT_FALSE(settings.exportSettings.outputNoColor.has_value()); // AUTO implies not explicitly no-color
    ASSERT_FALSE(settings.exportSettings.csvSeparator.has_value());
    ASSERT_TRUE(settings.exportSettings.csvFields.empty());
    ASSERT_TRUE(settings.exportSettings.jsonFields.empty());
    ASSERT_FALSE(settings.exportSettings.topMessagesCount.has_value()); // Default for ExportSettings
    ASSERT_FALSE(settings.exportSettings.streamMode.has_value());
    ASSERT_FALSE(settings.exportSettings.tailMode.has_value());
    ASSERT_FALSE(settings.exportSettings.tailInterval.has_value());
    ASSERT_FALSE(settings.parserErrorAction.has_value());
    ASSERT_TRUE(settings.customLogLevelMappings.empty());
    ASSERT_TRUE(settings.statisticConfigs.empty());
}

TEST_F(CLIConfigTest, CustomParsePattern) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--pattern", R"(^\{(\d+)\](.*)$)"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.lineParsePattern, R"(^\{(\d+)\](.*)$)");
    ASSERT_EQ(settings.lineParsePattern, R"(^\{(\d+)\](.*)$)");
}
