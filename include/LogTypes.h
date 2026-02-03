#ifndef LOG_TYPES_H
#define LOG_TYPES_H

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <cctype> // Required for std::tolower
#include <locale> // Required for std::locale, std::use_facet, std::ctype
#include <limits> // Required for std::numeric_limits

// Case-insensitive comparator for strings (moved from LogParser.h as it's a generic utility)
struct ci_less {
  struct nocase_compare {
    // Using static const std::locale classic_locale for efficiency and locale-independence
    char toLowerChar(char c) const {
      static const std::locale classic_locale;
      return std::use_facet<std::ctype<char>>(classic_locale).tolower(c);
    }

    bool operator()(char c1, char c2) const {
      return toLowerChar(c1) < toLowerChar(c2);
    }
  };
  bool operator()(const std::string &s1, const std::string &s2) const {
    return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(),
                                        s2.end(), nocase_compare());
  }
};

// Enum for different pattern matching types
enum class PatternType { Literal, Regex, Wildcard };

enum class LogLevel { TRACE, DEBUG, INFO, WARNING, ERROR, FATAL, UNKNOWN };

enum class ParseResultStatus {
    SUCCESS,                // Parsing successful
    PATTERN_MISMATCH,       // The log line did not match the parser's regex pattern
    TIMESTAMP_PARSE_ERROR,  // Failed to parse the timestamp field
    LEVEL_PARSE_ERROR,      // Failed to parse the log level field or level is unknown
    FIELD_PARSE_ERROR,      // General error parsing a specific field (not timestamp/level)
    BUFFERED_CONTINUATION,  // Line was buffered as a continuation of a multi-line entry (no full entry returned yet)
    UNMATCHED_START_PATTERN // (For multi-line parsers) Line did not match the start pattern and was not a continuation.
                            // If logEntryStartPattern is configured, this could indicate a line that doesn't belong to any entry,
                            // or it's the start of an entry that just doesn't match the start pattern.
};

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
  std::optional<size_t> groupIndex; // Use std::optional to represent unset index
  std::vector<std::string> formats; // Replaces 'format' for TIMESTAMP, used for kv delimiter for STRUCTURED_FIELD
  std::string structuredFieldName; // Required if field is STRUCTURED_FIELD, key for the map

  // Default constructor
  FieldMapping() = default;

  // Constructor for non-structured fields
  FieldMapping(LogEntryField f, int gi, const std::string& fmt = "")
      : field(f), groupIndex(gi == -1 ? std::nullopt : std::make_optional(static_cast<size_t>(gi))) {
    if (!fmt.empty()) {
        formats.push_back(fmt);
    }
  }

  // Constructor for non-structured fields accepting C-style strings
  FieldMapping(LogEntryField f, int gi, const char* fmt)
      : field(f), groupIndex(gi == -1 ? std::nullopt : std::make_optional(static_cast<size_t>(gi))) {
    if (fmt != nullptr) {
        formats.push_back(fmt);
    }
  }

  // Constructor for structured fields (kv_delimiter is now part of formats vector)
  FieldMapping(LogEntryField f, int gi, const std::string& sfn, const std::string& kv_delimiter = "=")
      : field(f), groupIndex(gi == -1 ? std::nullopt : std::make_optional(static_cast<size_t>(gi))), formats({kv_delimiter}), structuredFieldName(sfn) {}

  // Explicitly defined copy and move constructors/assignment operators for robust vector usage
  FieldMapping(const FieldMapping&) = default;
  FieldMapping(FieldMapping&&) = default;
  FieldMapping& operator=(const FieldMapping&) = default;
  FieldMapping& operator=(FieldMapping&&) = default;
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
  std::vector<LogParseError> parseErrors;
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
  size_t id = std::numeric_limits<size_t>::max(); // Unique identifier for each log entry
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
  // Existing field for backward compatibility; will be derived from 'status'
  // For new code, prefer checking 'status == ParseResultStatus::SUCCESS'.
  bool success = false;

  // New: Provides granular status of the parsing attempt.
  ParseResultStatus status = ParseResultStatus::SUCCESS;

  LogEntry entry; // The parsed log entry (valid only if status is SUCCESS)

  // Existing: Human-readable error message.
  std::string errorMessage;

  // Existing: The part of the log line that caused the failure.
  std::string failingPart;

  // New: Structured details about the error, e.g., {"field": "timestamp", "value": "invalid_date"}.
  std::map<std::string, std::string> errorDetails;
};

#endif // LOG_TYPES_H
