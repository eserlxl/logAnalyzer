#include <gtest/gtest.h>
#include "LogAnalyzer.h"
#include "LogTypes.h"
#include "LogAnalyzerSettings.h" // Added for LogAnalyzerSettings
#include "LogAnalyzerConfig.h" // Added for DEFAULT_LOG_REGEX_PATTERN
#include "Utils.h" // For formatTimestamp etc.
#include <sstream>
#include <chrono>
#include <fstream> // Required for std::ofstream


// Define a test fixture for LogAnalyzer if common setup/teardown is needed
class LogAnalyzerTest : public ::testing::Test {
protected:
    LogAnalyzer analyzer;

    void SetUp() override {
        // Common setup for tests, if any
        // Clear any previous state for a clean test run
        analyzer.clear();
    }

    void TearDown() override {
        // Common teardown for tests, if any
    }
};

// Test case for setCustomLogLevelMapping
TEST_F(LogAnalyzerTest, SetCustomLogLevelMapping) {
    // 1. Set a custom log level mapping
    analyzer.setCustomLogLevelMapping("CRITICAL", LogLevel::FATAL);

    // 2. Prepare a log file content with the custom level
    std::string logContent = "2023-01-01 12:00:00 CRITICAL: This is a critical error.\n";
    std::string filePath = "test_custom_level.log";
    std::ofstream ofs(filePath);
    ofs << logContent;
    ofs.close();

    // 3. Load and analyze the log file
    // Use the default regex pattern, which should capture timestamp, level, message.
    // The default pattern usually covers something like: (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) (\S+) (.*)
    // We expect the second group to be the level.
    AnalysisReport report = analyzer.loadAndReplace(filePath, std::string(DEFAULT_LOG_REGEX_PATTERN));
    
    // Check if loading was successful
    ASSERT_EQ(report.status, ParseError::SUCCESS);
    ASSERT_EQ(report.successfulParses, 1);
    ASSERT_EQ(report.parseErrors.size(), 0);

    // 4. Get the parsed entries
    const auto& entries = analyzer.getEntries();
    ASSERT_EQ(entries.size(), 1);

    // 5. Verify the log level of the parsed entry
    ASSERT_EQ(entries[0].level, LogLevel::FATAL);
    ASSERT_EQ(entries[0].message, "This is a critical error.");

    // Clean up the created file
    std::remove(filePath.c_str());
}

// Test case for default log level mapping (ensure it still works)
TEST_F(LogAnalyzerTest, DefaultLogLevelMapping) {
    // No custom mapping set, should use default
    
        std::string logContent = "2023-01-01 12:00:00 ERROR: This is an error.\n";    std::string filePath = "test_default_level.log";
    std::ofstream ofs(filePath);
    ofs << logContent;
    ofs.close();

    AnalysisReport report = analyzer.loadAndReplace(filePath, std::string(DEFAULT_LOG_REGEX_PATTERN));
    
    ASSERT_EQ(report.status, ParseError::SUCCESS);
    ASSERT_EQ(report.successfulParses, 1);
    ASSERT_EQ(report.parseErrors.size(), 0);

    const auto& entries = analyzer.getEntries();
    ASSERT_EQ(entries.size(), 1);

    ASSERT_EQ(entries[0].level, LogLevel::ERROR);
    ASSERT_EQ(entries[0].message, "This is an error.");

    std::remove(filePath.c_str());
}

// Test analyzeStream error propagation for file open failure
TEST_F(LogAnalyzerTest, AnalyzeStreamFileOpenError) {
    std::vector<std::string> filePaths = {"non_existent_file.log"};
    std::vector<LogEntry> processedEntries;
    
    auto callback = [&](const LogEntry& entry) {
        processedEntries.push_back(entry);
        return true; // Continue processing
    };

    auto result = analyzer.analyzeStream(filePaths, callback, "");

    // Expect an error due to file open failure
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, ParseError::FILE_OPEN_FAILED);
    ASSERT_NE(result.error().message.find("Could not open file"), std::string::npos);
    ASSERT_TRUE(processedEntries.empty()); // No entries should have been processed
}

