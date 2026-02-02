#include "LogAnalyzer.h"
#include "gtest/gtest.h"
#include <chrono>
#include <fstream>
#include <sstream>
#include <thread> // For std::this_thread::sleep_for

// Helper function to create a dummy log file
void createDummyLogFile(const std::string &filename,
                        const std::vector<std::string> &lines) {
  std::ofstream ofs(filename);
  for (const auto &line : lines) {
    ofs << line << std::endl;
  }
  ofs.close();
}

// Test fixture for LogAnalyzer
class LogAnalyzerTest : public ::testing::Test {
protected:
  LogAnalyzer analyzer;
  const std::string testLogFile = "test.log";

  void SetUp() override {
    // Ensure the analyzer is clean before each test
    analyzer.clear();
  }

  void TearDown() override {
    // Clean up dummy log file after each test
    std::remove(testLogFile.c_str());
  }
};

TEST_F(LogAnalyzerTest, AnalyzeSuccess) {
  createDummyLogFile(testLogFile,
                     {"[2023-01-01 10:00:00] INFO: Application started",
                      "[2023-01-01 10:00:01] WARNING: Low disk space",
                      "[2023-01-01 10:00:02] ERROR: Failed to connect to DB"});

  AnalysisReport report = analyzer.analyze(testLogFile);
  ASSERT_EQ(report.status, ParseError::SUCCESS);
  ASSERT_EQ(report.linesProcessed, 3);
  ASSERT_EQ(report.successfulParses, 3);
  ASSERT_TRUE(report.parseErrors.empty());
  ASSERT_EQ(analyzer.getEntries().size(), 3);
}

TEST_F(LogAnalyzerTest, AnalyzeFileOpenFailed) {
  AnalysisReport report = analyzer.analyze("non_existent_file.log");
  ASSERT_EQ(report.status, ParseError::FILE_OPEN_FAILED);
  ASSERT_FALSE(report.parseErrors.empty());
}

TEST_F(LogAnalyzerTest, AnalyzeInvalidRegex) {
  createDummyLogFile(testLogFile,
                     {"[2023-01-01 10:00:00] INFO: Application started"});
  // Malformed regex pattern
  AnalysisReport report = analyzer.analyze(testLogFile, R"([)");
  ASSERT_EQ(report.status, ParseError::INVALID_REGEX_PATTERN);
  ASSERT_FALSE(report.parseErrors.empty());
}

TEST_F(LogAnalyzerTest, AnalyzePartialFailure) {
  createDummyLogFile(testLogFile,
                     {"[2023-01-01 10:00:00] INFO: Valid line",
                      "This is an invalid log line",
                      "[2023-01-01 10:00:01] ERROR: Another valid line"});
  AnalysisReport report = analyzer.analyze(testLogFile);
  ASSERT_EQ(report.status, ParseError::PARTIAL_FAILURE);
  ASSERT_EQ(report.linesProcessed, 3);
  ASSERT_EQ(report.successfulParses, 2);
  ASSERT_EQ(report.parseErrors.size(), 1);
  ASSERT_EQ(report.parseErrors[0].first, 2); // Line number of error

  // Check the entries collected.
  auto entries = analyzer.getEntries();
  ASSERT_EQ(entries.size(), 3);

  bool foundUnknown = false;
  bool foundInfo = false;
  bool foundError = false;
  for (const auto &entry : entries) {
    if (entry.level == LogLevel::UNKNOWN)
      foundUnknown = true;
    if (entry.level == LogLevel::INFO)
      foundInfo = true;
    if (entry.level == LogLevel::ERROR)
      foundError = true;
  }
  ASSERT_TRUE(foundUnknown);
  ASSERT_TRUE(foundInfo);
  ASSERT_TRUE(foundError);
}

TEST_F(LogAnalyzerTest, AnalyzePartialFailureAllInvalid) {
  createDummyLogFile(testLogFile,
                     {"This is an invalid log line", "Another invalid line"});
  AnalysisReport report = analyzer.analyze(testLogFile);
  ASSERT_EQ(report.status, ParseError::PARTIAL_FAILURE);
  ASSERT_EQ(report.linesProcessed, 2);
  ASSERT_EQ(report.successfulParses, 0);
  ASSERT_EQ(report.parseErrors.size(), 2);
  ASSERT_EQ(analyzer.getEntries().size(), 2); // Both are UNKNOWN
}

