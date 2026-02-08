// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef LOG_ANALYZER_H
#define LOG_ANALYZER_H

#include "core/Log/Types.h"
#include "filter/Expression.h"
#include "filter/Types.h"
#include "core/Error.h"
#include "core/CiLess.h"
#include <nlohmann/json_fwd.hpp>

#include <atomic>
#include <cstddef>
#include <future>
#include <generator> // C++23 for std::generator
#include <shared_mutex>
#include <vector>
#include <string>
#include <string_view>
#include <map>
#include <optional>
#include <chrono>
#include <memory>
#include <functional>
#include <mutex>
#include <expected>
#include <span>
#include <iosfwd>
#include <utility>


// Forward declarations
class LogReader;
class LogWriter;
class ILogParser;
class ILogParserFactory;
class IStatisticCollector;
struct StatisticConfig;

namespace filter {
struct FilterCriteria;
}

#include "config/Core.h"

// New: Define a CancellationToken structure
struct CancellationToken {
    std::atomic<bool> cancelled = false;
    void cancel() { cancelled = true; }
    bool isCancelled() const { return cancelled.load(); }
};

// New: Define a ProgressCallback function type
using ProgressCallback = std::function<void(double progressPercentage, std::string_view message)>;

// New: Define a "LogView" concept or class that represents a pipeline stage.
// This could be an alias for a range, or a wrapper around a range.
// For now, a simple alias to a range of const LogEntry references.
// This will likely evolve as the ranges pipeline is built.
// The actual return type of range-based functions will be more complex, e.g., auto or specific range adaptors.
// For declarations, we'll use a placeholder or specify the underlying type.
// For now, let's represent it as a generator for simplicity in public APIs where lazy evaluation is key.
// Internally, it can be std::ranges::views::filter | std::ranges::views::transform etc.

// New: A dedicated structure/enum for specifying field projection.
struct FieldProjection {
    std::vector<std::string> fieldsToInclude; // Or a map for renaming
};

// New: FormattingOptions for Text-based output
struct TextOutputFormat {
    std::string entryDelimiter = "\n";
    std::string fieldDelimiter = " | ";
    std::string timestampFormat = "%Y-%m-%dT%H:%M:%S";
    std::vector<std::string> fieldsToOutput; // Order and selection
    bool includeHeader = true;
};

class LogAnalyzer {
    friend class LogReader;
    friend class LogWriter;

public:
    LogAnalyzer();
    virtual ~LogAnalyzer();
    explicit LogAnalyzer(const LogAnalyzerSettings& settings);
    ErrorCode::Result<void> setSettings(const LogAnalyzerSettings& settings);
    const LogAnalyzerSettings& getSettings() const;
    void clear();

    // New: Register a custom parser factory for a given format identifier
    // This allows users to extend parsing capabilities without modifying the core.
    static ErrorCode::Result<void> registerParserFactory(
        std::string_view formatIdentifier, 
        std::shared_ptr<ILogParserFactory> factory
    );

    // New: Select a parser by its identifier for subsequent loading operations.
    ErrorCode::Result<void> selectParser(std::string_view formatIdentifier);

    // New: Get the currently active parser's format identifier
    std::string_view getSelectedParserIdentifier() const;

    ErrorCode::Result<AnalysisReport> loadAndReplace(
        const std::string& filePath, 
        ParserErrorAction errorAction,
        std::optional<CancellationToken*> cancellationToken = std::nullopt,
        std::optional<ProgressCallback> progressCallback = std::nullopt
    );
    [[deprecated("Use loadAndReplace(const std::string& filePath, ParserErrorAction, std::optional<CancellationToken*>, std::optional<ProgressCallback>) instead.")]]
    ErrorCode::Result<AnalysisReport> loadAndReplace(const std::string& filePath, const std::string& pattern);
    
    ErrorCode::Result<AnalysisReport> load(
        const std::string& filePath, 
        ParserErrorAction errorAction,
        std::optional<CancellationToken*> cancellationToken = std::nullopt,
        std::optional<ProgressCallback> progressCallback = std::nullopt
    );
    [[deprecated("Use load(const std::string& filePath, ParserErrorAction, std::optional<CancellationToken*>, std::optional<ProgressCallback>) instead.")]]
    std::expected<void, LogParseError> load(const std::string& filePath, const std::string& pattern);
    
