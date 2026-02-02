#ifndef LOG_ANALYZER_H
#define LOG_ANALYZER_H

#include <string>
#include <vector>
#include <map>

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

class LogAnalyzer {
public:
    LogAnalyzer();
    void analyze(const std::string& filePath);
    void printSummary() const;
    void filterByLevel(LogLevel level) const;

private:
    LogLevel stringToLogLevel(const std::string& levelStr);
    std::string logLevelToString(LogLevel level) const;

    std::vector<LogEntry> entries;
    std::map<LogLevel, int> levelCounts;
};

#endif // LOG_ANALYZER_H