TEST_F(LogAnalyzerTest, AnalyzeStream) {
  createDummyLogFile(testLogFile,
                     {"[2023-01-01 10:00:00] INFO: Stream line 1",
                      "[2023-01-01 10:00:01] WARNING: Stream line 2",
                      "[2023-01-01 10:00:02] ERROR: Stream line 3"});

  std::vector<LogEntry> streamedEntries;
  int callbackCount = 0;
  analyzer.analyzeStream(testLogFile, [&](const LogEntry &entry) {
    streamedEntries.push_back(entry);
    callbackCount++;
    return true; // Continue processing
  });

  ASSERT_EQ(callbackCount, 3);
  ASSERT_EQ(streamedEntries.size(), 3);
  ASSERT_EQ(streamedEntries[0].level, LogLevel::INFO);
  ASSERT_EQ(streamedEntries[2].level, LogLevel::ERROR);
}

TEST_F(LogAnalyzerTest, AnalyzeStreamEarlyExit) {
  createDummyLogFile(testLogFile,
                     {"[2023-01-01 10:00:00] INFO: Stream line 1",
                      "[2023-01-01 10:00:01] WARNING: Stream line 2",
                      "[2023-01-01 10:00:02] ERROR: Stream line 3"});

  std::vector<LogEntry> streamedEntries;
  int callbackCount = 0;
  analyzer.analyzeStream(testLogFile, [&](const LogEntry &entry) {
    streamedEntries.push_back(entry);
    callbackCount++;
    return callbackCount < 2; // Stop after 2 entries
  });

  ASSERT_EQ(callbackCount, 2);
  ASSERT_EQ(streamedEntries.size(), 2);
  ASSERT_EQ(streamedEntries[0].level, LogLevel::INFO);
  ASSERT_EQ(streamedEntries[1].level, LogLevel::WARNING);
}

TEST_F(LogAnalyzerTest, GetFrequencyDistribution) {
  createDummyLogFile(testLogFile, {"[2023-01-01 10:00:00] INFO: Log 1",
                                   "[2023-01-01 10:00:05] WARNING: Log 2",
                                   "[2023-01-01 10:00:10] INFO: Log 3",
                                   "[2023-01-01 10:00:14] ERROR: Log 4",
                                   "[2023-01-01 10:00:18] INFO: Log 5",
                                   "[2023-01-01 10:00:20] DEBUG: Log 6"});
  analyzer.analyze(testLogFile);

  // Test with 10 second window
  auto distribution =
      analyzer.getFrequencyDistribution(std::chrono::seconds(10));

  ASSERT_EQ(distribution.size(), 3); // 0-10s, 10-20s, 20-30s

  // Window 1: 10:00:00 to 10:00:09
  // Expected: INFO: 2, WARNING: 1
  ASSERT_EQ(distribution[0].totalCount, 2);
  ASSERT_EQ(distribution[0].counts[LogLevel::INFO], 1);
  ASSERT_EQ(distribution[0].counts[LogLevel::WARNING], 1);
  ASSERT_EQ(distribution[0].counts[LogLevel::ERROR], 0);

  // Window 2: 10:00:10 to 10:00:19
  // Expected: INFO: 1, ERROR: 1
  ASSERT_EQ(distribution[1].totalCount, 3);
  ASSERT_EQ(distribution[1].counts[LogLevel::INFO], 2);
  ASSERT_EQ(distribution[1].counts[LogLevel::ERROR], 1);

  // Window 3: 10:00:20 to 10:00:29
  // Expected: DEBUG: 1
  ASSERT_EQ(distribution[2].totalCount, 1);
  ASSERT_EQ(distribution[2].counts[LogLevel::DEBUG], 1);
}

