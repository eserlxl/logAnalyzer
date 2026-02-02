#ifndef LOG_ANALYZER_H
#define LOG_ANALYZER_H

#include <string>
#include <vector>
#include <map>
#include <iostream>

enum class LogLevel {
    INFO,
    WARNING,
    ERROR,
    DEBUG,
    UNKNOWN
};

struct LogEntry {
    std::string timestamp;
    LogLevel level;
    std::string message;
};

struct FilterCriteria {
    std::vector<LogLevel> levels; // Multiple levels (empty means all)
    std::string keyword;         // Case-insensitive substring match
};

class LogAnalyzer {
public:
    LogAnalyzer();
    void analyze(const std::string& filePath, const std::string& pattern = "");
    void printSummary(std::ostream& out = std::cout) const;

    // New API Extensions
    const std::vector<LogEntry>& getEntries() const;
    std::vector<LogEntry> getFilteredEntries(const FilterCriteria& criteria) const;
    std::string getSummaryString() const;
    void exportAsJson(std::ostream& out, const FilterCriteria& filter = {}) const;

    static LogLevel stringToLogLevel(const std::string& levelStr);
    static std::string logLevelToString(LogLevel level);

private:
    std::vector<LogEntry> entries;
    std::map<LogLevel, int> levelCounts;
};

#endif // LOG_ANALYZER_H
