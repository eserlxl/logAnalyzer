// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "gtest/gtest.h"
#include <gmock/gmock.h>
#include "config/settings.h"
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <chrono>

using namespace filter;

namespace fs = std::filesystem;

class ConfigCoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
        testDir = fs::temp_directory_path() / ("log_analyzer_tests_" + std::to_string(nanos));
        fs::create_directories(testDir);
    }

    void TearDown() override {
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    static void createTestFile(const fs::path& path, const std::string& content) {
        std::ofstream ofs(path);
        ofs << content;
    }

    static void loadAndVerify(const fs::path& path, LogAnalyzerSettings& settings, bool expandEnv = true) {
        auto result = LogAnalyzerSettings::fromFile(path, expandEnv);
        if (!result.has_value()) {
            std::string allErrors;
            for (const auto& err : result.error()) {
                allErrors += err + "\n";
            }
            FAIL() << "Failed to load config from " << path << ". Errors:\n" << allErrors;
        }
        settings = result.value();
    }

    fs::path testDir;
};

TEST_F(ConfigCoreTest, CreateDefault) {
    LogAnalyzerSettings settings = LogAnalyzerSettings::createDefault();
    ASSERT_EQ(settings.lineParsePattern, DEFAULT_LOG_REGEX_PATTERN_INTERNAL);
    ASSERT_EQ(settings.fieldMappings.size(), 3);

    auto checkField = [&](size_t index, LogEntryField expected) {
        ASSERT_TRUE(std::holds_alternative<LogEntryField>(settings.fieldMappings[index].field));
        ASSERT_EQ(std::get<LogEntryField>(settings.fieldMappings[index].field), expected);
    };

    checkField(0, LogEntryField::TIMESTAMP);
    checkField(1, LogEntryField::LEVEL);
    checkField(2, LogEntryField::MESSAGE);
    ASSERT_FALSE(settings.caseSensitiveParsing.value_or(false));
}

TEST_F(ConfigCoreTest, Merge) {
    LogAnalyzerSettings base = LogAnalyzerSettings::createDefault();
    LogAnalyzerSettings overlay;
    overlay.setLineParsePattern("new_pattern");
    overlay.setCaseSensitiveParsing(true);
    overlay.addFilterRule({.field=LogEntryField::MESSAGE, .op=FilterOperator::CONTAINS, .value="error"});

    base.merge(overlay);

    ASSERT_EQ(base.lineParsePattern, "new_pattern");
    ASSERT_TRUE(base.caseSensitiveParsing);
    ASSERT_EQ(base.filterRules.size(), 1);
    ASSERT_EQ(base.filterRules[0].value, "error");
}

TEST_F(ConfigCoreTest, EnvVarExpansion) {
    // Set a test environment variable
#ifdef _WIN32
    _putenv("LOG_ANALYZER_TEST_VAR=expanded_value");
#else
    setenv("LOG_ANALYZER_TEST_VAR", "expanded_value", 1);
#endif

    std::string jsonContent = R"({ "lineParsePattern": "${LOG_ANALYZER_TEST_VAR}" })";
    auto configPath = testDir / "config.json";
    createTestFile(configPath, jsonContent);

    LogAnalyzerSettings settings;
    loadAndVerify(configPath, settings, true);
    ASSERT_EQ(settings.lineParsePattern, "expanded_value");

#ifdef _WIN32
    _putenv("LOG_ANALYZER_TEST_VAR=");
#else
    unsetenv("LOG_ANALYZER_TEST_VAR");
#endif
}

TEST_F(ConfigCoreTest, FileIncludes) {
    std::string baseJson = R"({ "lineParsePattern": "base_pattern" })";
    std::string rootJson = R"({ "version": "1.0", "includes": ["base.json"], "caseSensitiveParsing": true })";

    auto rootPath = testDir / "root.json";
    createTestFile(testDir / "base.json", baseJson);
    createTestFile(rootPath, rootJson);

    LogAnalyzerSettings settings;
    loadAndVerify(rootPath, settings);
    ASSERT_EQ(settings.lineParsePattern, "base_pattern");
    ASSERT_TRUE(settings.caseSensitiveParsing);
    ASSERT_EQ(settings.version, "1.0");
}

TEST_F(ConfigCoreTest, IncludeOverridesRoot) {
    std::string baseJson = R"({ "lineParsePattern": "base_pattern", "caseSensitiveParsing": false })";
    std::string rootJson = R"({ "includes": ["base.json"], "lineParsePattern": "root_pattern" })";

    auto rootPath = testDir / "root.json";
    createTestFile(testDir / "base.json", baseJson);
    createTestFile(rootPath, rootJson);

    LogAnalyzerSettings settings;
    loadAndVerify(rootPath, settings);
    // Root value should override included value
    ASSERT_EQ(settings.lineParsePattern, "root_pattern");
    // Value from base should persist if not in root
    ASSERT_FALSE(settings.caseSensitiveParsing.value_or(false));
}