TEST_F(LogAnalyzerTest, GetFrequencyDistributionOptimized) {
  createDummyLogFile(testLogFile, {"[2023-01-01 10:00:00] INFO: Log 1",
                                   "[2023-01-01 10:00:05] WARNING: Log 2",
                                   "[2023-01-01 10:00:10] INFO: Log 3",
                                   "[2023-01-01 10:00:14] ERROR: Log 4",
                                   "[2023-01-01 10:00:18] INFO: Log 5",
                                   "[2023-01-01 10:00:20] DEBUG: Log 6"});
  analyzer.analyze(testLogFile);

  // Test with 10 second window
  auto distribution =
      analyzer.getFrequencyDistributionOptimized(std::chrono::seconds(10));

  ASSERT_EQ(distribution.size(), 3); // 0-10s, 10-20s, 20-30s

  // Window 1: Starts at 10:00:00
  ASSERT_EQ(distribution[0].totalCount, 2);
  ASSERT_EQ(distribution[0].counts[LogLevel::INFO], 1);
  ASSERT_EQ(distribution[0].counts[LogLevel::WARNING], 1);
  ASSERT_EQ(distribution[0].counts[LogLevel::ERROR], 0);

  // Window 2: Starts at 10:00:10
  ASSERT_EQ(distribution[1].totalCount, 3);
  ASSERT_EQ(distribution[1].counts[LogLevel::INFO], 2);
  ASSERT_EQ(distribution[1].counts[LogLevel::ERROR], 1);

  // Window 3: Starts at 10:00:20
  ASSERT_EQ(distribution[2].totalCount, 1);
  ASSERT_EQ(distribution[2].counts[LogLevel::DEBUG], 1);
}

TEST_F(LogAnalyzerTest, MergeAnalyzers) {
  createDummyLogFile("log1.log", {"[2023-01-01 10:00:00] INFO: From log1",
                                  "[2023-01-01 10:00:02] WARNING: From log1"});
  createDummyLogFile("log2.log", {"[2023-01-01 10:00:01] ERROR: From log2",
                                  "[2023-01-01 10:00:03] INFO: From log2"});

  LogAnalyzer analyzer1;
  analyzer1.analyze("log1.log");
  LogAnalyzer analyzer2;
  analyzer2.analyze("log2.log");

  analyzer1.merge(analyzer2);

  ASSERT_EQ(analyzer1.getEntries().size(), 4);
  // Check sorting after merge
  ASSERT_EQ(analyzer1.getEntries()[0].level, LogLevel::INFO);    // 10:00:00
  ASSERT_EQ(analyzer1.getEntries()[1].level, LogLevel::ERROR);   // 10:00:01
  ASSERT_EQ(analyzer1.getEntries()[2].level, LogLevel::WARNING); // 10:00:02
  ASSERT_EQ(analyzer1.getEntries()[3].level, LogLevel::INFO);    // 10:00:03

  std::remove("log1.log");
  std::remove("log2.log");
}

TEST_F(LogAnalyzerTest, FindFirst) {
  createDummyLogFile(testLogFile, {"[2023-01-01 10:00:00] INFO: Entry 1",
                                   "[2023-01-01 10:00:01] WARNING: Entry 2",
                                   "[2023-01-01 10:00:02] INFO: Entry 3",
                                   "[2023-01-01 10:00:03] ERROR: Entry 4"});
  analyzer.analyze(testLogFile);

  FilterCriteria criteria;
  criteria.levels = {LogLevel::INFO};
  auto found = analyzer.findFirst(criteria);
  ASSERT_TRUE(found.has_value());
  ASSERT_EQ(found.value().message, "Entry 1");

  criteria.levels = {LogLevel::DEBUG};
  found = analyzer.findFirst(criteria);
  ASSERT_FALSE(found.has_value());
}

TEST_F(LogAnalyzerTest, FindLast) {
  createDummyLogFile(testLogFile, {"[2023-01-01 10:00:00] INFO: Entry 1",
                                   "[2023-01-01 10:00:01] WARNING: Entry 2",
                                   "[2023-01-01 10:00:02] INFO: Entry 3",
                                   "[2023-01-01 10:00:03] ERROR: Entry 4"});
  analyzer.analyze(testLogFile);

  FilterCriteria criteria;
  criteria.levels = {LogLevel::INFO};
  auto found = analyzer.findLast(criteria);
  ASSERT_TRUE(found.has_value());
  ASSERT_EQ(found.value().message, "Entry 3");

  criteria.levels = {LogLevel::DEBUG};
  found = analyzer.findLast(criteria);
  ASSERT_FALSE(found.has_value());
}

