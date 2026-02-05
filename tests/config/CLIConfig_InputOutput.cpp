// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "tests/config/CLIConfigTest.h"

TEST_F(CLIConfigTest, ParseSingleFilePathAndVerifyOptions) {
    auto result = parse({"log_analyzer", "dummy_log_file.log"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    
    // Verify CLIOptions
    ASSERT_EQ(options.filePaths.size(), 1);
    ASSERT_EQ(options.filePaths[0], "dummy_log_file.log");
    ASSERT_FALSE(options.readFromStdin);
    // LogAnalyzerSettings does not directly store filePaths, it's an input source.
}

TEST_F(CLIConfigTest, ReadFromStdin) {
    auto result = parse({"log_analyzer", "--stdin"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.readFromStdin);
}

TEST_F(CLIConfigTest, ReadFromStdinWithDash) {
    auto result = parse({"log_analyzer", "-"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.readFromStdin);
    ASSERT_TRUE(options.filePaths.empty());
}

TEST_F(CLIConfigTest, ParseMultipleFilePaths) {
    std::ofstream dummy_file2("dummy_log_file2.log");
    dummy_file2 << "more dummy content\n";
    dummy_file2.close();

    auto result = parse({"log_analyzer", "dummy_log_file.log", "dummy_log_file2.log"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;

    ASSERT_EQ(options.filePaths.size(), 2);
    ASSERT_EQ(options.filePaths[0], "dummy_log_file.log");
    ASSERT_EQ(options.filePaths[1], "dummy_log_file2.log");
    // LogAnalyzerSettings does not directly store filePaths, it's an input source.
}

TEST_F(CLIConfigTest, OutputPath) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--output", "output.txt"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.outputPath, "output.txt");
    ASSERT_EQ(settings.exportSettings.outputPath, "output.txt");
}

TEST_F(CLIConfigTest, NonExistentFilePath) {
    auto result = parse({"log_analyzer", "non_existent_file.log"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.filePaths.size(), 1);
    ASSERT_EQ(options.filePaths[0], "non_existent_file.log");
    // LogAnalyzerSettings does not directly store filePaths
}

TEST_F(CLIConfigTest, StreamMode) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--stream"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_TRUE(options.streamMode);
    ASSERT_TRUE(settings.exportSettings.streamMode);
}

TEST_F(CLIConfigTest, TailMode) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--tail"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_TRUE(options.tailMode);
    ASSERT_TRUE(settings.exportSettings.tailMode);
}

TEST_F(CLIConfigTest, TailInterval) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--tail", "--tail-interval", "500"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.tailInterval, std::chrono::milliseconds(500));
    ASSERT_EQ(settings.exportSettings.tailInterval, std::chrono::milliseconds(500));
}

TEST_F(CLIConfigTest, MaxMultilineBufferHumanReadable) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--max-multiline-buffer", "5MB"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.maxMultilineBufferSize, 5 * 1024 * 1024);
}