// Test analyzeStream error propagation for invalid regex pattern
TEST_F(LogAnalyzerTest, AnalyzeStreamInvalidRegexError) {
    std::vector<std::string> filePaths = {"valid_but_unused.log"};
    std::vector<LogEntry> processedEntries;
    
    // Create a dummy file so file open doesn't fail first
    std::ofstream ofs(filePaths[0]);
    ofs << "dummy log line";
    ofs.close();

    auto callback = [&](const LogEntry& entry) {
        processedEntries.push_back(entry);
        return true; // Continue processing
    };

    // Use an invalid regex pattern, e.g., unbalanced parenthesis
    auto result = analyzer.analyzeStream(filePaths, callback, "(");

    // Expect an error due to invalid regex pattern
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, ParseError::INVALID_REGEX_PATTERN);
    // Check for common messages indicating unmatched parenthesis or bracket errors for regex issues
    ASSERT_TRUE(result.error().message.find("regex_error") != std::string::npos ||
                result.error().message.find("Mismatched '(' and ')'") != std::string::npos ||
                result.error().message.find("unmatched ')'") != std::string::npos ||
                result.error().message.find("unmatched '['") != std::string::npos ||
                result.error().message.find("unmatched '{'") != std::string::npos ||
                result.error().message.find("invalid repetition operator") != std::string::npos ||
                result.error().message.find("invalid character in character class") != std::string::npos);
    ASSERT_TRUE(processedEntries.empty()); // No entries should have been processed

    std::remove(filePaths[0].c_str());
}

// Test invalid regex filtering for getFilteredEntries
TEST_F(LogAnalyzerTest, GetFilteredEntriesInvalidRegex) {
    // Add some dummy entries to the analyzer first
    std::string logContent = "2023-01-01 12:00:00 INFO: Valid entry\n";
    std::string filePath = "test_valid_entry.log";
    std::ofstream ofs(filePath);
    ofs << logContent;
    ofs.close();
    analyzer.loadAndReplace(filePath, std::string(DEFAULT_LOG_REGEX_PATTERN));
    std::remove(filePath.c_str());

    FilterCriteria criteria;
    criteria.regexPattern = "[invalid regex"; // Invalid regex pattern

    auto result = analyzer.getFilteredEntries(criteria);

    // Expect an error due to invalid regex pattern
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, ParseError::INVALID_REGEX_PATTERN);
    // Check for common messages indicating unmatched parenthesis or bracket error
    bool containsExpectedErrorMsg = 
        result.error().message.find("Mismatched '(' and ')'") != std::string::npos ||
        result.error().message.find("unmatched ')'") != std::string::npos ||
        result.error().message.find("unmatched '['") != std::string::npos ||
        result.error().message.find("The expression contained an unmatched bracket expression.") != std::string::npos ||
        result.error().message.find("Unexpected character within '[...]' in regular expression") != std::string::npos ||
        result.error().message.find("regex_error") != std::string::npos; // General fallback

    ASSERT_TRUE(containsExpectedErrorMsg) << "Error message: " << result.error().message;
}

// Test CSV export edge cases: comma in message, newlines in message, and empty entries.
TEST_F(LogAnalyzerTest, ExportAsCsvEdgeCases) {
    // Test with empty entries
    std::stringstream ssEmpty;
    FilterCriteria emptyFilter;
    analyzer.exportAsCsv(ssEmpty, emptyFilter, ',');
    ASSERT_EQ(ssEmpty.str(), "Timestamp,Level,Message,File\n"); // Only header

    // Add entries with commas and newlines
    std::string logContent = 
        "2023-01-01 10:00:00 INFO Message with, comma\n"
        "2023-01-01 10:01:00 WARN Message with\nnewline\n"
        "2023-01-01 10:02:00 ERROR \"Quoted message\" with, comma and\nnewline\n"
        ;
    std::string filePath = "test_csv_edge_cases.log";
    std::ofstream ofs(filePath);
    ofs << logContent;
    ofs.close();
    analyzer.loadAndReplace(filePath, std::string(DEFAULT_LOG_REGEX_PATTERN));
    std::remove(filePath.c_str());

    std::stringstream ss;
    FilterCriteria allFilter;
    analyzer.exportAsCsv(ss, allFilter, ',');

    std::string expectedCsv = 
        "Timestamp,Level,Message,File\n"
        "\"" + Utils::formatTimestamp(analyzer.getEntries()[0].timestamp) + "\",INFO,\"Message with, comma\",test_csv_edge_cases.log\n"
        "\"" + Utils::formatTimestamp(analyzer.getEntries()[1].timestamp) + "\",WARN,\"Message with\nnewline\",test_csv_edge_cases.log\n"
        "\"" + Utils::formatTimestamp(analyzer.getEntries()[2].timestamp) + "\",ERROR,\"\"\"Quoted message\"\" with, comma and\\nnewline\"\"\",test_csv_edge_cases.log\n"
        ;

    // Use a custom comparison function that ignores potential differences in exact timestamp format
    // as long as the components are correct and message escaping is handled.
    // For now, doing a direct string comparison. If it fails, I'll refine this.
    ASSERT_EQ(ss.str(), expectedCsv);
}