TEST_F(LogAnalyzerTest, MinLogLevelFiltering) {
  createDummyLogFile(testLogFile,
                     {"[2023-01-01 10:00:00] DEBUG: Debug message",
                      "[2023-01-01 10:00:01] INFO: Info message",
                      "[2023-01-01 10:00:02] WARNING: Warning message",
                      "[2023-01-01 10:00:03] ERROR: Error message"});
  analyzer.analyze(testLogFile);

  FilterCriteria criteria;
  criteria.minLogLevel = LogLevel::WARNING;
  auto filtered = analyzer.getFilteredEntries(criteria);
  ASSERT_EQ(filtered.size(), 2);
  ASSERT_EQ(filtered[0].level, LogLevel::WARNING);
  ASSERT_EQ(filtered[1].level, LogLevel::ERROR);

  criteria.minLogLevel = LogLevel::INFO;
  filtered = analyzer.getFilteredEntries(criteria);
  ASSERT_EQ(filtered.size(), 3);
  ASSERT_EQ(filtered[0].level, LogLevel::INFO);
  ASSERT_EQ(filtered[1].level, LogLevel::WARNING);
  ASSERT_EQ(filtered[2].level, LogLevel::ERROR);

  // Test with minLogLevel and specific levels
  criteria.levels = {LogLevel::ERROR};
  filtered = analyzer.getFilteredEntries(criteria);
  ASSERT_EQ(filtered.size(), 1);
  ASSERT_EQ(filtered[0].level, LogLevel::ERROR);
}

TEST_F(LogAnalyzerTest, FormatTimestampCustom) {
  std::chrono::system_clock::time_point test_tp;
  std::tm tm = {};
  tm.tm_year = 2023 - 1900; // Year is years since 1900
  tm.tm_mon = 0;            // January (0-11)
  tm.tm_mday = 1;           // Day of the month
  tm.tm_hour = 10;
  tm.tm_min = 30;
  tm.tm_sec = 15;
  test_tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

  std::string formatted = analyzer.formatTimestamp(test_tp, "%Y/%m/%d %H:%M");
  ASSERT_EQ(formatted, "2023/01/01 10:30");

  formatted = analyzer.formatTimestamp(test_tp, "[%H:%M:%S]");
  ASSERT_EQ(formatted, "[10:30:15]");
}

TEST_F(LogAnalyzerTest, PrintFilteredEntriesCustomFormat) {
  createDummyLogFile(testLogFile,
                     {"[2023-01-01 10:00:00] INFO: Message One",
                      "[2023-01-01 10:00:01] WARNING: Message Two"});
  analyzer.analyze(testLogFile);

  FilterCriteria criteria;
  std::ostringstream oss;
  analyzer.printFilteredEntries(
      oss, criteria, "Level: {level}, Time: {timestamp}, Msg: {message}");
  std::string expectedOutput =
      "Level: INFO, Time: 2023-01-01 10:00:00, Msg: Message One\n"
      "Level: WARNING, Time: 2023-01-01 10:00:01, Msg: Message Two\n";
  ASSERT_EQ(oss.str(), expectedOutput);
}

// --- Iteration 1 Feature Tests ---

TEST_F(LogAnalyzerTest, LoadExpectedSuccess) {
  createDummyLogFile(testLogFile, {"[2023-01-01 10:00:00] INFO: Test"});
  auto result = analyzer.load(testLogFile);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(analyzer.getEntries().size(), 1);
  ASSERT_EQ(analyzer.getAnalysisReport().status, ParseError::SUCCESS);
}

TEST_F(LogAnalyzerTest, LoadExpectedFileFail) {
  auto result = analyzer.load("non_existent_file.log");
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error().code, ParseError::FILE_OPEN_FAILED);
  ASSERT_EQ(analyzer.getAnalysisReport().status, ParseError::FILE_OPEN_FAILED);
}

TEST_F(LogAnalyzerTest, AppendSuccess) {
  createDummyLogFile(testLogFile, {"[2023-01-01 10:00:01] INFO: First file"});
  analyzer.load(testLogFile);
  ASSERT_EQ(analyzer.getEntries().size(), 1);

  createDummyLogFile("log2.log", {"[2023-01-01 10:00:00] DEBUG: Second file"});
  auto result = analyzer.append("log2.log");
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(analyzer.getEntries().size(), 2);
  ASSERT_EQ(analyzer.getEntries()[0].level, LogLevel::DEBUG); // Check sorting
  ASSERT_EQ(analyzer.getEntries()[1].level, LogLevel::INFO);
  std::remove("log2.log");
}

