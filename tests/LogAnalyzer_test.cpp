#include <gtest/gtest.h>
#include "LogAnalyzer.h"
#include "LogTypes.h"
#include "LogAnalyzerSettings.h"
#include "LogAnalyzerConfig.h"
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
    analyzer.loadAndReplace(filePath, std::string(DEFAULT_LOG_REGEX_PATTERN));
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
    analyzer.loadAndReplace(filePath, std::string(DEFAULT_LOG_REGEX_PATTERN));
    const auto& entries = analyzer.getEntries();
    ASSERT_EQ(entries.size(), 1);
    ASSERT_EQ(entries[0].level, LogLevel::ERROR);
    ASSERT_EQ(entries[0].message, "This is an error.");
    std::remove(filePath.c_str());
}

TEST_F(LogAnalyzerTest, AnalyzeStreamFileOpenError) {
    std::vector<std::string> filePaths = {"non_existent_file.log"};
    auto callback = [](const LogEntry&) { return true; };
    auto result = analyzer.analyzeStream(filePaths, callback, "");
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, ParseError::FILE_OPEN_FAILED);
}

TEST_F(LogAnalyzerTest, AnalyzeStreamInvalidRegexError) {
    std::vector<std::string> filePaths = {"valid_but_unused.log"};
    std::ofstream ofs(filePaths[0]);
    ofs << "dummy log line";
    ofs.close();
    auto callback = [](const LogEntry&) { return true; };
    auto result = analyzer.analyzeStream(filePaths, callback, "(");
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, ParseError::INVALID_REGEX_PATTERN);
    std::remove(filePaths[0].c_str());
}

TEST_F(LogAnalyzerTest, GetFilteredEntriesInvalidRegex) {
    std::string logContent = "2023-01-01 12:00:00 INFO: Valid entry\n";
    std::string filePath = "test_valid_entry.log";
    std::ofstream ofs(filePath);
    ofs << logContent;
    ofs.close();
    analyzer.loadAndReplace(filePath, std::string(DEFAULT_LOG_REGEX_PATTERN));
    std::remove(filePath.c_str());
    FilterCriteria criteria;
    criteria.regexPattern = "[invalid regex";
    auto result = analyzer.getFilteredEntries(criteria);
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, ParseError::INVALID_REGEX_PATTERN);
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
        "2023-01-01 10:00:00 INFO Message with \"quotes\" and \\backslashes\\\n"
        "2023-01-01 10:01:00 DEBUG Message with /slashes/ and newlines\nand tabs\tcharacters\n";
    std::string filePath = "test_json_edge_cases.log";
    std::ofstream ofs(filePath);
    ofs << logContent;
    ofs.close();
    analyzer.loadAndReplace(filePath, std::string(DEFAULT_LOG_REGEX_PATTERN));
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
    ASSERT_NE(j["entries"][0]["message"].get<std::string>().find("quotes"), std::string::npos);
    
    // Check second entry
    ASSERT_EQ(j["entries"][1]["level"], "DEBUG");
    ASSERT_NE(j["entries"][1]["message"].get<std::string>().find("tabs"), std::string::npos);
}

TEST_F(LogAnalyzerTest, GetFrequencyDistributionEdgeCases) {
    std::vector<TimeWindowStats> emptyStats = analyzer.getFrequencyDistribution(std::chrono::seconds(1));
    ASSERT_TRUE(emptyStats.empty());
}

TEST_F(LogAnalyzerTest, AppendCorrectness) {
    std::string logContent1 = "2023-01-01 10:00:00 INFO Entry 1\n";
    std::string filePath1 = "test_append_1.log";
    std::ofstream ofs1(filePath1);
    ofs1 << logContent1;
    ofs1.close();
    auto result1 = analyzer.append(filePath1, std::string(DEFAULT_LOG_REGEX_PATTERN));
    ASSERT_TRUE(result1.has_value());
    std::remove(filePath1.c_str());
}

TEST_F(LogAnalyzerTest, Concurrency_Placeholder) {
    SUCCEED();
}
