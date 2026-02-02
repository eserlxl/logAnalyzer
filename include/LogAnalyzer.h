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

enum class LogLevel {

    INFO,

    WARNING,

    ERROR,

    DEBUG,

    UNKNOWN

};



// New Enum for Parse Errors (Iteration 1 Feature)

enum class ParseError {

    SUCCESS,

    FILE_OPEN_FAILED,

    INVALID_REGEX_PATTERN,

    PARTIAL_FAILURE // Some lines failed to parse but others succeeded

};



// New struct for time-windowed statistics (Iteration 1 Feature)

struct TimeWindowStats {

    std::chrono::system_clock::time_point windowStart;

    std::map<LogLevel, int> counts;

    int totalCount;

};



// LogEntry structure enhancement

struct LogEntry {

    std::chrono::system_clock::time_point timestamp; // Changed from std::string
    LogLevel level;
    std::string message;
};

// FilterCriteria structure expansion
struct FilterCriteria {
    std::vector<LogLevel> levels;      // Multiple levels (empty means all)
    std::optional<LogLevel> minLogLevel; // New: Filter for this level and above
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
    // Modified analyze (Breaking change if return type was void)
    std::expected<size_t, ParseError> analyze(const std::string& filePath, const std::string& pattern = "");
    
    // Function signature for streaming (Iteration 1 Feature)
    void analyzeStream(
        const std::string& filePath, 
        std::function<bool(const LogEntry&)> entryCallback,
        const std::string& pattern = ""
    );

    void printSummary(std::ostream& out = std::cout) const;

    // Existing API Extensions (signatures updated if needed)
    const std::vector<LogEntry>& getEntries() const;
    std::vector<LogEntry> getFilteredEntries(const FilterCriteria& criteria) const;
    std::string getSummaryString() const;
    
    // Enhanced Output and Export
    void exportAsJson(
        std::ostream& out,
        const FilterCriteria& filter = {},
        bool includeSummary = false,      // New: include summary statistics
        bool prettyPrint = false          // New: pretty-print JSON
    ) const;

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

    // New API Extensions for Iteration 1 - Multi-file Merge
    void merge(const LogAnalyzer& other);

    // New API Extensions for Iteration 1 - Search functionality
    std::optional<LogEntry> findFirst(const FilterCriteria& criteria) const;
    std::optional<LogEntry> findLast(const FilterCriteria& criteria) const;
    
    // Helper for timestamp formatting
    std::string formatTimestamp(std::chrono::system_clock::time_point tp, std::string_view format = "%Y-%m-%d %H:%M:%S") const;

    static LogLevel stringToLogLevel(const std::string& levelStr);
    static std::string logLevelToString(LogLevel level);
    
private:
    std::vector<LogEntry> entries;
    std::map<LogLevel, int> levelCounts;
};

#endif // LOG_ANALYZER_H
