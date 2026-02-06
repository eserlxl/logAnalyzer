// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "tests/include/CLI.h"
#include "filter/Types.h" // For SortBy, SortOrder

using namespace filter;

TEST_F(CLIConfigTest, OutputJsonOption) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--format", "json"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();

    ASSERT_EQ(options.outputFormat, "json");
    ASSERT_EQ(settings.exportSettings.format, ExportFormat::JSON);
}

TEST_F(CLIConfigTest, OutputTextOption) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--format", "text"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();

    ASSERT_EQ(options.outputFormat, "text");
    ASSERT_EQ(settings.exportSettings.format, ExportFormat::PLAINTEXT);
}

TEST_F(CLIConfigTest, NoColorOption) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--color", "never"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();

    ASSERT_EQ(options.colorOption, CLIConfig::ColorOption::NEVER);
    ASSERT_TRUE(settings.exportSettings.outputNoColor);
}

TEST_F(CLIConfigTest, SortByTimestampAsc) {
    auto result = parse({"log_analyzer", "--sort-by", "time", "--order", "asc", "dummy_log_file.log"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_TRUE(options.sortBy.has_value());
    ASSERT_EQ(options.sortBy.value(), SortBy::TIMESTAMP);
    ASSERT_TRUE(options.sortOrder.has_value());
    ASSERT_EQ(options.sortOrder.value(), SortOrder::ASCENDING);
    // Verify LogAnalyzerSettings through ExportSettings
    ASSERT_TRUE(settings.exportSettings.sortBy.has_value());
    ASSERT_EQ(settings.exportSettings.sortBy.value(), SortBy::TIMESTAMP);
    ASSERT_TRUE(settings.exportSettings.sortOrder.has_value());
    ASSERT_EQ(settings.exportSettings.sortOrder.value(), SortOrder::ASCENDING);
}

TEST_F(CLIConfigTest, SortByCaseInsensitivity) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--sort-by", "TIME"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.sortBy.has_value());
    ASSERT_EQ(options.sortBy.value(), SortBy::TIMESTAMP);
}

TEST_F(CLIConfigTest, SortOrderCaseInsensitivity) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--order", "DESC"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.sortOrder.has_value());
    ASSERT_EQ(options.sortOrder.value(), SortOrder::DESCENDING);
}

TEST_F(CLIConfigTest, CustomTextOutputFormat) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--text-format", "{level}: {message}"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.textOutputFormat, "{level}: {message}");
    ASSERT_EQ(settings.exportSettings.textOutputFormat, "{level}: {message}");
}

TEST_F(CLIConfigTest, IncludeSummary) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--include-summary"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_TRUE(options.includeSummary);
    ASSERT_TRUE(settings.exportSettings.includeSummary);
}

TEST_F(CLIConfigTest, PrettyPrint) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--pretty"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_TRUE(options.prettyPrint);
    ASSERT_TRUE(settings.exportSettings.prettyPrint);
}

TEST_F(CLIConfigTest, ColorOptionAuto) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--color", "auto"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.colorOption, CLIConfig::ColorOption::AUTO);
    ASSERT_FALSE(settings.exportSettings.outputNoColor); // Default behavior for AUTO
}

TEST_F(CLIConfigTest, CsvSeparatorAndFields) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--csv-sep", ";", "--csv-fields", "level,message"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.csvSeparator, ';');
    ASSERT_EQ(options.csvFields.size(), 2);
    ASSERT_EQ(options.csvFields[0], "level");
    ASSERT_EQ(options.csvFields[1], "message");
    ASSERT_EQ(settings.exportSettings.csvSeparator, ';');
    ASSERT_EQ(settings.exportSettings.csvFields.size(), 2);
    ASSERT_EQ(settings.exportSettings.csvFields[0].first, "level");
    ASSERT_EQ(settings.exportSettings.csvFields[1].first, "message");
}

TEST_F(CLIConfigTest, CsvFieldsWithAliases) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--csv-fields", "timestamp as Time, level as Severity"});
    ASSERT_TRUE(result.has_value());
    auto& settings = result.value().first;
    ASSERT_EQ(settings.exportSettings.csvFields.size(), 2);
    ASSERT_EQ(settings.exportSettings.csvFields[0].first, "timestamp");
    ASSERT_EQ(settings.exportSettings.csvFields[0].second, "Time");
    ASSERT_EQ(settings.exportSettings.csvFields[1].first, "level");
    ASSERT_EQ(settings.exportSettings.csvFields[1].second, "Severity");
}

TEST_F(CLIConfigTest, JsonFields) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--json-fields", "timestamp,level"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.jsonFields.size(), 2);
    ASSERT_EQ(options.jsonFields[0], "timestamp");
    ASSERT_EQ(options.jsonFields[1], "level");
    ASSERT_EQ(settings.exportSettings.jsonFields.size(), 2);
    ASSERT_EQ(settings.exportSettings.jsonFields[0].first, "timestamp");
    ASSERT_EQ(settings.exportSettings.jsonFields[1].first, "level");
}

TEST_F(CLIConfigTest, OutputFormatCaseInsensitivity) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--format", "JSON"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.outputFormat, "json"); // Stored as lowercase
    ASSERT_EQ(settings.exportSettings.format, ExportFormat::JSON);
}

TEST_F(CLIConfigTest, ColorOptionCaseInsensitivity) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--color", "ALWAYS"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.colorOption, CLIConfig::ColorOption::ALWAYS);
    ASSERT_FALSE(settings.exportSettings.outputNoColor); // "always" implies color is not disabled
}
