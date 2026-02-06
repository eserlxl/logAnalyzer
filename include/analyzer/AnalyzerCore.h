#ifndef LOG_ANALYZER_H
#define LOG_ANALYZER_H

#include "core/LogTypes.h"
#include "filter/Core.h"
#include "core/Error.h"
#include "config/CLIConfig.h"
#include "stats/Statistics.h"
#include "analyzer/LogReader.h"
#include "analyzer/LogWriter.h"
#include "analyzer/Types.h" // Added for FormattingOptions
#include <shared_mutex>
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

// Forward declarations
class LogReader;
class LogWriter;

#include "core/LogParser.h"
#include "config/ConfigCore.h"

class LogAnalyzer {
    friend class LogReader;
    friend class LogWriter;

public:
    LogAnalyzer();
    ~LogAnalyzer();
    explicit LogAnalyzer(const LogAnalyzerSettings& settings);
    ErrorCode::Result<void> setSettings(const LogAnalyzerSettings& settings);
    const LogAnalyzerSettings& getSettings() const;
    void clear();

    ErrorCode::Result<AnalysisReport> loadAndReplace(const std::string& filePath, CLIConfig::ParserErrorAction errorAction);
    [[deprecated("Use loadAndReplace(const std::string& filePath, CLIConfig::ParserErrorAction) instead.")]]
    ErrorCode::Result<AnalysisReport> loadAndReplace(const std::string& filePath, const std::string& pattern);
    
    ErrorCode::Result<AnalysisReport> load(const std::string& filePath, CLIConfig::ParserErrorAction errorAction);
    [[deprecated("Use load(const std::string& filePath, CLIConfig::ParserErrorAction) instead.")]]
    std::expected<void, LogParseError> load(const std::string& filePath, const std::string& pattern);
    
    std::future<ErrorCode::Result<AnalysisReport>> loadAsync(const std::string& filePath, CLIConfig::ParserErrorAction errorAction);
    [[deprecated("Use loadAsync(const std::string& filePath, CLIConfig::ParserErrorAction) instead.")]]
    std::future<ErrorCode::Result<AnalysisReport>> loadAsync(const std::string& filePath, const std::string& pattern);

    ErrorCode::Result<AnalysisReport> append(const std::string& filePath, CLIConfig::ParserErrorAction errorAction);
    [[deprecated("Use append(const std::string&, CLIConfig::ParserErrorAction) instead.")]]
    std::expected<void, LogParseError> append(const std::string& filePath, const std::string& pattern);
    
    ErrorCode::Result<AnalysisReport> streamIn(std::istream& is, const std::string& sourceIdentifier, CLIConfig::ParserErrorAction errorAction);
    ErrorCode::Result<void> analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, CLIConfig::ParserErrorAction errorAction);
    [[deprecated("Use analyzeStream(const std::vector<std::string>&, std::function<bool(const LogEntry&)>, CLIConfig::ParserAction) instead.")]]
    std::expected<void, LogParseError> analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, const std::string& pattern);
    
    std::string formatEntry(const LogEntry& entry, std::string_view format, const FormattingOptions& options) const;
    std::string formatEntry(const LogEntry& entry, std::string_view format, bool useColor) const;

    [[deprecated("Use printFilteredEntries(std::ostream&, const FilterExpression&, const FormattingOptions&) instead.")]]
    void printFilteredEntries(std::ostream& out, const FilterCriteria& criteria, const FormattingOptions& options) const;
    void printFilteredEntries(std::ostream& out, const FilterExpression& expression, const FormattingOptions& options) const;

    [[deprecated("Use printFilteredEntries(std::ostream&, const FilterExpression&, std::string_view) instead.")]]
    void printFilteredEntries(std::ostream& out, const FilterCriteria& criteria, std::string_view formatString) const;
    void printFilteredEntries(std::ostream& out, const FilterExpression& expression, std::string_view formatString) const;

    const std::vector<LogEntry>& getEntries() const;
    std::span<const LogEntry> getEntriesView() const;
    const AnalysisReport& getLastReport() const;
    
    void setCustomLogLevelMapping(std::string_view levelString, LogLevel mappedLevel);
    
    [[deprecated("Use exportAsCsv(std::ostream&, const FilterExpression&, char, std::string_view) instead.")]]
    void exportAsCsv(std::ostream& out, const FilterCriteria& filter, char delimiter = ',', std::string_view timestampFormat = "%Y-%m-%dT%H:%M:%S.%fZ") const;
    void exportAsCsv(std::ostream& out, const FilterExpression& expression, char delimiter = ',', std::string_view timestampFormat = "%Y-%m-%dT%H:%M:%S.%fZ") const;
    
    [[deprecated("Use exportAsJson(std::ostream&, const FilterExpression&, bool, std::string_view) instead.")]]
    void exportAsJson(std::ostream& out, const FilterCriteria& filter, bool prettyPrint, std::string_view timestampFormat = "%Y-%m-%dT%H:%M:%S.%fZ") const;
    void exportAsJson(std::ostream& out, const FilterExpression& expression, bool prettyPrint, std::string_view timestampFormat = "%Y-%m-%dT%H:%M:%S.%fZ") const;

    [[deprecated("Use getFilteredEntries(const FilterExpression&) instead.")]]
    ErrorCode::Result<std::vector<LogEntry>> getFilteredEntries(const FilterCriteria& criteria) const;
    ErrorCode::Result<std::vector<LogEntry>> getFilteredEntries(const FilterExpression& expression) const;

    [[deprecated("Use getSortedFilteredEntries(const FilterExpression&, SortBy, SortOrder) instead.")]]
    std::vector<LogEntry> getSortedFilteredEntries(const FilterCriteria& criteria, SortBy sortBy, SortOrder sortOrder) const;
    std::vector<LogEntry> getSortedFilteredEntries(const FilterExpression& expression, SortBy sortBy, SortOrder sortOrder) const;

    // Statistics Refactoring
    void addStatisticCollector(std::shared_ptr<IStatisticCollector> collector);
    void processEntryForStatistics(const LogEntry& entry);
    std::map<std::string, json> getAllStatisticReports() const;

    ILogParser* getCurrentParser() const { return currentParser_.get(); }

    // Helper methods used by IO.cpp
    void setDefaultFieldMappings(LogAnalyzerSettings& settings);
    std::pair<std::vector<LogEntry>, AnalysisReport> parseAndReport(std::istream& is, const std::string& sourceIdentifier, CLIConfig::ParserErrorAction errorAction);

private:
    mutable std::shared_mutex stateMutex_;
    mutable std::shared_mutex customLogLevelMappingMutex_;

    std::vector<LogEntry> entries_;
    std::map<LogLevel, size_t> levelCounts;
    AnalysisReport lastReport;
    LogAnalyzerSettings currentSettings_;
    std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> customLogLevelMapping_;
    std::unique_ptr<ILogParser> currentParser_;
    std::vector<std::shared_ptr<IStatisticCollector>> collectors_;

    std::vector<std::future<void>> pendingAsyncTasks_;
    std::mutex pendingAsyncTasksMutex_;

    ErrorCode::Result<std::vector<LogEntry>> getFilteredEntries_NoLock(const FilterCriteria& criteria) const;
    ErrorCode::Result<std::vector<LogEntry>> getFilteredEntries_NoLock(const FilterExpression& expression) const;
    
    static std::shared_ptr<IStatisticCollector> createStatisticCollector(const StatisticConfig& config);
    
    std::unique_ptr<LogReader> logReader_;
    std::unique_ptr<LogWriter> logWriter_;
};

#endif // LOG_ANALYZER_H
