#ifndef LOG_TYPES_H
#define LOG_TYPES_H

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <cctype> // Required for std::tolower

// Case-insensitive comparator for strings (moved from LogParser.h as it's a generic utility)
struct ci_less {
  struct nocase_compare {
    bool operator()(const unsigned char &c1, const unsigned char &c2) const {
      return std::tolower(c1) < std::tolower(c2);
    }
  };
  bool operator()(const std::string &s1, const std::string &s2) const {
    return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(),
                                        s2.end(), nocase_compare());
  }
};

// Enum for different pattern matching types
enum class PatternType { Literal, Glob, Regex };

enum class LogLevel { TRACE, DEBUG, INFO, WARNING, ERROR, FATAL, UNKNOWN };

// New enum to specify which LogEntry field a regex capture group maps to
enum class LogEntryField {
  UNKNOWN,
  TIMESTAMP,
  LEVEL,
  MESSAGE,
  SOURCE_FILE, // Maps to LogEntry::sourceFile
  STRUCTURED_FIELD // For fields that go into LogEntry::structuredFields
};

// New struct to define the mapping from a regex capture group to a LogEntry field
struct FieldMapping {
  LogEntryField field = LogEntryField::UNKNOWN;
  int groupIndex = -1; // Use index instead of name (0 for full match, 1 for first capture group, etc.)
  std::string format;    // Optional: format string for TIMESTAMP (e.g., "%Y-%m-%d %H:%M:%S")
                         //           or delimiter for STRUCTURED_FIELD (e.g., "=" for key=value pairs)
  std::string structuredFieldName; // Required if field is STRUCTURED_FIELD, key for the map

  // Default constructor
  FieldMapping() = default;

  // Constructor for non-structured fields
  FieldMapping(LogEntryField f, int gi, const std::string& fmt = "")
      : field(f), groupIndex(gi), format(fmt) {}

  // Constructor for structured fields
  FieldMapping(LogEntryField f, int gi, const std::string& fmt, const std::string& sfn)
      : field(f), groupIndex(gi), format(fmt), structuredFieldName(sfn) {}
};

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
