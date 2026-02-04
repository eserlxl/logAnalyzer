#ifndef LOG_ANALYZER_H
#define LOG_ANALYZER_H

#include "LogTypes.h"
#include "Filter.h" 
#include "Error.h"
#include "CLIConfig.h"
#include "Statistics.h"
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

// New struct to encapsulate formatting options
struct FormattingOptions {
    std::string dateTimeFormat = "%Y-%m-%d %H:%M:%S";
    bool useColor = false;
    bool includeStructuredFields = true;
    std::string structuredFieldDelimiter = ", ";
    std::string structuredFieldKvDelimiter = "=";
};

// using namespace ErrorCode; // Removed due to namespace pollution

#include "LogParser.h"
#include "LogAnalyzerConfig.h"

class LogAnalyzer {
public:
    LogAnalyzer();
    ~LogAnalyzer(); // Declare destructor to wait for async tasks
    explicit LogAnalyzer(const LogAnalyzerSettings& settings);
    Result<void> setSettings(const LogAnalyzerSettings& settings);
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

    [[deprecated("Use analyzeStream(const std::vector<std::string>&, std::function<bool(const LogEntry&)>, CLIConfig::ParserAction) instead.")]]
    std::expected<void, LogParseError> analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, const std::string& pattern);
    
    Result<AnalysisReport> append(const std::string& filePath, CLIConfig::ParserErrorAction errorAction);

    [[deprecated("Use append(const std::string&, CLIConfig::ParserErrorAction) instead.")]]
    std::expected<void, LogParseError> append(const std::string& filePath, const std::string& pattern);
    
    std::span<const LogEntry> getEntriesView() const;
    void exportAsCsv(std::ostream& out, const FilterCriteria& filter, char delimiter = ',', std::string_view timestampFormat = "%Y-%m-%dT%H:%M:%S.%fZ") const;
    void exportAsJson(std::ostream& out, const FilterCriteria& filter, bool prettyPrint, std::string_view timestampFormat = "%Y-%m-%dT%H:%M:%S.%fZ") const;
    Result<std::vector<LogEntry>> getFilteredEntries(const FilterCriteria& criteria) const;
    std::vector<LogEntry> getSortedFilteredEntries(const FilterCriteria& criteria, SortBy sortBy, SortOrder sortOrder) const;

    // Formatting methods
    std::string formatEntry(const LogEntry& entry, std::string_view format, const FormattingOptions& options) const;
    std::string formatEntry(const LogEntry& entry, std::string_view format, bool useColor) const;

    // Printing methods
    void printFilteredEntries(std::ostream& out, const FilterCriteria& criteria, const FormattingOptions& options) const;
    void printFilteredEntries(std::ostream& out, const FilterCriteria& criteria, std::string_view formatString) const;

    // Statistics Refactoring
    void addStatisticCollector(std::shared_ptr<IStatisticCollector> collector);
    void processEntryForStatistics(const LogEntry& entry);
    std::map<std::string, json> getAllStatisticReports() const;


private:
    // Mutex for protecting general state (entries_, levelCounts, lastReport, currentSettings_, currentParser_, collectors_)
    mutable std::shared_mutex stateMutex_;
    // Mutex for protecting customLogLevelMapping_
    mutable std::shared_mutex customLogLevelMappingMutex_; 

    std::vector<LogEntry> entries_;
    std::map<LogLevel, size_t> levelCounts;
    AnalysisReport lastReport;
    LogAnalyzerSettings currentSettings_;
    std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> customLogLevelMapping_; // New member for custom log level mapping
    std::unique_ptr<ILogParser> currentParser_;
    std::vector<std::shared_ptr<IStatisticCollector>> collectors_;

    // For lifetime management of async tasks
    std::vector<std::future<void>> pendingAsyncTasks_;
    std::mutex pendingAsyncTasksMutex_;

    Result<std::vector<LogEntry>> getFilteredEntries_NoLock(const FilterCriteria& criteria) const;

    std::pair<std::vector<LogEntry>, AnalysisReport> parseAndReport(std::istream& is, const std::string& sourceIdentifier, CLIConfig::ParserErrorAction errorAction);
    Result<void> parseStreamInternal(std::istream& is, const std::string& sourceIdentifier, CLIConfig::ParserErrorAction errorAction, bool replaceExisting);
};

#endif // LOG_ANALYZER_H