TEST_F(LogAnalyzerTest, LoadAsync) {
  createDummyLogFile(testLogFile, {"[2023-01-01 10:00:00] INFO: Async test"});
  std::future<AnalysisReport> futureReport = analyzer.load_async(testLogFile);

  // You can do other work here...

  AnalysisReport report = futureReport.get(); // Wait for completion
  ASSERT_EQ(report.status, ParseError::SUCCESS);
  ASSERT_EQ(analyzer.getEntries().size(), 1);
  ASSERT_EQ(analyzer.getEntries()[0].message, "Async test");
}

TEST_F(LogAnalyzerTest, EntriesView) {
  createDummyLogFile(testLogFile, {"[2023-01-01 10:00:00] INFO: Entry 1",
                                   "[2023-01-01 10:00:01] WARNING: Entry 2"});
  analyzer.load(testLogFile);

  std::span<const LogEntry> view = analyzer.entries_view();
  ASSERT_EQ(view.size(), 2);
  ASSERT_EQ(view[0].message, "Entry 1");
  ASSERT_EQ(view[1].message, "Entry 2");
}

TEST_F(LogAnalyzerTest, CustomLogLevelMapping) {
  analyzer.setCustomLogLevelMapping("CRITICAL", LogLevel::ERROR);
  analyzer.setCustomLogLevelMapping("trace", LogLevel::DEBUG);

  createDummyLogFile(testLogFile,
                     {"[2023-01-01 10:00:00] CRITICAL: System failure",
                      "[2023-01-01 10:00:01] TRACE: function entered"});
  analyzer.load(testLogFile);

  ASSERT_EQ(analyzer.getEntries().size(), 2);
  ASSERT_EQ(analyzer.getEntries()[0].level, LogLevel::ERROR);
  ASSERT_EQ(analyzer.getEntries()[1].level, LogLevel::DEBUG);
}

TEST_F(LogAnalyzerTest, CsvExport) {
  createDummyLogFile(testLogFile,
                     {"[2023-01-01 10:00:00] INFO: Simple message",
                      "[2023-01-01 10:00:01] WARNING: Message with, a comma",
                      "[2023-01-01 10:00:02] ERROR: Message with \"quotes\""});
  analyzer.load(testLogFile);

  std::ostringstream oss;
  analyzer.exportAsCsv(oss);

  std::string expected =
      "Timestamp,Level,Message\n"
      "2023-01-01 10:00:00,INFO,Simple message\n"
      "2023-01-01 10:00:01,WARNING,\"Message with, a comma\"\n"
      "2023-01-01 10:00:02,ERROR,\"Message with \"\"quotes\"\"\"\n";

  ASSERT_EQ(oss.str(), expected);
}

TEST_F(LogAnalyzerTest, TimeGaps) {
  createDummyLogFile(testLogFile, {
                                      "[2023-01-01 10:00:00] INFO: A",
                                      "[2023-01-01 10:00:01] INFO: B", // 1s gap
                                      "[2023-01-01 10:00:05] INFO: C", // 4s gap
                                      "[2023-01-01 10:00:15] INFO: D" // 10s gap
                                  });
  analyzer.load(testLogFile);

  auto gaps = analyzer.findTimeGaps(std::chrono::seconds(2));
  ASSERT_EQ(gaps.size(), 2);
  ASSERT_EQ(std::chrono::duration_cast<std::chrono::seconds>(gaps[0].duration)
                .count(),
            4);
  ASSERT_EQ(std::chrono::duration_cast<std::chrono::seconds>(gaps[1].duration)
                .count(),
            10);
}

TEST_F(LogAnalyzerTest, AverageEntryRate) {
  createDummyLogFile(
      testLogFile,
      {
          "[2023-01-01 10:00:00] INFO: A", "[2023-01-01 10:00:01] INFO: B",
          "[2023-01-01 10:00:02] INFO: C",
          "[2023-01-01 10:00:10] INFO: D" // Total duration 10s, 4 entries
      });
  analyzer.load(testLogFile);

  // 4 entries over 10 seconds = 0.4 entries/sec
  ASSERT_NEAR(analyzer.getAverageEntryRate(), 0.4, 0.001);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
