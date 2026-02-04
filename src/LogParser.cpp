#include "LogParser.h"
#include "LogTypes.h"  // For FieldMapping, LogEntryField
#include <chrono>      // For std::chrono::parse
#include <map>         // For std::map
#include <memory>      // For std::unique_ptr
#include <optional>    // For std::optional
#include <regex>       // For std::regex
#include <sstream>     // For std::istringstream
#include <string>
#include <string_view> // For std::string_view
#include <vector>      // For std::vector

// Helper function to infer field mappings from a pattern string.
// This function needs to be defined before it's used by the DefaultLogParser constructor.
std::vector<FieldMapping> inferFieldMappingsFromPattern([[maybe_unused]] const std::string& pattern) {
    // This is a basic inference; a real implementation might parse the regex pattern
    // to identify named capture groups or specific field indicators.
    // For now, we use fixed group indices as a placeholder.
    // TODO: Implement actual regex pattern parsing to infer field mappings.
    // For now, hardcode some defaults based on common log formats.
    // This is a simplified example and would need a more robust implementation
    // to truly infer from a generic regex pattern.
    std::vector<FieldMapping> inferredMappings;
    inferredMappings.push_back(FieldMapping(LogEntryField::TIMESTAMP, 1, std::string("%Y-%m-%d %H:%M:%S")));
    inferredMappings.push_back(FieldMapping(LogEntryField::LEVEL, 2));
    inferredMappings.push_back(FieldMapping(LogEntryField::MESSAGE, 3));
    return inferredMappings;
}

// New factory function to handle regex compilation errors
std::expected<std::unique_ptr<DefaultLogParser>, LogParseError> DefaultLogParser::create(
    std::string pattern,
    std::vector<FieldMapping> fieldMappings,
    const std::map<std::string, LogLevel, ci_less>& levelMappings,
    std::optional<std::string> logEntryStartPattern) {
    try {
        // Use a private constructor or a helper to avoid direct instantiation
        // For now, we call the public constructor and catch exceptions.
        auto parser = std::make_unique<DefaultLogParser>(
            std::move(pattern),
            std::move(fieldMappings),
            levelMappings,
            std::move(logEntryStartPattern)
        );
        return parser;
    } catch (const std::regex_error& e) {
        return std::unexpected(LogParseError{ParseError::INVALID_REGEX_PATTERN, e.what(), 0});
    }
}

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

// New constructor with field mappings, level mappings, and optional log entry start pattern
DefaultLogParser::DefaultLogParser(
    std::string pattern,
    std::vector<FieldMapping> fieldMappings,
    const std::map<std::string, LogLevel, ci_less> &levelMappings,
    std::optional<std::string> logEntryStartPattern)
    : logPattern(pattern, std::regex::optimize),
      patternString(std::move(pattern)),
      fieldMappings(std::move(fieldMappings)),
      customLevelMappings(levelMappings),
      logEntryStartPatternString(std::move(logEntryStartPattern))
{
    if (logEntryStartPatternString.has_value()) {
        logEntryStartRegex = std::regex(logEntryStartPatternString.value(), std::regex::optimize);
    }
}

// Deprecated constructor, delegates to the new one
DefaultLogParser::DefaultLogParser(
    std::string pattern)
    : DefaultLogParser(
        pattern,
        inferFieldMappingsFromPattern(pattern),
        {},
        std::nullopt) {}

