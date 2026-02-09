// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef LOG_TYPES_H
#define LOG_TYPES_H

#include <chrono>
#include <algorithm>
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
  std::shared_ptr<const std::regex> compiledKvPattern; // Use shared_ptr for efficient copying and thread-safety

  // Default constructor
  FieldMapping() : field(LogEntryField::UNKNOWN) {}

  // Constructor for enum fields (existing behavior, adapted for variant)
  FieldMapping(LogEntryField f, std::optional<size_t> groupIdx = std::nullopt, const std::vector<std::string>& fmts = {})
      : field(f), groupIndex(groupIdx), formats(fmts) {}

  // Constructor for enum fields with int groupIndex (for backward compatibility with old API)
  [[deprecated("Use constructor with std::optional<size_t> for groupIndex")]]
  FieldMapping(LogEntryField f, int gi, const std::string& fmt = "")
      : field(f), groupIndex(gi == -1 ? std::nullopt : std::make_optional(static_cast<size_t>(gi))) {
    if (!fmt.empty()) {
        formats.push_back(fmt);
    }
  }

  // Constructor for enum fields with int groupIndex and const char* format (for backward compatibility)
  [[deprecated("Use constructor with std::optional<size_t> for groupIndex and std::vector<std::string> for formats")]]
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

    // Note: compiledKvPattern is not serialized to JSON as it's a runtime object.
    // It will be re-compiled during deserialization if a pattern is provided via 'formats'.
}

inline void from_json(const nlohmann::json& j, FieldMapping& fm) {
    fm = FieldMapping{};
    // Required fields check
    if (!j.contains("groupIndex")) {
         throw nlohmann::json::parse_error::create(101, 0, "FieldMapping must contain 'groupIndex'", &j);
    }

    if (j.at("groupIndex").is_number_integer()) {
        const auto rawGroupIndex = j.at("groupIndex").get<long long>();
        if (rawGroupIndex < 0) {
            throw nlohmann::json::type_error::create(302, "FieldMapping 'groupIndex' must be non-negative when provided", &j);
        }
        fm.groupIndex = static_cast<size_t>(rawGroupIndex);
    } else if (j.at("groupIndex").is_null()) {
        fm.groupIndex = std::nullopt;
    } else {
        throw nlohmann::json::type_error::create(302, "FieldMapping 'groupIndex' must be an integer or null", &j);
    }

    if (j.contains("formats")) {
        if (j.at("formats").is_null()) {
            fm.formats.clear();
        } else if (j.at("formats").is_array()) {
            fm.formats = j.at("formats").get<std::vector<std::string>>();
        } else {
            throw nlohmann::json::type_error::create(302, "FieldMapping 'formats' must be an array or null", &j);
        }
    } // 'formats' is optional

    // Field identifier (enum or string)
    if (!j.contains("field")) {
        throw nlohmann::json::parse_error::create(101, 0, "FieldMapping must contain 'field'", &j);
    }

    if (j.at("field").is_string()) {
        std::string fieldStr = j.at("field").get<std::string>();
        const auto first = fieldStr.find_first_not_of(" \t");
        if (first == std::string::npos) {
            throw nlohmann::json::parse_error::create(101, 0, "FieldMapping 'field' cannot be empty", &j);
        }
        const auto last = fieldStr.find_last_not_of(" \t");
        fieldStr = fieldStr.substr(first, last - first + 1);
        LogEntryField standardField = Utils::stringToLogEntryField(fieldStr);
        if (standardField != LogEntryField::UNKNOWN && standardField != LogEntryField::CUSTOM) {
             fm.field = standardField;
            if (j.contains("customFieldType") && !j.at("customFieldType").is_null()) {
                throw nlohmann::json::parse_error::create(101, 0, "FieldMapping 'customFieldType' is only valid for custom fields", &j);
            }
        } else {
            fm.field = fieldStr; // It's a custom field name
            if (j.contains("customFieldType")) {
                if (j.at("customFieldType").is_null()) {
                    fm.customFieldType = std::nullopt;
                } else if (j.at("customFieldType").is_string()) {
                    std::string typeValue = j.at("customFieldType").get<std::string>();
                    const auto typeFirst = typeValue.find_first_not_of(" \t");
                    if (typeFirst == std::string::npos) {
                        throw nlohmann::json::parse_error::create(101, 0, "FieldMapping 'customFieldType' cannot be empty", &j);
                    }
                    const auto typeLast = typeValue.find_last_not_of(" \t");
                    fm.customFieldType = typeValue.substr(typeFirst, typeLast - typeFirst + 1);
                } else {
                    throw nlohmann::json::type_error::create(302, "FieldMapping 'customFieldType' must be a string or null", &j);
                }
            }
        }
    } else {
        throw nlohmann::json::type_error::create(302, "FieldMapping 'field' must be a string", &j);
    }

    // Handle regex compilation for structured fields
    if (std::holds_alternative<LogEntryField>(fm.field) && std::get<LogEntryField>(fm.field) == LogEntryField::STRUCTURED_FIELD) {
        auto selectedPatternIt = std::find_if(fm.formats.begin(), fm.formats.end(), [](const std::string& pattern) {
            return pattern.find_first_not_of(" \t") != std::string::npos;
        });
        if (selectedPatternIt != fm.formats.end()) {
            try {
                fm.compiledKvPattern = std::make_shared<const std::regex>(*selectedPatternIt, std::regex::optimize);
            } catch (const std::regex_error& e) {
                throw nlohmann::json::parse_error::create(101, 0, "Invalid regex pattern for STRUCTURED_FIELD: " + std::string(e.what()), &j);
            }
        }
    }
}

