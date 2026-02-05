#ifndef LOG_TYPES_H
#define LOG_TYPES_H

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <regex> // Required for std::regex
#include <cctype> // Required for std::tolower
#include <locale> // Required for std::locale, std::use_facet, std::ctype
#include <limits> // Required for std::numeric_limits
#include <variant> // Required for std::variant
#include <nlohmann/json.hpp> // Required for JSON serialization
#include "core/Error.h" // New: For Error struct and Result alias
#include "core/CiLess.h" // Include the new header for ci_less comparator

// Enum for different pattern matching types
enum class PatternType { Literal, Regex, Wildcard };

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL,
    FATAL,
    UNKNOWN
};

// Moved into Utils namespace
namespace Utils {
    LogLevel stringToLogLevel(const std::string &levelStr, const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> &customMappings);
    LogLevel stringToLogLevel(const std::string &levelStr);
    std::string logLevelToString(LogLevel level);
}


// New enum to specify which LogEntry field a regex capture group maps to
enum class LogEntryField {
  UNKNOWN,
  ID, // Added for export purposes
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
  std::variant<LogEntryField, std::string> field; // Renamed back to field
  std::optional<size_t> groupIndex; // Use std::optional to represent unset index
  std::vector<std::string> formats; // Replaces 'format' for TIMESTAMP, used for kv delimiter for STRUCTURED_FIELD
  std::optional<std::string> customFieldType; // New: for custom fields, explicitly state the type if known (e.g., "int", "string", "datetime")
  std::optional<std::regex> compiledKvPattern; // New: for structured fields, pre-compiled regex for key-value parsing

  // Default constructor
  FieldMapping() : field(LogEntryField::UNKNOWN) {}

  // Constructor for enum fields (existing behavior, adapted for variant)
  FieldMapping(LogEntryField f, std::optional<size_t> groupIdx = std::nullopt, const std::vector<std::string>& fmts = {})
      : field(f), groupIndex(groupIdx), formats(fmts) {}

  // Constructor for enum fields with int groupIndex (for backward compatibility with old API)
  FieldMapping(LogEntryField f, int gi, const std::string& fmt = "")
      : field(f), groupIndex(gi == -1 ? std::nullopt : std::make_optional(static_cast<size_t>(gi))) {
    if (!fmt.empty()) {
        formats.push_back(fmt);
    }
  }

  // Constructor for enum fields with int groupIndex and const char* format (for backward compatibility)
  FieldMapping(LogEntryField f, int gi, const char* fmt)
      : field(f), groupIndex(gi == -1 ? std::nullopt : std::make_optional(static_cast<size_t>(gi))) {
    if (fmt != nullptr) {
        formats.push_back(fmt);
    }
  }

  // Constructor for custom string fields
  FieldMapping(const std::string& customFieldName, std::optional<size_t> groupIdx = std::nullopt, const std::vector<std::string>& fmts = {}, const std::optional<std::string>& customType = std::nullopt)
      : field(customFieldName), groupIndex(groupIdx), formats(fmts), customFieldType(customType) {}

  // Convenience overload for custom string fields with a single format string
  FieldMapping(const std::string& customFieldName, std::optional<size_t> groupIdx, const std::string& format, const std::optional<std::string>& customType = std::nullopt)
      : field(customFieldName), groupIndex(groupIdx), formats({format}), customFieldType(customType) {}

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
    j = nlohmann::json::object();
    if (fm.groupIndex) {
        j["groupIndex"] = *fm.groupIndex;
    } else {
        j["groupIndex"] = nullptr;
    }
    j["formats"] = fm.formats;
    
    // Handle the variant for field
    if (std::holds_alternative<LogEntryField>(fm.field)) {
        j["field"] = Utils::logEntryFieldToString(std::get<LogEntryField>(fm.field));
    } else if (std::holds_alternative<std::string>(fm.field)) {
        j["field"] = std::get<std::string>(fm.field);
        if (fm.customFieldType) {
            j["customFieldType"] = *fm.customFieldType;
        }
    }
}

inline void from_json(const nlohmann::json& j, FieldMapping& fm) {
    std::vector<std::string> errors;
    
    // Required fields
    if (j.contains("groupIndex")) {
        if (j.at("groupIndex").is_number_integer()) {
            fm.groupIndex = j.at("groupIndex").get<size_t>();
        } else if (j.at("groupIndex").is_null()) {
            fm.groupIndex = std::nullopt;
        } else {
            errors.push_back("FieldMapping has invalid 'groupIndex'.");
        }
    } else {
        errors.push_back("FieldMapping is missing 'groupIndex'.");
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
                fm.field = standardField;
            } else {
                // It's not a standard field, assume it's a custom field name
                fm.field = fieldStr;
                // Check for customFieldType if it's a custom field
                if (j.contains("customFieldType") && j.at("customFieldType").is_string()) {
                    fm.customFieldType = j.at("customFieldType").get<std::string>();
                }
            }
        } else {
            errors.push_back("FieldMapping 'field' must be a string.");
        }
    }
    else {
        errors.push_back("FieldMapping is missing the required 'field' key.");
    }

    if (!errors.empty()) {
        throw std::runtime_error(errors[0]); // Throw standard exception
    }
}

// LogEntry structure enhancement

enum class ParseError {
    SUCCESS,
    PARTIAL_FAILURE,
    UNKNOWN_ERROR,
    INVALID_REGEX_PATTERN
};

enum class ParserErrorAction {
    Ignore,
    Warn,
    Throw
};

struct LogParseError {
    ParseError error;
    std::string message;
    size_t lineNumber;
};

struct AnalysisReport {
    size_t linesProcessed = 0;
    size_t successfulParses = 0;
    std::vector<LogParseError> parseErrors;
    ParseError status = ParseError::SUCCESS;
};

struct TimeWindowStats {
    std::chrono::system_clock::time_point windowStart;
    std::chrono::system_clock::time_point windowEnd;
    size_t entryCount = 0;
};

struct TimeGap {
    std::chrono::system_clock::time_point gapStart;
    std::chrono::system_clock::time_point gapEnd;
    std::chrono::milliseconds duration;
};

struct LogEntry {
  std::optional<size_t> id; // Unique identifier for each log entry
  std::string sourceFile; // The file from which this entry was read
  std::optional<size_t> sourceLineNumber; // New: Line number in the source file
  std::optional<std::chrono::system_clock::time_point> timestamp;
  LogLevel level;
  std::string message;
  std::optional<std::string> threadId; // New: Direct member for thread ID
  std::optional<std::string> module;   // New: Direct member for module
  std::optional<std::string> host;     // New: Direct member for host
  std::map<std::string, std::string> customFields;
  std::optional<std::string> structuredData; // New: Raw structured data string
  std::vector<ErrorCode::Error> parsingErrors; // Added to store parsing errors
          
  // Helper to check if any parsing errors occurred
  bool hasParsingErrors() const {
      return !parsingErrors.empty();
  }
          
  // Helper to get all error messages concatenated
  std::string getParsingErrorsAsString() const {
      std::string all_errors;
      for (const auto& err : parsingErrors) {
          if (!all_errors.empty()) {
              all_errors += "; ";
          }
          all_errors += err.message;
      }
      return all_errors;
  }
  bool operator==(const LogEntry &other) const {
    return id == other.id && timestamp == other.timestamp &&
           level == other.level && message == other.message &&
           threadId == other.threadId && // Compare new members
           module == other.module &&     // Compare new members
           host == other.host &&         // Compare new members
           customFields == other.customFields;
  }
};


#endif // LOG_TYPES_H
