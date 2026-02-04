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
#include <variant> // New: Required for std::variant
#include <nlohmann/json.hpp> // Required for JSON serialization
#include "Utils.h" // Required for utility functions like logEntryFieldToString
#include "Utils.h" // Required for utility functions like logEntryFieldToString
#include "Utils.h" // Required for utility functions like logEntryFieldToString

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

enum class LogLevel { TRACE, DEBUG, INFO, WARNING, ERROR, CRITICAL, FATAL, UNKNOWN };

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
  SOURCE_FILE,
  LINE_NUMBER,
  THREAD_ID,
  MODULE,
  HOST,
  CUSTOM,
  STRUCTURED_FIELD
};

// New struct to define the mapping from a regex capture group to a LogEntry field
struct FieldMapping {
  std::variant<LogEntryField, std::string> field_identifier; // New: variant
  std::optional<size_t> groupIndex; // Use std::optional to represent unset index
  std::vector<std::string> formats; // Replaces 'format' for TIMESTAMP, used for kv delimiter for STRUCTURED_FIELD
  std::optional<std::string> customFieldType; // New: for custom fields, explicitly state the type if known (e.g., "int", "string", "datetime")

  // Default constructor
  FieldMapping() : field_identifier(LogEntryField::UNKNOWN) {}

  // Constructor for enum fields (existing behavior, adapted for variant)
  FieldMapping(LogEntryField f, std::optional<size_t> groupIdx = std::nullopt, const std::vector<std::string>& fmts = {})
      : field_identifier(f), groupIndex(groupIdx), formats(fmts) {}

  // Constructor for enum fields with int groupIndex (for backward compatibility with old API)
  FieldMapping(LogEntryField f, int gi, const std::string& fmt = "")
      : field_identifier(f), groupIndex(gi == -1 ? std::nullopt : std::make_optional(static_cast<size_t>(gi))) {
    if (!fmt.empty()) {
        formats.push_back(fmt);
    }
  }

  // Constructor for enum fields with int groupIndex and const char* format (for backward compatibility)
  FieldMapping(LogEntryField f, int gi, const char* fmt)
      : field_identifier(f), groupIndex(gi == -1 ? std::nullopt : std::make_optional(static_cast<size_t>(gi))) {
    if (fmt != nullptr) {
        formats.push_back(fmt);
    }
  }

  // Constructor for custom string fields
  FieldMapping(const std::string& customFieldName, std::optional<size_t> groupIdx = std::nullopt, const std::vector<std::string>& fmts = {}, const std::optional<std::string>& customType = std::nullopt)
      : field_identifier(customFieldName), groupIndex(groupIdx), formats(fmts), customFieldType(customType) {}

  // Convenience overload for custom string fields with a single format string
  FieldMapping(const std::string& customFieldName, std::optional<size_t> groupIdx, const std::string& format, const std::optional<std::string>& customType = std::nullopt)
      : field_identifier(customFieldName), groupIndex(groupIdx), formats({format}), customFieldType(customType) {}

  // Explicitly defined copy and move constructors/assignment operators for robust vector usage
  FieldMapping(const FieldMapping&) = default;
  FieldMapping(FieldMapping&&) = default;
  FieldMapping& operator=(const FieldMapping&) = default;
  FieldMapping& operator=(FieldMapping&&) = default;
};

// Forward declarations to break circular dependency with Utils.h
namespace Utils {
    std::string logEntryFieldToString(LogEntryField field);
    LogEntryField stringToLogEntryField(const std::string& fieldStr);
}

// --- JSON Conversion for FieldMapping ---
inline void to_json(nlohmann::json& j, const FieldMapping& fm) {
    j = nlohmann::json{
        {"groupIndex", fm.groupIndex},
        {"formats", fm.formats}
    };
    // Handle the variant for field_identifier
    if (std::holds_alternative<LogEntryField>(fm.field_identifier)) {
        j["field"] = Utils::logEntryFieldToString(std::get<LogEntryField>(fm.field_identifier));
    } else if (std::holds_alternative<std::string>(fm.field_identifier)) {
        j["field"] = std::get<std::string>(fm.field_identifier);
        if (fm.customFieldType) {
            j["customFieldType"] = *fm.customFieldType;
        }
    }
}

inline void from_json(const nlohmann::json& j, FieldMapping& fm) {
    std::vector<std::string> errors;
    
    // Required fields
    if (j.contains("groupIndex") && j.at("groupIndex").is_number_integer()) {
        fm.groupIndex = j.at("groupIndex").get<int>();
    } else {
        errors.push_back("FieldMapping is missing or has invalid 'groupIndex'.");
    }

    if (j.contains("formats") && j.at("formats").is_array()) {
        fm.formats = j.at("formats").get<std::vector<std::string>>();
    } // 'formats' is optional, so no error if missing

    // Field identifier (enum or string)
    if (j.contains("field")) {
        if (j.at("field").is_string()) {
            std::string fieldStr = j.at("field").get<std::string>();
            // Try to convert to standard LogEntryField first
            LogEntryField standardField = Utils::stringToLogEntryField(fieldStr);
            if (standardField != LogEntryField::UNKNOWN) {
                fm.field_identifier = standardField;
            } else {
                // It's not a standard field, assume it's a custom field name
                fm.field_identifier = fieldStr;
                // Check for customFieldType if it's a custom field
                if (j.contains("customFieldType") && j.at("customFieldType").is_string()) {
                    fm.customFieldType = j.at("customFieldType").get<std::string>();
                } else {
                    // If it's a custom field but no type is provided, it might be an issue depending on requirements.
                    // For now, we'll allow it but might add a validation check later.
                }
            }
        } else {
            errors.push_back("FieldMapping 'field' must be a string.");
        }
    } else {
        errors.push_back("FieldMapping is missing the required 'field' key.");
    }

    if (!errors.empty()) {
        throw nlohmann::json::exception(errors.size(), errors[0].c_str()); // Basic exception for now
    }
}
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
