#ifndef LOG_ANALYZER_H
#define LOG_ANALYZER_H

#include "LogTypes.h"
#include "Filter.h" 
#include "Error.h"
#include "CLIConfig.h"
#include "Statistics.h"
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

using namespace ErrorCode;

#include "LogParser.h"
#include "LogAnalyzerConfig.h"

class LogAnalyzer {
public:
    LogAnalyzer();
    explicit LogAnalyzer(LogAnalyzerSettings settings);
    Result<void> setSettings(LogAnalyzerSettings settings);
    const LogAnalyzerSettings& getSettings() const;
    void clear();
    Result<AnalysisReport> loadAndReplace(const std::string& filePath, CLIConfig::ParserErrorAction errorAction);

    [[deprecated("Use loadAndReplace(const std::string& filePath, CLIConfig::ParserErrorAction) instead.")]]
    AnalysisReport loadAndReplace(const std::string& filePath, const std::string& pattern);
    
    const std::vector<LogEntry>& getEntries() const;
    void setCustomLogLevelMapping(std::string_view levelString, LogLevel mappedLevel);
    
    Result<AnalysisReport> load(const std::string& filePath, CLIConfig::ParserErrorAction errorAction);

    [[deprecated("Use load(const std::string& filePath, CLIConfig::ParserErrorAction) instead.")]]
    std::expected<void, LogParseError> load(const std::string& filePath, const std::string& pattern);
    
    std::future<Result<AnalysisReport>> loadAsync(const std::string& filePath, CLIConfig::ParserErrorAction errorAction);

    [[deprecated("Use loadAsync(const std::string& filePath, CLIConfig::ParserErrorAction) instead.")]]
    std::future<AnalysisReport> loadAsync(const std::string& filePath, const std::string& pattern);
    
    Result<AnalysisReport> streamIn(std::istream& is, const std::string& sourceIdentifier, CLIConfig::ParserErrorAction errorAction);
    
    Result<void> analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, CLIConfig::ParserErrorAction errorAction);

    [[deprecated("Use analyzeStream(const std::vector<std::string>&, std::function<bool(const LogEntry&)>, CLIConfig::ParserErrorAction) instead.")]]
    std::expected<void, LogParseError> analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, const std::string& pattern);
    
    Result<AnalysisReport> append(const std::string& filePath, CLIConfig::ParserErrorAction errorAction);

    [[deprecated("Use append(const std::string&, CLIConfig::ParserErrorAction) instead.")]]
    std::expected<void, LogParseError> append(const std::string& filePath, const std::string& pattern);
    
    std::span<const LogEntry> getEntriesView() const;
    void exportAsCsv(std::ostream& out, const FilterCriteria& filter, char delimiter = ',', std::string_view timestampFormat = "%Y-%m-%dT%H:%M:%S.%fZ") const;
    void exportAsJson(std::ostream& out, const FilterCriteria& filter, bool prettyPrint, std::string_view timestampFormat = "%Y-%m-%dT%H:%M:%S.%fZ") const;
    std::vector<LogEntry> getSortedFilteredEntries(const FilterCriteria& criteria, SortBy sortBy, SortOrder sortOrder) const;

    // Statistics Refactoring
    void addStatisticCollector(std::shared_ptr<IStatisticCollector> collector);
    void processEntryForStatistics(const LogEntry& entry);
    std::map<std::string, json> getAllStatisticReports() const;


private:
    mutable std::mutex mutex_;
    std::vector<LogEntry> entries_;
    std::map<LogLevel, size_t> levelCounts;
    AnalysisReport lastReport;
    LogAnalyzerSettings currentSettings_;
    std::unique_ptr<ILogParser> currentParser_;
    std::vector<std::shared_ptr<IStatisticCollector>> _collectors;

    Result<std::vector<LogEntry>> getFilteredEntries_NoLock(const FilterCriteria& criteria) const;

    std::pair<std::vector<LogEntry>, AnalysisReport> parseAndReport(std::istream& is, const std::string& sourceIdentifier, CLIConfig::ParserErrorAction errorAction);
};

#endif // LOG_ANALYZER_H