// LogEntry structure enhancement

enum class ParseError {
    SUCCESS,
    PARTIAL_FAILURE,
    UNKNOWN_ERROR,
    INVALID_REGEX_PATTERN,
    FILE_ERROR, // New: For file not found/not readable errors
    SETTINGS_RESTORE_FAILED, // New: For when restoring settings fails
    CANCELLED // New: For operation cancellation
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
    std::optional<ErrorCode::Error> originalError; // New member to store the underlying ErrorCode::Error

    // Default constructor
    LogParseError(ParseError err = ParseError::UNKNOWN_ERROR, std::string msg = "", size_t line = 0)
        : error(err), message(std::move(msg)), lineNumber(line), originalError(std::nullopt) {}

    // Constructor with original ErrorCode::Error
    LogParseError(ParseError err, std::string msg, size_t line, const ErrorCode::Error& original)
        : error(err), message(std::move(msg)), lineNumber(line), originalError(original) {}
    
    // Constructor to convert ErrorCode::Error directly
    explicit LogParseError(const ErrorCode::Error& err, size_t line = 0)
        : message(err.message), lineNumber(line), originalError(err) {
            // Map ErrorCode::Code to ParseError if possible
            if (err.code == Code::FileNotFound || err.code == Code::FileNotReadable) {
                error = ParseError::FILE_ERROR;
            } else if (err.code == Code::InvalidRegex) {
                error = ParseError::INVALID_REGEX_PATTERN;
            } else if (err.code == Code::SettingsRestoreFailed) {
                error = ParseError::SETTINGS_RESTORE_FAILED;
            } else {
                error = ParseError::UNKNOWN_ERROR;
            }
        }
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
      if (parsingErrors.empty()) {
          return "";
      }
      std::stringstream ss;
      for (size_t i = 0; i < parsingErrors.size(); ++i) {
          ss << parsingErrors[i].message;
          if (i < parsingErrors.size() - 1) {
              ss << "; ";
          }
      }
      return ss.str();
  }

  // Auto-generate C++20 default comparison
  // This is the simplest and most robust way to ensure all members are compared.
  // The compiler will do the right thing.
  bool operator==(const LogEntry &other) const = default;

  // New: Convert LogEntry to nlohmann::json
  nlohmann::json toJson() const;
};


#endif // LOG_TYPES_H