// Test JSON export edge cases: empty entries, special characters in message, consistent root.
TEST_F(LogAnalyzerTest, ExportAsJsonEdgeCases) {
    // Test with empty entries, no summary
    std::stringstream ssEmptyNoSummary;
    FilterCriteria emptyFilter;
    analyzer.exportAsJson(ssEmptyNoSummary, emptyFilter, false, false);
    // Expected: {"summary":{"totalEntries":0},"entries":[]}
    ASSERT_EQ(ssEmptyNoSummary.str(), "{\"summary\":{\"totalEntries\":0},\"entries\":[]}\n");

    // Test with empty entries, with summary
    std::stringstream ssEmptyWithSummary;
    analyzer.exportAsJson(ssEmptyWithSummary, emptyFilter, true, false);
    // Expected: {"summary":{"totalEntries":0},"entries":[]}
    ASSERT_EQ(ssEmptyWithSummary.str(), "{\"summary\":{\"totalEntries\":0},\"entries\":[]}\n");

    // Add entries with special characters in messages
    std::string logContent = 
        "2023-01-01 10:00:00 INFO Message with \"quotes\" and \\backslashes\\\n"
        "2023-01-01 10:01:00 DEBUG Message with /slashes/ and newlines\nand tabs\tcharacters\n"
        ;
    std::string filePath = "test_json_edge_cases.log";
    std::ofstream ofs(filePath);
    ofs << logContent;
    ofs.close();
    analyzer.loadAndReplace(filePath, std::string(DEFAULT_LOG_REGEX_PATTERN));
    std::remove(filePath.c_str());

    // Export with pretty printing and summary
    std::stringstream ss;
    FilterCriteria allFilter;
    analyzer.exportAsJson(ss, allFilter, true, true);

    const auto& entries = analyzer.getEntries();
    std::string expectedJson =
        "{\n"
        "  \"summary\": {\"totalEntries\": 2},\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"timestamp\":\"" + Utils::formatTimestamp(entries[0].timestamp) + "\",\n"
        "      \"level\":\"INFO\",\n"
        "      \"message\":\"Message with \\\"quotes\\\" and \\\\backslashes\\\\\",\n"
        "      \"file\":\"test_json_edge_cases.log\"\n"
        "    },\n"
        "    {\n"
        "      \"timestamp\":\"" + Utils::formatTimestamp(entries[1].timestamp) + "\",\n"
        "      \"level\":\"DEBUG\",\n"
        "      \"message\":\"Message with /slashes/ and newlines\\nand tabs\\tcharacters\",\n"
        "      \"file\":\"test_json_edge_cases.log\"\n"
        "    }\n"
        "  ]\n"
        "}\n";

    ASSERT_EQ(ss.str(), expectedJson);

    // Export without pretty printing and no summary
    std::stringstream ssNoPrettyNoSummary;
    analyzer.exportAsJson(ssNoPrettyNoSummary, allFilter, false, false);
    std::string expectedJsonNoPrettyNoSummary =
        "{\"summary\":{\"totalEntries\":2},\"entries\":["
        "{\"timestamp\":\"" + Utils::formatTimestamp(entries[0].timestamp) + "\",\"level\":\"INFO\",\"message\":\"Message with \\\"quotes\\\" and \\\\backslashes\\\\\",\"file\":\"test_json_edge_cases.log\"},"
        "{\"timestamp\":\"" + Utils::formatTimestamp(entries[1].timestamp) + "\",\"level\":\"DEBUG\",\"message\":\"Message with /slashes/ and newlines\\nand tabs\\tcharacters\",\"file\":\"test_json_edge_cases.log\"}"
        "]}\n";
    ASSERT_EQ(ssNoPrettyNoSummary.str(), expectedJsonNoPrettyNoSummary);
}