    std::future<ErrorCode::Result<AnalysisReport>> loadAsync(
        const std::string& filePath, 
        ParserErrorAction errorAction,
        std::shared_ptr<CancellationToken> cancellationToken = nullptr, // shared_ptr for async ownership
        std::optional<ProgressCallback> progressCallback = std::nullopt
    );
    [[deprecated("Use loadAsync(const std::string& filePath, ParserErrorAction, std::shared_ptr<CancellationToken>, std::optional<ProgressCallback>) instead.")]]
    std::future<ErrorCode::Result<AnalysisReport>> loadAsync(const std::string& filePath, const std::string& pattern);

    ErrorCode::Result<AnalysisReport> append(
        const std::string& filePath, 
        ParserErrorAction errorAction,
        std::optional<CancellationToken*> cancellationToken = std::nullopt,
        std::optional<ProgressCallback> progressCallback = std::nullopt
    );
    [[deprecated("Use append(const std::string&, ParserErrorAction, std::optional<CancellationToken*>, std::optional<ProgressCallback>) instead.")]]
    std::expected<void, LogParseError> append(const std::string& filePath, const std::string& pattern);
    
    ErrorCode::Result<AnalysisReport> streamIn(
        std::istream& is, 
        const std::string& sourceIdentifier, 
        ParserErrorAction errorAction,
        std::optional<CancellationToken*> cancellationToken = std::nullopt,
        std::optional<ProgressCallback> progressCallback = std::nullopt
    );
    ErrorCode::Result<void> analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, ParserErrorAction errorAction);
    [[deprecated("Use analyzeStream(const std::vector<std::string>&, std::function<bool(const LogEntry&)>, ParserErrorAction) instead.")]]
    std::expected<void, LogParseError> analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, const std::string& pattern);
    
    // New: Stream filtered entries using std::generator
    std::generator<const LogEntry&> streamFilteredEntries(const filter::FilterExpression& expression) const;

    // New: Stream sorted and filtered entries using std::generator
    std::generator<const LogEntry&> streamSortedFilteredEntries(
        const filter::FilterExpression& expression, 
        filter::SortBy sortBy, 
        filter::SortOrder sortOrder
    ) const;

    // New: General-purpose streaming analysis pipeline
    std::generator<const LogEntry&> streamAnalyze(
        std::istream& is, 
        std::string_view sourceIdentifier,
        ParserErrorAction errorAction,
        std::optional<filter::FilterExpression> filter = std::nullopt,
        std::optional<std::function<LogEntry(LogEntry)>> transform = std::nullopt,
        std::optional<CancellationToken*> cancellationToken = std::nullopt,
        std::optional<ProgressCallback> progressCallback = std::nullopt
    ) const;

    const std::vector<LogEntry>& getEntries() const;
    std::vector<LogEntry> getEntriesSnapshot() const;
    std::span<const LogEntry> getEntriesView() const; // Renamed from getEntriesView() to avoid confusion
    const AnalysisReport& getLastReport() const;
    AnalysisReport getLastReportSnapshot() const;
    
    void setCustomLogLevelMapping(std::string_view levelString, LogLevel mappedLevel);
    
    [[deprecated("Use exportAsCsv(std::ostream&, const filter::FilterExpression&, char, std::string_view) instead.")]]
    void exportAsCsv(std::ostream& out, const filter::FilterCriteria& filter, char delimiter = ',', std::string_view timestampFormat = "%Y-%m-%dT%H:%M:%S.%fZ") const;
    void exportAsCsv(std::ostream& out, const filter::FilterExpression& expression, char delimiter = ',', std::string_view timestampFormat = "%Y-%m-%dT%H:%M:%S.%fZ") const;

    // New: Export to CSV from a projected view (e.g., from project() method)
    void exportAsCsv(
        std::ostream& out, 
        std::generator<nlohmann::json> projectedView,
        char delimiter = ',', 
        std::string_view timestampFormat = "%Y-%m-%dT%H:%M:%S.%fZ"
    ) const;
    
    [[deprecated("Use exportAsJson(std::ostream&, const filter::FilterExpression&, bool, std::string_view) instead.")]]
    void exportAsJson(std::ostream& out, const filter::FilterCriteria& filter, bool prettyPrint, std::string_view timestampFormat = "%Y-%m-%dT%H:%M:%S.%fZ") const;
    void exportAsJson(std::ostream& out, const filter::FilterExpression& expression, bool prettyPrint, std::string_view timestampFormat = "%Y-%m-%dT%H:%M:%S.%fZ") const;

    // New: Export to custom text format
    void exportAsText(
        std::ostream& out, 
        std::generator<const LogEntry&> filteredView, 
        const TextOutputFormat& format
    ) const;

    [[deprecated("Use getFilteredEntries(const filter::FilterExpression&) instead.")]]
    virtual ErrorCode::Result<std::vector<LogEntry>> getFilteredEntries(const filter::FilterCriteria& criteria) const;
    virtual ErrorCode::Result<std::vector<LogEntry>> getFilteredEntries(const filter::FilterExpression& expression) const;

    [[deprecated("Use getSortedFilteredEntries(const filter::FilterExpression&, filter::SortBy, filter::SortOrder) instead.")]]
    std::vector<LogEntry> getSortedFilteredEntries(const filter::FilterCriteria& criteria, filter::SortBy sortBy, filter::SortOrder sortOrder) const;
    std::vector<LogEntry> getSortedFilteredEntries(const filter::FilterExpression& expression, filter::SortBy sortBy, filter::SortOrder sortOrder) const;

    // New: Apply a filter, returning a new view (generator)
    std::generator<const LogEntry&> filter(std::generator<const LogEntry&> input, const filter::FilterExpression& expression) const;

    // New: Apply a transformation, returning a new view (generator)
    std::generator<LogEntry> transform(std::generator<const LogEntry&> input, std::function<LogEntry(LogEntry)> transformer) const; 
    
    // New: Apply a projection (select specific fields), returning a new view of a simplified structure
    std::generator<nlohmann::json> project(std::generator<const LogEntry&> input, const FieldProjection& projection) const;

    // Statistics Refactoring
    void addStatisticCollector(std::shared_ptr<IStatisticCollector> collector);
    void processEntryForStatistics(const LogEntry& entry);
    // New: Remove a specific statistic collector
    void removeStatisticCollector(const std::shared_ptr<IStatisticCollector>& collector);
    // New: Clear all statistic collectors
    void clearStatisticCollectors();
    // New: Reset all active collectors (clear their accumulated data)
    void resetStatisticCollectors();
    // New: Process a range of entries for statistics
    void processEntriesForStatistics(std::span<const LogEntry> entries); 
    void processEntriesForStatistics(std::generator<const LogEntry&> entries);
    std::map<std::string, nlohmann::json> getAllStatisticReports() const;

    ILogParser* getCurrentParser() const { return currentParser_.get(); }

    // Helper methods used by IO.cpp
    void setDefaultFieldMappings(LogAnalyzerSettings& settings);
    std::pair<std::vector<LogEntry>, AnalysisReport> parseAndReport(
        std::istream& is, 
        const std::string& sourceIdentifier, 
        ParserErrorAction errorAction,
        std::optional<CancellationToken*> cancellationToken = std::nullopt,
        std::optional<ProgressCallback> progressCallback = std::nullopt
    );
    virtual filter::FilterExpression createFilterExpressionFromCriteria(const filter::FilterCriteria& criteria) const;

private:
    // Internal helper methods that generators would call
    std::generator<const LogEntry&> filterEntriesInternal(const filter::FilterExpression& expression) const;
    std::generator<const LogEntry&> sortEntriesInternal(std::generator<const LogEntry&> input, filter::SortBy sortBy, filter::SortOrder sortOrder) const;

    // Internal (re)factory method for currentParser_ based on currentParserIdentifier_ and settings
    void updateCurrentParser();

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

    // Internal map to store registered parser factories
    static std::map<std::string, std::shared_ptr<ILogParserFactory>, LogAnalyzerInternal::ci_less> s_parserFactories_;
    // The currently active parser factory identifier
    std::string currentParserIdentifier_;

    ErrorCode::Result<std::vector<LogEntry>> getFilteredEntries_NoLock(const filter::FilterCriteria& criteria) const;
    ErrorCode::Result<std::vector<LogEntry>> getFilteredEntries_NoLock(const filter::FilterExpression& expression) const;
    
    static std::shared_ptr<IStatisticCollector> createStatisticCollector(const StatisticConfig& config);
    
    std::unique_ptr<LogReader> logReader_;
    std::unique_ptr<LogWriter> logWriter_;
};

#endif // LOG_ANALYZER_H
