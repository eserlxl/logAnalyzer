#ifndef LOG_ANALYZER_H
#define LOG_ANALYZER_H

#include "LogTypes.h"
#include "Filter.h" 
// #include "LogFileView.h" // This file does not exist
#include <vector>
#include <string>
#include <map>
#include <optional>
#include <chrono>
#include <memory>
#include <future>
#include <functional>
#include <mutex>
#include <expected>
#include <span>
#include <iosfwd>

#include "LogParser.h"
// class LogFileView; // Forward-declare LogFileView

class LogAnalyzer {
public:
    static constexpr std::string_view DEFAULT_LOG_REGEX_PATTERN = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) ([A-Z]+): (.*)$)";
    LogAnalyzer();
    void clear();
    AnalysisReport loadAndReplace(const std::string& filePath, const std::string& pattern);
    const std::vector<LogEntry>& getEntries() const;
    void setCustomLogLevelMapping(std::string_view levelString, LogLevel mappedLevel);
    std::expected<void, LogParseError> load(const std::string& filePath, const std::string& pattern);
    std::future<AnalysisReport> loadAsync(const std::string& filePath, const std::string& pattern);
    std::expected<void, LogParseError> analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, const std::string& pattern);
    std::vector<TimeWindowStats> getFrequencyDistribution(std::chrono::seconds windowSize) const;
    std::expected<std::vector<LogEntry>, LogParseError> getFilteredEntries(const FilterCriteria& criteria) const;
    std::string formatTimestamp(std::chrono::system_clock::time_point tp, std::string_view format = "%Y-%m-%d %H:%M:%S") const;
    std::string formatEntry(const LogEntry& entry, std::string_view format, bool useColor = false) const;
    void printFilteredEntries(std::ostream& out, const FilterCriteria& criteria, std::string_view formatString) const;
    std::expected<void, LogParseError> append(const std::string& filePath, const std::string& pattern);
    std::span<const LogEntry> getEntriesView() const;
    void exportAsCsv(std::ostream& out, const FilterCriteria& filter, char delimiter = ',') const;
    std::vector<TimeGap> findTimeGaps(std::chrono::milliseconds minGapDuration) const;
    double getAverageEntryRate() const;
    std::string logLevelToString(LogLevel level) const;
    LogLevel stringToLogLevel(const std::string& levelStr);
    void exportAsJson(std::ostream& out, const FilterCriteria& filter, bool includeSummary, bool prettyPrint) const;
    std::vector<LogEntry> getSortedFilteredEntries(const FilterCriteria& criteria, SortBy sortBy, SortOrder sortOrder) const;
    std::map<std::string, int> getUniqueMessageCounts() const;
    std::vector<std::pair<std::string, int>> getTopMessages(int n) const;

private:
    mutable std::mutex mutex_;
    std::vector<LogEntry> entries_;
    std::map<LogLevel, size_t> levelCounts;
    AnalysisReport lastReport;
    std::unique_ptr<ILogParser> defaultParser_;
    std::map<std::string, LogLevel, ci_less> customLevelMappings;
    // std::unique_ptr<LogFileView> logSourceView_;

    std::expected<std::vector<LogEntry>, LogParseError> getFilteredEntries_NoLock(const FilterCriteria& criteria) const;
    std::map<std::string, int> getUniqueMessageCounts_NoLock() const;
};

#endif // LOG_ANALYZER_H
