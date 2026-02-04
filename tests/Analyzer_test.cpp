#include <gtest/gtest.h>
#include "Analyzer.h"
#include "LogTypes.h"
#include "Settings.h"
#include "Config.h"
#include "Utils.h"
#include <sstream>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>

class LogAnalyzerTest : public ::testing::Test {
protected:
    LogAnalyzer analyzer;
    void SetUp() override { analyzer.clear(); }
};

TEST_F(LogAnalyzerTest, SetCustomLogLevelMapping) {
    analyzer.setCustomLogLevelMapping("CRITICAL", LogLevel::FATAL);
    std::string logContent = "2023-01-01 12:00:00 CRITICAL: This is a critical error.\n";
    std::string filePath = "test_custom_level.log";
    std::ofstream ofs(filePath);
    ofs << logContent;
    ofs.close();
    analyzer.loadAndReplace(filePath, CLIConfig::ParserErrorAction::Warn); // Updated to non-deprecated
    const auto& entries = analyzer.getEntries();
    ASSERT_EQ(entries.size(), 1);
    ASSERT_EQ(entries[0].level, LogLevel::FATAL);
    ASSERT_EQ(entries[0].message, "This is a critical error.");
    std::remove(filePath.c_str());
}

TEST_F(LogAnalyzerTest, DefaultLogLevelMapping) {
    std::string logContent = "2023-01-01 12:00:00 ERROR: This is an error.\n";
    std::string filePath = "test_default_level.log";
    std::ofstream ofs(filePath);
    ofs << logContent;
    ofs.close();
    analyzer.loadAndReplace(filePath, CLIConfig::ParserErrorAction::Warn); // Updated to non-deprecated
    const auto& entries = analyzer.getEntries();
    ASSERT_EQ(entries.size(), 1);
    ASSERT_EQ(entries[0].level, LogLevel::ERROR);
    ASSERT_EQ(entries[0].message, "This is an error.");
    std::remove(filePath.c_str());
}

TEST_F(LogAnalyzerTest, AnalyzeStreamFileOpenError) {
    std::vector<std::string> filePaths = {"non_existent_file.log"};
    auto callback = [](const LogEntry&) { return true; };
    auto result = analyzer.analyzeStream(filePaths, callback, CLIConfig::ParserErrorAction::Warn);
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::FileNotReadable);
}

TEST_F(LogAnalyzerTest, AnalyzeStreamInvalidRegexError) {
    std::vector<std::string> filePaths = {"valid_but_unused.log"};
    std::ofstream ofs(filePaths[0]);
    ofs << "dummy log line";
    ofs.close();
    auto callback = [](const LogEntry&) { return true; };
    auto result = analyzer.analyzeStream(filePaths, callback, CLIConfig::ParserErrorAction::Warn);
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidRegex);
    std::remove(filePaths[0].c_str());
}

TEST_F(LogAnalyzerTest, GetFilteredEntriesInvalidRegex) {
    std::string logContent = "2023-01-01 12:00:00 INFO: Valid entry\n";
    std::string filePath = "test_valid_entry.log";
    std::ofstream ofs(filePath);
    ofs << logContent;
    ofs.close();
    analyzer.loadAndReplace(filePath, CLIConfig::ParserErrorAction::Warn); // Updated to non-deprecated
    std::remove(filePath.c_str());
    FilterCriteria criteria;
    criteria.regexPattern = "[invalid regex";
    auto result = analyzer.getFilteredEntries(criteria);
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidRegex);
}

TEST_F(LogAnalyzerTest, ExportAsCsvEdgeCases) {
    std::stringstream ssEmpty;
    FilterCriteria emptyFilter;
    analyzer.exportAsCsv(ssEmpty, emptyFilter, ',');
    ASSERT_EQ(ssEmpty.str(), "Timestamp,Level,Message,File\n");
}

TEST_F(LogAnalyzerTest, ExportAsJsonEdgeCases) {
    FilterCriteria emptyFilter;
    std::stringstream ssEmptyNoSummary;
    analyzer.exportAsJson(ssEmptyNoSummary, emptyFilter, false);
    nlohmann::json jEmpty = nlohmann::json::parse(ssEmptyNoSummary.str());
    ASSERT_EQ(jEmpty["summary"]["totalEntries"], 0);
    ASSERT_TRUE(jEmpty["entries"].empty());

    std::string logContent = 
        "2023-01-01 10:00:00 INFO First message.\n"
        "2023-01-01 10:01:00 DEBUG Second message.\n";
    std::string filePath = "test_json_edge_cases.log";
    std::ofstream ofs(filePath);
    ofs << logContent;
    ofs.close();
    auto reportResult = analyzer.loadAndReplace(filePath, CLIConfig::ParserErrorAction::Warn);
    ASSERT_TRUE(reportResult.has_value()) << "Load failed with error: " << reportResult.error().toString();
    AnalysisReport report = reportResult.value();
    ASSERT_EQ(report.successfulParses, 2) << "Expected 2 successful parses, but got " << report.successfulParses;
    std::remove(filePath.c_str());

    std::stringstream ss;
    FilterCriteria allFilter;
    analyzer.exportAsJson(ss, allFilter, true);

    nlohmann::json j = nlohmann::json::parse(ss.str());
    ASSERT_EQ(j["summary"]["totalEntries"], 2);
    ASSERT_EQ(j["entries"].size(), 2);
    
    // Check first entry
    ASSERT_EQ(j["entries"][0]["level"], "INFO");
    // Verify message content (JSON parsing handles escapes)
    ASSERT_NE(j["entries"][0]["message"].get<std::string>().find("First message."), std::string::npos); // Updated message check
    
    // Check second entry
    ASSERT_EQ(j["entries"][1]["level"], "DEBUG");
    ASSERT_NE(j["entries"][1]["message"].get<std::string>().find("Second message."), std::string::npos); // Updated message check
}

// TEST_F(LogAnalyzerTest, GetFrequencyDistributionEdgeCases) {
//     std::vector<TimeWindowStats> emptyStats = analyzer.getFrequencyDistribution(std::chrono::seconds(1));
//     ASSERT_TRUE(emptyStats.empty());
// }

TEST_F(LogAnalyzerTest, AppendCorrectness) {
    std::string logContent1 = "2023-01-01 10:00:00 INFO Entry 1\n";
    std::string filePath1 = "test_append_1.log";
    std::ofstream ofs1(filePath1);
    ofs1 << logContent1;
    ofs1.close();
    auto result1 = analyzer.append(filePath1, CLIConfig::ParserErrorAction::Warn); // Updated to non-deprecated
    ASSERT_TRUE(result1.has_value());
    std::remove(filePath1.c_str());
}

TEST_F(LogAnalyzerTest, Concurrency_Placeholder) {
    SUCCEED();
}