// Test getFrequencyDistribution edge cases: empty entries, single entry, various window sizes.
TEST_F(LogAnalyzerTest, GetFrequencyDistributionEdgeCases) {
    // Empty entries
    std::vector<TimeWindowStats> emptyStats = analyzer.getFrequencyDistribution(std::chrono::seconds(1));
    ASSERT_TRUE(emptyStats.empty());

    // Single entry
    std::string logContent = "2023-01-01 12:00:00 INFO Single entry\n";
    std::string filePath = "test_single_entry.log";
    std::ofstream ofs(filePath);
    ofs << logContent;
    ofs.close();
    analyzer.loadAndReplace(filePath, std::string(DEFAULT_LOG_REGEX_PATTERN));
    std::remove(filePath.c_str());

    std::vector<TimeWindowStats> singleEntryStats = analyzer.getFrequencyDistribution(std::chrono::seconds(1));
    ASSERT_EQ(singleEntryStats.size(), 1);
    ASSERT_EQ(singleEntryStats[0].totalCount, 1);
    ASSERT_EQ(singleEntryStats[0].counts[LogLevel::INFO], 1);

    analyzer.clear();

    // Multiple entries in one window
    logContent = 
        "2023-01-01 12:00:00 INFO Entry 1\n"
        "2023-01-01 12:00:00.500 DEBUG Entry 2\n"
        "2023-01-01 12:00:01 INFO Entry 3\n"
        ;
    filePath = "test_multi_entry_one_window.log";
    ofs.open(filePath);
    ofs << logContent;
    ofs.close();
    analyzer.loadAndReplace(filePath, std::string(DEFAULT_LOG_REGEX_PATTERN));
    std::remove(filePath.c_str());

    // Use a window size that covers all entries
    std::vector<TimeWindowStats> statsOneWindow = analyzer.getFrequencyDistribution(std::chrono::seconds(10));
    ASSERT_EQ(statsOneWindow.size(), 1);
    ASSERT_EQ(statsOneWindow[0].totalCount, 3);
    ASSERT_EQ(statsOneWindow[0].counts[LogLevel::INFO], 2);
    ASSERT_EQ(statsOneWindow[0].counts[LogLevel::DEBUG], 1);
    analyzer.clear();

    // Multiple entries across multiple windows
    logContent = 
        "2023-01-01 12:00:00 INFO Entry 1\n"
        "2023-01-01 12:00:05 DEBUG Entry 2\n"
        "2023-01-01 12:00:10 WARN Entry 3\n"
        "2023-01-01 12:00:11 ERROR Entry 4\n"
        ;
    filePath = "test_multi_entry_multi_window.log";
    ofs.open(filePath);
    ofs << logContent;
    ofs.close();
    analyzer.loadAndReplace(filePath, std::string(DEFAULT_LOG_REGEX_PATTERN));
    std::remove(filePath.c_str());

    // Window size of 5 seconds
    std::vector<TimeWindowStats> statsMultiWindow = analyzer.getFrequencyDistribution(std::chrono::seconds(5));
    ASSERT_EQ(statsMultiWindow.size(), 3); // 0-5s (2 entries), 5-10s (1 entry), 10-15s (1 entry)

    ASSERT_EQ(statsMultiWindow[0].totalCount, 2);
    ASSERT_EQ(statsMultiWindow[0].counts[LogLevel::INFO], 1);
    ASSERT_EQ(statsMultiWindow[0].counts[LogLevel::DEBUG], 1);

    ASSERT_EQ(statsMultiWindow[1].totalCount, 1); // Entry 2 (12:00:05) falls into this if start is inclusive, end exclusive
    ASSERT_EQ(statsMultiWindow[1].counts[LogLevel::WARNING], 1);

    ASSERT_EQ(statsMultiWindow[2].totalCount, 1);
    ASSERT_EQ(statsMultiWindow[2].counts[LogLevel::ERROR], 1);
    analyzer.clear();
    
    // Test windowSize of 0 or negative
    std::vector<TimeWindowStats> zeroWindowSizeStats = analyzer.getFrequencyDistribution(std::chrono::seconds(0));
    ASSERT_TRUE(zeroWindowSizeStats.empty());
    std::vector<TimeWindowStats> negativeWindowSizeStats = analyzer.getFrequencyDistribution(std::chrono::seconds(-5));
    ASSERT_TRUE(negativeWindowSizeStats.empty());
}

