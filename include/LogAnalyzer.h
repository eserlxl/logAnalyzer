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
#include "LogAnalyzerConfig.h"
// class LogFileView; // Forward-declare LogFileView

class LogAnalyzer {
public:
    // static constexpr std::string_view DEFAULT_LOG_REGEX_PATTERN = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) ([A-Z]+): (.*)$)"; // Moved to LogAnalyzerConfig.h
    LogAnalyzer();
    explicit LogAnalyzer(LogAnalyzerSettings settings);
    std::expected<void, LogParseError> setSettings(LogAnalyzerSettings settings);
    const LogAnalyzerSettings& getSettings() const;
    void clear();
    // Original loadAndReplace will be updated to use currentSettings_ and return AnalysisReport directly.
    // Its signature is already compatible with the return type change.
    AnalysisReport loadAndReplace(const std::string& filePath);

    [[deprecated("Use loadAndReplace(const std::string& filePath) instead. The pattern is now configured via LogAnalyzerSettings.")]]
    AnalysisReport loadAndReplace(const std::string& filePath, const std::string& pattern);
    const std::vector<LogEntry>& getEntries() const;
    void setCustomLogLevelMapping(std::string_view levelString, LogLevel mappedLevel);
    // New: Load from a file using the analyzer's current settings.
    // Returns a comprehensive report on parsing results.
    std::expected<AnalysisReport, LogParseError> load(const std::string& filePath);

    // Original load (deprecated)
    [[deprecated("Use load(const std::string& filePath) instead. The pattern is now configured via LogAnalyzerSettings.")]]
    std::expected<void, LogParseError> load(const std::string& filePath, const std::string& pattern);
    // New: Asynchronous load using current settings.
    std::future<std::expected<AnalysisReport, LogParseError>> loadAsync(const std::string& filePath);

    // Original loadAsync (deprecated)
    [[deprecated("Use loadAsync(const std::string& filePath) instead. The pattern is now configured via LogAnalyzerSettings.")]]
    std::future<AnalysisReport> loadAsync(const std::string& filePath, const std::string& pattern);
    // New: Stream in log entries from an istream using current settings, adding them to internal entries_.
    // This allows processing of non-file inputs (e.g., stdin, network streams) and leverages multi-line parsing.
    std::expected<AnalysisReport, LogParseError> streamIn(std::istream& is, const std::string& sourceIdentifier = "stream");

    // Existing analyzeStream (no change, as it's callback-based and doesn't affect internal state)
        [[deprecated("Use analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback) instead. The pattern is now configured via LogAnalyzerSettings.")]]
    std::expected<void, LogParseError> analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, const std::string& pattern);
    // New: analyzeStream overload using current settings.
    std::expected<void, LogParseError> analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback);

    // ... existing public members ...
    std::vector<TimeWindowStats> getFrequencyDistribution(std::chrono::seconds windowSize) const;
    std::expected<std::vector<LogEntry>, LogParseError> getFilteredEntries(const FilterCriteria& criteria) const;
    std::vector<LogEntry> getFilteredEntries(std::function<bool(const LogEntry&)> predicate) const;

    std::string formatTimestamp(std::chrono::system_clock::time_point tp, std::string_view format = "%Y-%m-%d %H:%M:%S") const;
    // New: Structure for advanced formatting options (e.g., color, timezone, structured field access)
    struct FormattingOptions {
        bool useColor = false;
        std::string dateTimeFormat = "%Y-%m-%d %H:%M:%S";
        bool includeStructuredFields = false; // Whether to append structured fields
        std::string structuredFieldDelimiter = "; "; // Delimiter for structured fields
        std::string structuredFieldKvDelimiter = "="; // Key-value delimiter for structured fields
    };

    // Extended formatEntry with FormattingOptions for richer output customization.
    // The format string can now support placeholders for structured fields, e.g., "{key.subkey}".
    std::string formatEntry(const LogEntry& entry, std::string_view format, const FormattingOptions& options) const;

    // Original formatEntry (deprecated)
    [[deprecated("Use formatEntry(const LogEntry& entry, std::string_view format, const FormattingOptions& options) instead.")]]
    std::string formatEntry(const LogEntry& entry, std::string_view format, bool useColor = false) const;
    // New: printFilteredEntries overload using FormattingOptions.
    void printFilteredEntries(std::ostream& out, const FilterCriteria& criteria, const FormattingOptions& options) const;

    // Original printFilteredEntries (deprecated)
    [[deprecated("Use printFilteredEntries(std::ostream& out, const FilterCriteria& criteria, const FormattingOptions& options) instead.")]]
    void printFilteredEntries(std::ostream& out, const FilterCriteria& criteria, std::string_view formatString) const;
    // New: Append from a file using the analyzer's current settings.
    // Returns a comprehensive report on parsing results.
    std::expected<AnalysisReport, LogParseError> append(const std::string& filePath);

    // Original append (deprecated)
    [[deprecated("Use append(const std::string& filePath) instead. The pattern is now configured via LogAnalyzerSettings.")]]
    std::expected<void, LogParseError> append(const std::string& filePath, const std::string& pattern);
    std::span<const LogEntry> getEntriesView() const;
    void exportAsCsv(std::ostream& out, const FilterCriteria& filter, char delimiter = ',', std::string_view timestampFormat = "%Y-%m-%dT%H:%M:%S.%fZ") const;
    std::vector<TimeGap> findTimeGaps(std::chrono::milliseconds minGapDuration) const;
    double getAverageEntryRate() const;
    std::string logLevelToString(LogLevel level) const;
    LogLevel stringToLogLevel(const std::string& levelStr);
    void exportAsJson(std::ostream& out, const FilterCriteria& filter, bool prettyPrint, std::string_view timestampFormat = "%Y-%m-%dT%H:%M:%S.%fZ") const;
    std::vector<LogEntry> getSortedFilteredEntries(const FilterCriteria& criteria, SortBy sortBy, SortOrder sortOrder) const;
    std::map<std::string, int> getUniqueMessageCounts() const;
    std::vector<std::pair<std::string, int>> getTopMessages(int n) const;

private:
    mutable std::mutex mutex_;
    std::vector<LogEntry> entries_;
    std::map<LogLevel, size_t> levelCounts;
    AnalysisReport lastReport;
    LogAnalyzerSettings currentSettings_; // New member to store current settings
    std::unique_ptr<ILogParser> currentParser_; // Renamed from defaultParser_ for clarity
    std::map<std::string, LogLevel, ci_less> customLevelMappings;
    // std::unique_ptr<LogFileView> logSourceView_;

    std::expected<std::vector<LogEntry>, LogParseError> getFilteredEntries_NoLock(const FilterCriteria& criteria) const;
    std::map<std::string, int> getUniqueMessageCounts_NoLock() const;

    // Internal helper for core parsing logic, to be used by all public load/append/streamIn methods.
    // This helper will handle the multi-line parsing logic via ILogParser::processLine and flushRemaining.
    // Returns a vector of all parsed entries and an AnalysisReport.
    std::pair<std::vector<LogEntry>, AnalysisReport> parseAndReport(std::istream& is, const std::string& sourceIdentifier);
};

#endif // LOG_ANALYZER_H
