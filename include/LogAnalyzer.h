#ifndef LOG_ANALYZER_H
#define LOG_ANALYZER_H

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <chrono>   // For std::chrono::system_clock::time_point
#include <optional> // For std::optional
#include <regex>    // For regexPattern in FilterCriteria
#include <iomanip>  // For timestamp formatting
#include <expected> // For std::expected (C++23)
#include <functional> // For std::function
#include <string_view> // For std::string_view (C++17, but emphasized for C++20/23 use)
#include <ranges> // For std::ranges (C++20)
#include <span>   // For std::span (C++20)
#include <future> // For std::future

enum class LogLevel {
    INFO,
    WARNING,
    ERROR,
    DEBUG,
    UNKNOWN
};

// New Enum for Parse Errors
enum class ParseError {
    SUCCESS,
    FILE_OPEN_FAILED,
    INVALID_REGEX_PATTERN,
    PARTIAL_FAILURE // Some lines failed to parse but others succeeded
};

// New type for robust error handling (Iteration 1 Feature)
struct LogParseError {
    ParseError code = ParseError::SUCCESS;
    std::string message;
    size_t lineNumber = 0;
};

// New struct for comprehensive analysis results (Iteration 1 Feature)
struct AnalysisReport {
    size_t linesProcessed = 0;
    size_t successfulParses = 0;
    std::vector<std::pair<size_t, std::string>> parseErrors; // line number -> error reason
    ParseError status = ParseError::SUCCESS;
};

// New struct for time-gap analysis (Iteration 1 Feature)
struct TimeGap {
    std::chrono::system_clock::time_point start;
    std::chrono::system_clock::time_point end;
    std::chrono::system_clock::duration duration;
};

// New struct for time-windowed statistics (Iteration 1 Feature)
struct TimeWindowStats {
    std::chrono::system_clock::time_point windowStart;
    std::map<LogLevel, int> counts;
    int totalCount;
};

// LogEntry structure enhancement
struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string message;
};

// FilterCriteria structure expansion
struct FilterCriteria {
    std::vector<LogLevel> levels;      // Multiple levels (empty means all)
    std::optional<LogLevel> minLogLevel; // Filter for this level and above
    std::string keyword;               // Case-insensitive substring match (kept for compatibility)
    std::string regexPattern;          // Optional: full regex pattern for message content. If provided, overrides 'keyword'.
    bool keywordCaseSensitive = false; // For 'keyword' field if used.
    
    std::optional<std::chrono::system_clock::time_point> startTime; // Optional start timestamp
    std::optional<std::chrono::system_clock::time_point> endTime;   // Optional end timestamp
};

// Sorting Functionality enums
enum class SortBy {
    TIMESTAMP,
    LEVEL,
    MESSAGE
};

enum class SortOrder {
    ASCENDING,
    DESCENDING
};

class LogAnalyzer {
public:
    LogAnalyzer();

    // New/Modernized Loading APIs (Iteration 1)
    std::expected<void, LogParseError> load(const std::string& filePath, const std::string& pattern = "");
    std::expected<void, LogParseError> append(const std::string& filePath, const std::string& pattern = "");
    std::future<AnalysisReport> load_async(const std::string& filePath, const std::string& pattern = "");
    
    // Backward compatibility: analyze now calls load
    AnalysisReport analyze(const std::string& filePath, const std::string& pattern = "");
    
    // Function signature for streaming
    void analyzeStream(
        const std::string& filePath, 
        std::function<bool(const LogEntry&)> entryCallback,
        const std::string& pattern = ""
    );

    void printSummary(std::ostream& out = std::cout) const;

    // View-based access (Iteration 1 Feature)
    std::span<const LogEntry> entries_view() const;

    // Existing API Extensions
    const std::vector<LogEntry>& getEntries() const;
    std::vector<LogEntry> getFilteredEntries(const FilterCriteria& criteria) const;
    std::string getSummaryString() const;
    
    // Enhanced Output and Export
    void exportAsJson(
        std::ostream& out,
        const FilterCriteria& filter = {},
        bool includeSummary = false,
        bool prettyPrint = false
    ) const;

    // CSV Export (Iteration 1 Feature)
    void exportAsCsv(std::ostream& out, const FilterCriteria& filter = {}) const;

    // New API Extensions for Iteration 1
    std::vector<LogEntry> getSortedFilteredEntries(
        const FilterCriteria& criteria,
        SortBy sortBy = SortBy::TIMESTAMP,
        SortOrder sortOrder = SortOrder::ASCENDING
    ) const;
    std::map<std::string, int> getUniqueMessageCounts() const;
    std::vector<std::pair<std::string, int>> getTopMessages(int n) const;
    void printFilteredEntries(
        std::ostream& out,
        const FilterCriteria& criteria,
        std::string_view formatString = "{timestamp} [{level}] {message}"
    ) const;
    void clear(); // Clears all loaded log entries and reset counts.

    // New API Extensions for Iteration 1 - Advanced Statistical Analysis
    std::vector<TimeWindowStats> getFrequencyDistribution(
        std::chrono::seconds windowSize
    ) const;
    // Optimized O(N) implementation of distribution
    std::vector<TimeWindowStats> getFrequencyDistributionOptimized(std::chrono::seconds windowSize) const;

    // Time Gap Detection and Rate Analysis (Iteration 1 Features)
    std::vector<TimeGap> findTimeGaps(std::chrono::milliseconds minGapDuration) const;
    double getAverageEntryRate() const;

    // Log Level Customization (Iteration 1 Feature)
    void setCustomLogLevelMapping(std::string_view levelString, LogLevel mappedLevel);

    // New API Extensions for Iteration 1 - Multi-file Merge
    void merge(const LogAnalyzer& other);

    // New API Extensions for Iteration 1 - Search functionality
    std::optional<LogEntry> findFirst(const FilterCriteria& criteria) const;
    std::optional<LogEntry> findLast(const FilterCriteria& criteria) const;
    
    // Accessor for the last analysis report
    AnalysisReport getAnalysisReport() const { return lastReport; }

    // Helper for timestamp formatting
    std::string formatTimestamp(std::chrono::system_clock::time_point tp, std::string_view format = "%Y-%m-%d %H:%M:%S") const;

    static LogLevel stringToLogLevel(const std::string& levelStr);
    static std::string logLevelToString(LogLevel level);
    
private:
    std::vector<LogEntry> entries;
    std::map<LogLevel, int> levelCounts;
    AnalysisReport lastReport;
    std::map<std::string, LogLevel, std::less<>> customLevelMappings;

    // Internal parsing logic (refactored from analyze)
    std::expected<void, LogParseError> parseFile(const std::string& filePath, const std::string& pattern, bool appendMode);
    LogLevel resolveLogLevel(const std::string& levelStr) const;
};

#endif // LOG_ANALYZER_H