// Private helper for parsing a single line, called by processLine and the public parseLine override.
ParseResult DefaultLogParser::parseLineInternal(std::string_view line,
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
    // Check if groupIndex has a value and if it's a valid index for the match.
    if (!mapping.groupIndex.has_value() || mapping.groupIndex.value() >= match.size() || !match[mapping.groupIndex.value()].matched) {
        continue;
    }
    std::string capturedValue = match[mapping.groupIndex.value()].str();

    switch (mapping.field) {
      case LogEntryField::TIMESTAMP: {
        std::chrono::system_clock::time_point tp;
        bool parsed = false;
        if (!mapping.formats.empty() && !mapping.formats[0].empty()) { // Use formats[0]
            std::istringstream ss(capturedValue);
            // Ensure to use the correct chrono::parse or equivalent
            // For simplicity, assuming std::chrono::parse is available and works as expected.
            // If not, a manual parsing approach with std::get_time would be needed.
            ss >> std::chrono::parse(mapping.formats[0], tp); // Use formats[0]
            if (!ss.fail()) parsed = true;
        } else {
            // Attempt common ISO formats and other common formats
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
        }
        else {
            auto default_it = DEFAULT_LEVEL_MAPPINGS.find(capturedValue);
            if (default_it != DEFAULT_LEVEL_MAPPINGS.end()) {
                entry.level = default_it->second;
            }
            else {
                entry.level = LogLevel::UNKNOWN;
            }
        }
        break;
      }
      case LogEntryField::MESSAGE: {
        entry.message = capturedValue;
        // Legacy structured field parsing from MESSAGE if no explicit STRUCTURED_FIELD mapping is present
        if (!structuredFieldsExplicitlyMapped) {
            const std::regex kvPattern_legacy("([\\w.-]+)\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s,.]+))");
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
        }
        else { // If structuredFieldName is empty, try to parse key-value pairs from capturedValue
            std::string delimiter = (!mapping.formats.empty() && !mapping.formats[0].empty()) ? mapping.formats[0] : "="; // Use formats[0]
            std::string pattern_str = "([\\w.-]+)\\s*" + delimiter + "\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s,]+))";
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
      case LogEntryField::SOURCE_FILE: {
          entry.sourceFile = capturedValue;
          break;
      }
      default:
        // Handle other fields or ignore
        break;
    }
  }

  result.success = true;
  result.entry = std::move(entry);
  return result;
}

// Implementation for the pure virtual method from ILogParser interface.
// This method provides a stateless single-line parsing capability, separate from multi-line processing.
ParseResult DefaultLogParser::parseLine(std::string_view line, size_t lineNumber) const {
    return parseLineInternal(line, lineNumber);
}


// New implementation for processLine to handle multi-line log entries
std::optional<ParseResult> DefaultLogParser::processLine(std::string_view line, size_t lineNumber) {
    // If no specific start pattern is defined, treat each line as a separate entry.
    // This falls back to the behavior of the old parseLine.
    if (!logEntryStartRegex.has_value()) {
        currentLogEntryBuffer.clear();
        currentLogEntryStartLineNumber = 0;
        lastProcessedLineNumber = lineNumber;
        return parseLineInternal(line, lineNumber);
    }

    std::string lineStr(line);
    bool startsNewEntry = false;
    if (logEntryStartRegex.has_value() && std::regex_search(lineStr, *logEntryStartRegex)) {
        startsNewEntry = true;
    }

    if (startsNewEntry) {
        // If we were buffering, the buffered content forms a complete entry.
        if (!currentLogEntryBuffer.empty()) {
            ParseResult prevResult = parseLineInternal(currentLogEntryBuffer, currentLogEntryStartLineNumber);
            // Reset buffer for the new entry
            currentLogEntryBuffer = std::string(line);
            currentLogEntryStartLineNumber = lineNumber;
            lastProcessedLineNumber = lineNumber;
            return prevResult;
        } else {
            // New entry starts, but nothing was buffered. This is likely the first line.
            currentLogEntryBuffer = std::string(line);
            currentLogEntryStartLineNumber = lineNumber;
            lastProcessedLineNumber = lineNumber;
            return std::nullopt; // No complete entry yet
        }
    } else {
        // Continuation line
        if (currentLogEntryBuffer.empty()) { // If buffer is empty, and it's a continuation, this must be the start of the first entry that didn't match start pattern.
             currentLogEntryBuffer = std::string(line);
             currentLogEntryStartLineNumber = lineNumber;
        } else {
            currentLogEntryBuffer += "\n"; // Append newline for multi-line entries
            currentLogEntryBuffer += line;
        }
        lastProcessedLineNumber = lineNumber;
        return std::nullopt; // No complete entry yet
    }
}

// New implementation for flushRemaining to process any remaining buffered log entries
std::vector<ParseResult> DefaultLogParser::flushRemaining() {
    std::vector<ParseResult> results;
    if (!currentLogEntryBuffer.empty()) {
        results.push_back(parseLineInternal(currentLogEntryBuffer, currentLogEntryStartLineNumber));
        currentLogEntryBuffer.clear();
        currentLogEntryStartLineNumber = 0;
    }
    return results;
}

std::unique_ptr<ILogParser> DefaultLogParser::clone() const {
  return std::make_unique<DefaultLogParser>(patternString, fieldMappings, customLevelMappings, logEntryStartPatternString);
}

std::regex DefaultLogParser::getLineFilterRegexCompiled() const {
  if (patternString.empty()) {
    return std::regex(".*", std::regex::optimize);
  }
  return logPattern;
}