// Test append correctness: empty entries, single entry, merge with existing, merge with new out of order.
TEST_F(LogAnalyzerTest, AppendCorrectness) {
    // 1. Append to empty analyzer
    std::string logContent1 = 
        "2023-01-01 10:00:00 INFO Entry 1\n"
        "2023-01-01 10:00:02 WARN Entry 3\n"; // Deliberately out of order for append to sort later
    std::string filePath1 = "test_append_1.log";
    std::ofstream ofs1(filePath1);
    ofs1 << logContent1;
    ofs1.close();

    auto result1 = analyzer.append(filePath1, std::string(DEFAULT_LOG_REGEX_PATTERN));
    ASSERT_TRUE(result1.has_value());
    ASSERT_EQ(analyzer.getEntries().size(), 2);
    // Check if sorted by timestamp
    ASSERT_EQ(analyzer.getEntries()[0].message, "Entry 1");
    ASSERT_EQ(analyzer.getEntries()[1].message, "Entry 3");
    std::remove(filePath1.c_str());
    
    // 2. Append more entries, some of which should be interleaved
    std::string logContent2 =
        "2023-01-01 10:00:01 DEBUG Entry 2\n"
        "2023-01-01 10:00:03 ERROR Entry 4\n";
    std::string filePath2 = "test_append_2.log";
    std::ofstream ofs2(filePath2);
    ofs2 << logContent2;
    ofs2.close();

    auto result2 = analyzer.append(filePath2, std::string(DEFAULT_LOG_REGEX_PATTERN));
    ASSERT_TRUE(result2.has_value());
    ASSERT_EQ(analyzer.getEntries().size(), 4);
    
    // Verify sorted order
    const auto& entries = analyzer.getEntries();
    ASSERT_EQ(entries[0].message, "Entry 1"); // 10:00:00
    ASSERT_EQ(entries[1].message, "Entry 2"); // 10:00:01
    ASSERT_EQ(entries[2].message, "Entry 3"); // 10:00:02
    ASSERT_EQ(entries[3].message, "Entry 4"); // 10:00:03

    std::remove(filePath2.c_str());
    analyzer.clear();

    // 3. Append a file that fails to open
    auto result3 = analyzer.append("non_existent_append.log", std::string(DEFAULT_LOG_REGEX_PATTERN));
    ASSERT_FALSE(result3.has_value());
    ASSERT_EQ(result3.error().code, ParseError::FILE_OPEN_FAILED);
    ASSERT_TRUE(analyzer.getEntries().empty()); // No entries should have been added
}


// Placeholder for concurrency tests. These are more complex and would involve
// multiple threads accessing the analyzer simultaneously.
// For example:
// - Multiple threads calling loadAndReplace/append
// - Multiple threads calling getFilteredEntries/getFrequencyDistribution
// - Mix of read/write operations
TEST_F(LogAnalyzerTest, Concurrency_Placeholder) {
    // This test would involve creating multiple threads and asserting that
    // the LogAnalyzer state remains consistent and no data races occur.
    // This often requires more advanced testing frameworks or explicit thread management.
    // For now, it's a placeholder.
    SUCCEED(); // Mark as successful for now
}