TEST_F(ConfigCoreTest, PartialMergePreservesValues) {
    LogAnalyzerSettings base = LogAnalyzerSettings::createDefault();
    base.setCaseSensitiveParsing(true);
    base.setLineParsePattern("base_pattern");
    
    LogAnalyzerSettings overlay;
    // overlay has default values for caseSensitiveParsing (std::nullopt) and lineParsePattern (DEFAULT)
    
    base.merge(overlay);
    
    // Values should be preserved if overlay doesn't specify them
    EXPECT_TRUE(base.caseSensitiveParsing.value_or(false));
    EXPECT_EQ(base.lineParsePattern, "base_pattern");
}

TEST_F(ConfigCoreTest, ExportSettingsMerge) {
    LogAnalyzerSettings base = LogAnalyzerSettings::createDefault();
    base.exportSettings.outputPath = "base_output.log";
    base.exportSettings.format = ExportFormat::JSON;
    
    LogAnalyzerSettings overlay;
    overlay.exportSettings.outputPath = "overlay_output.log";
    // overlay.exportSettings.format is nullopt
    
    base.merge(overlay);
    
    EXPECT_EQ(base.exportSettings.outputPath, "overlay_output.log");
    EXPECT_EQ(base.exportSettings.format, ExportFormat::JSON);
}

TEST_F(ConfigCoreTest, AdditiveCollectionMerge) {
    LogAnalyzerSettings base = LogAnalyzerSettings::createDefault();
    base.filterRules.push_back({LogEntryField::LEVEL, FilterOperator::EQUALS, "ERROR"});
    
    LogAnalyzerSettings overlay;
    overlay.filterRules.push_back({LogEntryField::MESSAGE, FilterOperator::CONTAINS, "critical"});
    
    base.merge(overlay);
    
    ASSERT_EQ(base.filterRules.size(), 2);
    EXPECT_EQ(base.filterRules[0].value, "ERROR");
    EXPECT_EQ(base.filterRules[1].value, "critical");
}

TEST_F(ConfigCoreTest, CircularIncludeDetection) {
    std::string file1Json = R"({ "includes": ["file2.json"] })";
    std::string file2Json = R"({ "includes": ["file1.json"] })";

    auto path1 = testDir / "file1.json";
    auto path2 = testDir / "file2.json";

    createTestFile(path1, file1Json);
    createTestFile(path2, file2Json);

    auto result = LogAnalyzerSettings::fromFile(path1);
    ASSERT_FALSE(result.has_value());
    ASSERT_THAT(result.error(), testing::Contains(testing::HasSubstr("Circular include detected")));
}

TEST_F(ConfigCoreTest, ComprehensiveMerge) {
    LogAnalyzerSettings base;
    base.setLineParsePattern("base_pattern");
    base.setCaseSensitiveParsing(false);
    base.parserErrorAction = ParserErrorAction::Warn;
    base.maxMultilineBufferSize = 1024;
    
    LogAnalyzerSettings overlay;
    overlay.setLineParsePattern("overlay_pattern");
    overlay.setCaseSensitiveParsing(true);
    overlay.setLogEntryStartPattern("start_pattern");
    overlay.parserErrorAction = ParserErrorAction::Throw;
    overlay.maxMultilineBufferSize = 2048;
    
    base.merge(overlay);
    
    EXPECT_EQ(base.lineParsePattern, "overlay_pattern");
    EXPECT_TRUE(base.caseSensitiveParsing.value());
    EXPECT_EQ(base.logEntryStartPattern.value(), "start_pattern");
    EXPECT_EQ(base.parserErrorAction, ParserErrorAction::Throw);
    EXPECT_EQ(base.maxMultilineBufferSize, 2048);
}

TEST_F(ConfigCoreTest, AdvancedMerge) {
    LogAnalyzerSettings base;
    base.statisticConfigs.push_back({StatisticType::UNIQUE_MESSAGES, {}});
    
    LogAnalyzerSettings overlay;
    overlay.statisticConfigs.push_back({StatisticType::TOP_MESSAGES, {{"top_n", "5"}}});
    filter::FilterExpression expr(filter::FilterLogicalOperator::AND);
    overlay.rootFilterExpression = expr;
    
    base.merge(overlay);
    
    ASSERT_EQ(base.statisticConfigs.size(), 2);
    EXPECT_EQ(base.statisticConfigs[0].type, StatisticType::UNIQUE_MESSAGES);
    EXPECT_EQ(base.statisticConfigs[1].type, StatisticType::TOP_MESSAGES);
    EXPECT_EQ(base.statisticConfigs[1].params.at("top_n"), "5");
    ASSERT_TRUE(base.rootFilterExpression.has_value());
    EXPECT_EQ(base.rootFilterExpression->getLogicalOperator(), filter::FilterLogicalOperator::AND);
}

TEST_F(ConfigCoreTest, MissingIncludeDetection) {
    std::string rootJson = R"({ "includes": ["non_existent.json"] })";
    auto rootPath = testDir / "root.json";
    createTestFile(rootPath, rootJson);

    auto result = LogAnalyzerSettings::fromFile(rootPath);
    ASSERT_FALSE(result.has_value());
    ASSERT_THAT(result.error(), testing::Contains(testing::HasSubstr("Failed to resolve include path")));
}
