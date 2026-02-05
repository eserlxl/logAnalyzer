// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 eserlxl

#include "gtest/gtest.h"
#include "config/Settings.h"
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

class ConfigCoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for test files
        testDir = fs::temp_directory_path() / "log_analyzer_tests";
        fs::create_directories(testDir);
    }

    void TearDown() override {
        // Clean up the temporary directory
        fs::remove_all(testDir);
    }

    void createTestFile(const fs::path& path, const std::string& content) {
        std::ofstream ofs(path);
        ofs << content;
        ofs.close();
    }

    fs::path testDir;
};

TEST_F(ConfigCoreTest, CreateDefault) {
    LogAnalyzerSettings settings = LogAnalyzerSettings::createDefault();
    ASSERT_EQ(settings.lineParsePattern, R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) ([A-Z]+): (.*)$)");
    ASSERT_EQ(settings.fieldMappings.size(), 3);
    ASSERT_TRUE(std::holds_alternative<LogEntryField>(settings.fieldMappings[0].field));
    ASSERT_EQ(std::get<LogEntryField>(settings.fieldMappings[0].field), LogEntryField::TIMESTAMP);
    ASSERT_TRUE(std::holds_alternative<LogEntryField>(settings.fieldMappings[1].field));
    ASSERT_EQ(std::get<LogEntryField>(settings.fieldMappings[1].field), LogEntryField::LEVEL);
    ASSERT_TRUE(std::holds_alternative<LogEntryField>(settings.fieldMappings[2].field));
    ASSERT_EQ(std::get<LogEntryField>(settings.fieldMappings[2].field), LogEntryField::MESSAGE);
    ASSERT_FALSE(settings.caseSensitiveParsing);
}

TEST_F(ConfigCoreTest, Merge) {
    LogAnalyzerSettings base = LogAnalyzerSettings::createDefault();
    LogAnalyzerSettings overlay;
    overlay.setLineParsePattern("new_pattern");
    overlay.setCaseSensitiveParsing(true);
    overlay.addFilterRule({LogEntryField::MESSAGE, FilterOperator::CONTAINS, "error"});

    base.merge(overlay);

    ASSERT_EQ(base.lineParsePattern, "new_pattern");
    ASSERT_TRUE(base.caseSensitiveParsing);
    ASSERT_EQ(base.filterRules.size(), 1);
    ASSERT_EQ(base.filterRules[0].value, "error");
}

TEST_F(ConfigCoreTest, EnvVarExpansion) {
    // Set a test environment variable
#ifdef _WIN32
    _putenv("TEST_VAR=expanded_value");
#else
    setenv("TEST_VAR", "expanded_value", 1);
#endif

    std::string jsonContent = R"({ "lineParsePattern": "${TEST_VAR}" })";
    auto configPath = testDir / "config.json";
    createTestFile(configPath, jsonContent);

    auto result = LogAnalyzerSettings::fromFile(configPath, true);
    ASSERT_TRUE(result.has_value()) << "Errors: " << (result.has_value() ? "" : result.error()[0]);
    
    LogAnalyzerSettings settings = result.value();
    ASSERT_EQ(settings.lineParsePattern, "expanded_value");

#ifdef _WIN32
    _putenv("TEST_VAR=");
#else
    unsetenv("TEST_VAR");
#endif
}

TEST_F(ConfigCoreTest, FileIncludes) {
    std::string baseJson = R"({ "lineParsePattern": "base_pattern" })";
    std::string rootJson = R"({ "version": "1.0", "includes": ["base.json"], "caseSensitiveParsing": true })";

    auto basePath = testDir / "base.json";
    auto rootPath = testDir / "root.json";

    createTestFile(basePath, baseJson);
    createTestFile(rootPath, rootJson);

    auto result = LogAnalyzerSettings::fromFile(rootPath);
    ASSERT_TRUE(result.has_value()) << "Errors: " << (result.has_value() ? "" : result.error()[0]);

    LogAnalyzerSettings settings = result.value();
    ASSERT_EQ(settings.lineParsePattern, "base_pattern");
    ASSERT_TRUE(settings.caseSensitiveParsing);
    ASSERT_EQ(settings.version, "1.0");
}

TEST_F(ConfigCoreTest, IncludeOverridesRoot) {
    std::string baseJson = R"({ "lineParsePattern": "base_pattern", "caseSensitiveParsing": false })";
    std::string rootJson = R"({ "includes": ["base.json"], "lineParsePattern": "root_pattern" })";

    auto basePath = testDir / "base.json";
    auto rootPath = testDir / "root.json";

    createTestFile(basePath, baseJson);
    createTestFile(rootPath, rootJson);

    auto result = LogAnalyzerSettings::fromFile(rootPath);
    ASSERT_TRUE(result.has_value()) << "Errors: " << (result.has_value() ? "" : result.error()[0]);

    LogAnalyzerSettings settings = result.value();
    // Root value should override included value
    ASSERT_EQ(settings.lineParsePattern, "root_pattern");
    // Value from base should persist if not in root
    ASSERT_FALSE(settings.caseSensitiveParsing);
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
    ASSERT_GE(result.error().size(), 1);
    bool found = false;
    for(const auto& err : result.error()) {
        if(err.find("Circular include detected") != std::string::npos) {
            found = true;
            break;
        }
    }
    ASSERT_TRUE(found);
}
