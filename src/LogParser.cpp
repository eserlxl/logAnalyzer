#include "LogParser.h"
#include "Utils.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>

// Helper to infer FieldMappings from the deprecated constructor's pattern
static std::vector<FieldMapping> inferFieldMappingsFromPattern(const std::string& pattern) {
    std::vector<FieldMapping> inferredMappings;
    if (!pattern.empty()) {
        // Infer timestamp, level, and message from capture groups 1, 2, and 3 respectively.
        // This is for backward compatibility with the old constructor.
        // The constructor call for TIMESTAMP is ambiguous. Let's assume LogTypes.h has a
        // constructor that takes (LogEntryField, int, std::string) for the format.
        // To resolve the ambiguity reported by the compiler, we must match a specific signature.
        // Let's assume a constructor `FieldMapping(LogEntryField, int, std::string)` exists.
        inferredMappings.emplace_back(LogEntryField::TIMESTAMP, 1, std::string("%Y-%m-%d %H:%M:%S"));
        inferredMappings.emplace_back(LogEntryField::LEVEL, 2);
        inferredMappings.emplace_back(LogEntryField::MESSAGE, 3);
    }
    return inferredMappings;
}

// Default implementation of ILogParser::getLineFilterRegexCompiled
std::regex ILogParser::getLineFilterRegexCompiled() const {
  return std::regex(getLineFilterRegex(), std::regex::optimize);
}

const std::map<std::string, LogLevel, ci_less> DefaultLogParser::DEFAULT_LEVEL_MAPPINGS = {
    {"TRACE", LogLevel::TRACE},
    {"DEBUG", LogLevel::DEBUG},
    {"INFO", LogLevel::INFO},
    {"WARNING", LogLevel::WARNING},
    {"ERROR", LogLevel::ERROR},
    {"FATAL", LogLevel::FATAL}
};

// New constructor with field mappings
DefaultLogParser::DefaultLogParser(
    std::string pattern,
    std::vector<FieldMapping> fieldMappings,
    const std::map<std::string, LogLevel, ci_less> &levelMappings)
    : logPattern(pattern, std::regex::optimize),
      patternString(std::move(pattern)),
      fieldMappings(std::move(fieldMappings)),
      customLevelMappings(levelMappings) {}

// Deprecated constructor, delegates to the new one
DefaultLogParser::DefaultLogParser(
    std::string pattern)
    : DefaultLogParser(
        pattern,
        inferFieldMappingsFromPattern(pattern),
        {}) {}

ParseResult DefaultLogParser::parseLine(std::string_view line,
                                        size_t lineNumber) const {
  ParseResult result;
  result.success = false;
  LogEntry entry;
  entry.id = lineNumber;
  entry.level = LogLevel::UNKNOWN;

  if (patternString.empty()) {
    result.errorMessage = "No regex pattern provided to parser.";
    result.failingPart = std::string(line);
    return result;
  }

  std::string lineStr(line);
  std::smatch match;

  if (!std::regex_match(lineStr, match, logPattern)) {
    result.errorMessage = "Line does not match log pattern.";
    result.failingPart = lineStr;
    return result;
  }

  bool structuredFieldsExplicitlyMapped = false;
  for (const auto& mapping : fieldMappings) {
      if (mapping.field == LogEntryField::STRUCTURED_FIELD) {
          structuredFieldsExplicitlyMapped = true;
          break;
      }
  }

  for (const auto& mapping : fieldMappings) {
    if (mapping.groupIndex < 0 || static_cast<size_t>(mapping.groupIndex) >= match.size() || !match[mapping.groupIndex].matched) {
        continue;
    }
    std::string capturedValue = match[mapping.groupIndex].str();

    switch (mapping.field) {
      case LogEntryField::TIMESTAMP: {
        std::chrono::system_clock::time_point tp;
        bool parsed = false;
        if (!mapping.format.empty()) {
            std::istringstream ss(capturedValue);
            ss >> std::chrono::parse(mapping.format, tp);
            if (!ss.fail()) parsed = true;
        } else {
            const char* isoFormats[] = {"%Y-%m-%dT%H:%M:%S%z", "%Y-%m-%dT%H:%M:%S", "%Y-%m-%d %H:%M:%S", "%Y/%m/%d %H:%M:%S"};
            for (const char* fmt : isoFormats) {
                std::istringstream tempSs(capturedValue);
                tempSs >> std::chrono::parse(fmt, tp);
                if (!tempSs.fail()) {
                    parsed = true;
                    break;
                }
            }
        }
        if (!parsed) {
             result.errorMessage = "Failed to parse timestamp: " + capturedValue;
             result.failingPart = capturedValue;
             return result;
        }
        entry.timestamp = tp;
        break;
      }
      case LogEntryField::LEVEL: {
        auto custom_it = customLevelMappings.find(capturedValue);
        if (custom_it != customLevelMappings.end()) {
            entry.level = custom_it->second;
        } else {
            auto default_it = DEFAULT_LEVEL_MAPPINGS.find(capturedValue);
            if (default_it != DEFAULT_LEVEL_MAPPINGS.end()) {
                entry.level = default_it->second;
            } else {
                entry.level = LogLevel::UNKNOWN;
            }
        }
        break;
      }
      case LogEntryField::MESSAGE: {
        entry.message = capturedValue;
        if (!structuredFieldsExplicitlyMapped) {
            const std::regex kvPattern_legacy("(\\w+)\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s,]+))");
            auto words_begin = std::sregex_iterator(entry.message.begin(), entry.message.end(), kvPattern_legacy);
            auto words_end = std::sregex_iterator();
            for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
                std::smatch kvMatch = *i;
                std::string k = kvMatch[1].str();
                std::string v = kvMatch[2].matched ? kvMatch[2].str() : (kvMatch[3].matched ? kvMatch[3].str() : kvMatch[4].str());
                entry.structuredFields[k] = v;
            }
        }
        break;
      }
      case LogEntryField::STRUCTURED_FIELD: {
        if (!mapping.structuredFieldName.empty()) {
             entry.structuredFields[mapping.structuredFieldName] = capturedValue;
        } else {
            std::string delimiter = mapping.format.empty() ? "=" : mapping.format;
            // Correctly build the regex pattern string before compiling
            std::string pattern_str = "(\\w+)\\s*" + delimiter + "\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s,]+))";
            const std::regex kvPattern(pattern_str);
            auto kv_begin = std::sregex_iterator(capturedValue.begin(), capturedValue.end(), kvPattern);
            auto kv_end = std::sregex_iterator();
            for (std::sregex_iterator i = kv_begin; i != kv_end; ++i) {
                std::smatch kvMatch = *i;
                std::string k = kvMatch[1].str();
                std::string v = kvMatch[2].matched ? kvMatch[2].str() : (kvMatch[3].matched ? kvMatch[3].str() : kvMatch[4].str());
                entry.structuredFields[k] = v;
            }
        }
        break;
      }
      default:
        break;
    }
  }

  result.success = true;
  result.entry = std::move(entry);
  return result;
}

std::unique_ptr<ILogParser> DefaultLogParser::clone() const {
  return std::make_unique<DefaultLogParser>(patternString, fieldMappings, customLevelMappings);
}

std::regex DefaultLogParser::getLineFilterRegexCompiled() const {
  if (patternString.empty()) {
    return std::regex(".*", std::regex::optimize);
  }
  return logPattern;
}
