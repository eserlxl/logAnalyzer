#ifndef LOG_TYPES_H
#define LOG_TYPES_H

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

enum class LogLevel { TRACE, DEBUG, INFO, WARNING, ERROR, FATAL, UNKNOWN };

// Enum for Parse Errors
enum class ParseError {
  SUCCESS,
  FILE_OPEN_FAILED,
  INVALID_REGEX_PATTERN,
  PARTIAL_FAILURE // Some lines failed to parse but others succeeded
};

// Type for robust error handling
struct LogParseError {
  ParseError code = ParseError::SUCCESS;
  std::string message;
  size_t lineNumber = 0;

  bool operator==(const LogParseError &other) const {
    return code == other.code && message == other.message &&
           lineNumber == other.lineNumber;
  }
};

// Struct for comprehensive analysis results
struct AnalysisReport {
  size_t linesProcessed = 0;
  size_t successfulParses = 0;
  std::vector<std::pair<size_t, std::string>>
      parseErrors; // line number -> error reason
  ParseError status = ParseError::SUCCESS;
  std::string message; // Added message field
};

// Struct for time-gap analysis
struct TimeGap {
  std::chrono::system_clock::time_point start;
  std::chrono::system_clock::time_point end;
  std::chrono::system_clock::duration duration;
};

// Struct for time-windowed statistics
struct TimeWindowStats {
  std::chrono::system_clock::time_point windowStart;
  std::chrono::system_clock::time_point windowEnd; // Added for clarity
  std::map<LogLevel, int> counts;
  int totalCount;
};

// LogEntry structure enhancement
struct LogEntry {
  size_t id; // Unique identifier for each log entry
  std::string sourceFile; // The file from which this entry was read
  std::chrono::system_clock::time_point timestamp;
  LogLevel level;
  std::string message;
  std::map<std::string, std::string>
      structuredFields; // For structured data

  bool operator==(const LogEntry &other) const {
    return id == other.id && timestamp == other.timestamp &&
           level == other.level && message == other.message &&
           structuredFields == other.structuredFields;
  }
};

// New struct for returning detailed parse results
struct ParseResult {
  std::optional<LogEntry> entry;
  bool success = false;
  std::string errorMessage;
  std::string failingPart;
};

#endif // LOG_TYPES_H
