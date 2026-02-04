#include "LogParser.h"
#include "LogTypes.h"
#include "Error.h"
#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// using namespace ErrorCode; // Removed global using directive

std::vector<FieldMapping> getDefaultFieldMappings() {
    std::vector<FieldMapping> inferredMappings;
    inferredMappings.push_back(FieldMapping(LogEntryField::TIMESTAMP, 1, std::string("%Y-%m-%d %H:%M:%S")));
    inferredMappings.push_back(FieldMapping(LogEntryField::LEVEL, 2));
    inferredMappings.push_back(FieldMapping(LogEntryField::MESSAGE, 3));
    return inferredMappings;
}

Result<std::unique_ptr<DefaultLogParser>> DefaultLogParser::create(
    std::string pattern,
    std::vector<FieldMapping> fieldMappings,
    const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less>& levelMappings,
    std::optional<std::string> logEntryStartPatternString_param, // The original string pattern
    CLIConfig::ParserErrorAction errorAction) {

    // 1. Compile logPattern
    std::regex compiledLogPattern;
    try {
        compiledLogPattern = std::regex(pattern, std::regex::optimize);
    } catch (const std::regex_error& e) {
        return std::unexpected(Error(Error::Code::InvalidRegex, "Invalid log pattern: " + std::string(e.what())));
    }

    // 2. Compile logEntryStartRegex if provided
    std::optional<std::regex> compiledLogEntryStartRegex;
    if (logEntryStartPatternString_param.has_value()) {
        try {
            compiledLogEntryStartRegex = std::regex(logEntryStartPatternString_param.value(), std::regex::optimize);
        } catch (const std::regex_error& e) {
            return std::unexpected(Error(Error::Code::InvalidRegex, "Invalid log entry start pattern: " + std::string(e.what())));
        }
    }

    // 3. Pre-compile structured field regexes (Medium-risk issue #1)
    for (auto& mapping : fieldMappings) {
        if (std::holds_alternative<LogEntryField>(mapping.field) &&
            std::get<LogEntryField>(mapping.field) == LogEntryField::STRUCTURED_FIELD) {
            std::string delimiter;
            if (!mapping.formats.empty() && !mapping.formats[0].empty()) {
                delimiter = mapping.formats[0];
            } else {
                delimiter = "="; // Default delimiter if not specified
            }
            std::string pattern_str = "([\\w.-]+)\\s*" + delimiter + "\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s,]+))";
            try {
                mapping.compiledKvPattern = std::regex(pattern_str);
            } catch (const std::regex_error& e) {
                return std::unexpected(Error(Error::Code::InvalidRegex, "Invalid structured field pattern for delimiter '" + delimiter + "': " + std::string(e.what())));
            }
        }
    }

    // Now call the constructor with pre-compiled regexes
    return std::make_unique<DefaultLogParser>(
        std::move(pattern), // Original pattern string (for cloning/introspection)
        std::move(compiledLogPattern), // Compiled main log pattern
        std::move(fieldMappings),
        levelMappings,
        std::move(compiledLogEntryStartRegex), // Compiled log entry start regex
        std::move(logEntryStartPatternString_param), // Original log entry start pattern string (for cloning/introspection)
        errorAction
    );
}

const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> DefaultLogParser::DEFAULT_LEVEL_MAPPINGS = {
    {"TRACE", LogLevel::TRACE},
    {"DEBUG", LogLevel::DEBUG},
    {"INFO", LogLevel::INFO},
    {"WARNING", LogLevel::WARNING},
    {"ERROR", LogLevel::ERROR},
    {"FATAL", LogLevel::FATAL}
};

DefaultLogParser::DefaultLogParser(
    std::string patternString_param, // Original pattern string for cloning
    std::regex compiledLogPattern, // Already compiled regex
    std::vector<FieldMapping> fieldMappings_param,
    const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> &levelMappings,
    std::optional<std::regex> compiledLogEntryStartRegex, // Already compiled regex
    std::optional<std::string> logEntryStartPatternString_param, // Original string for cloning
    CLIConfig::ParserErrorAction errorAction)
    : logPattern(std::move(compiledLogPattern)),
      patternString(std::move(patternString_param)),
      fieldMappings(std::move(fieldMappings_param)),
      customLevelMappings(levelMappings),
      logEntryStartRegex(std::move(compiledLogEntryStartRegex)),
      logEntryStartPatternString(std::move(logEntryStartPatternString_param)),
      _parserErrorAction(errorAction)
{}

DefaultLogParser::DefaultLogParser(
    std::string pattern)
    : DefaultLogParser(
        pattern, // patternString
        std::regex(pattern, std::regex::optimize), // compiledLogPattern
        getDefaultFieldMappings(), // fieldMappings
        {}, // levelMappings
        std::nullopt, // compiledLogEntryStartRegex
        std::nullopt, // logEntryStartPatternString
        CLIConfig::ParserErrorAction::Warn) // errorAction
{}

Result<LogEntry> DefaultLogParser::parseLineInternal(std::string_view line, size_t lineNumber, const std::string& sourceFile) const {
  LogEntry entry;
  entry.id = lineNumber;
  entry.sourceLineNumber = lineNumber;
  entry.sourceFile = sourceFile;
  entry.level = LogLevel::UNKNOWN;

  if (patternString.empty()) {
    return std::unexpected(Error(Error::Code::MalformedLogEntry, "No regex pattern provided to parser."));
  }

  std::string lineStr(line);
  if (!lineStr.empty() && lineStr.back() == '\r') {
      lineStr.pop_back();
  }
  std::smatch match;

  if (!std::regex_match(lineStr, match, logPattern)) {
    return std::unexpected(Error(Error::Code::MalformedLogEntry, "Line does not match log pattern."));
  }

  bool timestampParsingFailed = false;

  bool customFieldsExplicitlyMapped = false;
  for (const auto& mapping : fieldMappings) {
      if (std::holds_alternative<LogEntryField>(mapping.field) &&
          std::get<LogEntryField>(mapping.field) == LogEntryField::STRUCTURED_FIELD) {
          customFieldsExplicitlyMapped = true;
          break;
      }
  }

  for (const auto& mapping : fieldMappings) {
    if (!mapping.groupIndex.has_value() || mapping.groupIndex.value() >= match.size() || !match[mapping.groupIndex.value()].matched) {
        continue;
    }
    std::string capturedValue = match[mapping.groupIndex.value()].str();

    std::visit(
        [&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, LogEntryField>) {
                switch (arg) {
                    case LogEntryField::TIMESTAMP: {
                        auto parsedTime = Utils::parseTimeWithFormats(capturedValue, mapping.formats);
                        if(parsedTime) {
                            entry.timestamp = *parsedTime;
                        } else {
                            timestampParsingFailed = true; // High-risk #1: Set flag if parsing fails
                        }
                        break;
                    }
                    case LogEntryField::LEVEL: {
                        entry.level = Utils::stringToLogLevel(capturedValue, customLevelMappings);
                        break;
                    }
                    case LogEntryField::MESSAGE: {
                        entry.message = capturedValue;
                        if (!customFieldsExplicitlyMapped) {
                            // This part still uses legacy parsing, consider if this should also be pre-compiled or removed.
                            // For now, addressing the structured_field explicit mapping.
                            const std::regex kvPattern_legacy("([\\w.-]+)\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s,.]+))");
                            auto words_begin = std::sregex_iterator(entry.message.begin(), entry.message.end(), kvPattern_legacy);
                            auto words_end = std::sregex_iterator();
                            for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
                                std::smatch kvMatch = *i;
                                std::string k = kvMatch[1].str();
                                std::string v = kvMatch[2].matched ? kvMatch[2].str() : (kvMatch[3].matched ? kvMatch[3].str() : kvMatch[4].str());
                                entry.customFields[k] = v;
                            }
                        }
                        break;
                    }
                    case LogEntryField::SOURCE_FILE: {
                        entry.sourceFile = capturedValue;
                        break;
                    }
                    case LogEntryField::STRUCTURED_FIELD: {
                        // Medium-risk #1: Use pre-compiled kvPattern
                        if (mapping.compiledKvPattern.has_value()) {
                            const std::regex& kvPattern = mapping.compiledKvPattern.value();
                            auto kv_begin = std::sregex_iterator(capturedValue.begin(), capturedValue.end(), kvPattern);
                            auto kv_end = std::sregex_iterator();
                            for (std::sregex_iterator i = kv_begin; i != kv_end; ++i) {
                                std::smatch kvMatch = *i;
                                std::string k = kvMatch[1].str();
                                std::string v = kvMatch[2].matched ? kvMatch[2].str() : (kvMatch[3].matched ? kvMatch[3].str() : kvMatch[4].str());
                                entry.customFields[k] = v;
                            }
                        } else {
                            // Fallback or error if compiledKvPattern is missing (should not happen if `create` is correctly implemented)
                            // For robustness, could fall back to on-the-fly compilation or default regex, but ideally this is an assert.
                            // For now, let's assume `create` ensures `compiledKvPattern` is present for STRUCTURED_FIELD.
                        }
                        break;
                    }
                    default:
                        break;
                }
            } else if constexpr (std::is_same_v<T, std::string>) {
                entry.customFields[arg] = capturedValue;
            }
        },
        mapping.field);
  }

  if (timestampParsingFailed) {
      return std::unexpected(Error(Error::Code::TimestampParsingFailed, "Failed to parse timestamp in line: '" + lineStr + "' for file: " + sourceFile + " at line: " + std::to_string(lineNumber)));
  }

  return entry;
}
Result<LogEntry> DefaultLogParser::parseLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) const {
    Result<LogEntry> result = parseLineInternal(line, lineNumber, sourceFile);
    if (!result.has_value()) {
        const Error& error = result.error();
        if (_parserErrorAction == CLIConfig::ParserErrorAction::Warn) {
            std::cerr << "Warning (LogParser): " << error.message << std::endl;
            // Return a default-constructed LogEntry with basic info
            LogEntry defaultEntry;
            defaultEntry.id = lineNumber;
            defaultEntry.sourceLineNumber = lineNumber;
            defaultEntry.sourceFile = sourceFile;
            defaultEntry.level = LogLevel::UNKNOWN; // Default level
            defaultEntry.message = "Parse failed (warn): " + std::string(line); // Include original line for context
            // Timestamp will be optional and thus not set for this default entry if parsing failed
            return defaultEntry;
        } else if (_parserErrorAction == CLIConfig::ParserErrorAction::Throw) {
            throw error;
        } else if (_parserErrorAction == CLIConfig::ParserErrorAction::Ignore) {
            // Return a default-constructed LogEntry, effectively ignoring the error.
            // The caller will receive a valid, but possibly empty/incomplete, LogEntry.
            LogEntry defaultEntry;
            defaultEntry.id = lineNumber;
            defaultEntry.sourceLineNumber = lineNumber;
            defaultEntry.sourceFile = sourceFile;
            defaultEntry.level = LogLevel::UNKNOWN;
            defaultEntry.message = "Parse ignored: " + std::string(line);
            // Timestamp will be optional and thus not set for this default entry if parsing failed
            return defaultEntry;
        }
    }
    return result;
}

std::optional<Result<LogEntry>> DefaultLogParser::processLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) {
    if (!logEntryStartRegex.has_value()) {
        currentLogEntryBuffer.clear();
        bufferedLineNumbers.clear();
        lastProcessedLineNumber = lineNumber;
        // Delegate to parseLine for single-line handling, which already has _parserErrorAction logic
        // parseLine returns Result<LogEntry>, processLine expects optional<Result<LogEntry>>
        return std::make_optional(parseLine(line, lineNumber, sourceFile));
    }

    std::string lineStr(line);
    bool startsNewEntry = std::regex_search(lineStr, *logEntryStartRegex);

    if (startsNewEntry) {
        if (!currentLogEntryBuffer.empty()) {
            // Process the buffered entry first
            Result<LogEntry> prevResult = parseLineInternal(currentLogEntryBuffer, currentLogEntryStartLineNumber, currentLogEntrySourceFile);

            std::optional<Result<LogEntry>> returnOptionalResult = std::nullopt;

            if (!prevResult.has_value()) {
                const Error& error = prevResult.error();
                if (_parserErrorAction == CLIConfig::ParserErrorAction::Warn) {
                    std::cerr << "Warning (LogParser): " << error.message << std::endl;
                    LogEntry defaultEntry;
                    defaultEntry.id = currentLogEntryStartLineNumber;
                    defaultEntry.sourceLineNumber = currentLogEntryStartLineNumber;
                    defaultEntry.sourceFile = currentLogEntrySourceFile;
                    defaultEntry.level = LogLevel::UNKNOWN;
                    defaultEntry.message = "Parse failed (warn): " + currentLogEntryBuffer;
                    returnOptionalResult = std::make_optional(defaultEntry); // Return default entry on warn
                } else if (_parserErrorAction == CLIConfig::ParserErrorAction::Throw) {
                    throw error;
                } else if (_parserErrorAction == CLIConfig::ParserErrorAction::Ignore) {
                    // Do nothing, effectively ignore the previous entry, so returnOptionalResult remains std::nullopt
                } else {
                     returnOptionalResult = std::make_optional(prevResult); // Propagate the unexpected error
                }
            } else {
                returnOptionalResult = std::make_optional(prevResult); // Previous entry was successful
            }

            // Start new entry for the current line
            currentLogEntryBuffer = lineStr;
            currentLogEntryStartLineNumber = lineNumber;
            currentLogEntrySourceFile = sourceFile; // Update source file
            bufferedLineNumbers = {lineNumber};
            lastProcessedLineNumber = lineNumber;

            return returnOptionalResult;
        } else {
            // This is the first line of the very first log entry or the first after an ignored error
            currentLogEntryBuffer = lineStr;
            currentLogEntryStartLineNumber = lineNumber;
            currentLogEntrySourceFile = sourceFile; // Store source file
            bufferedLineNumbers = {lineNumber};
            lastProcessedLineNumber = lineNumber;
            return std::nullopt; // No full entry yet
        }
    } else {
        // Accumulate lines
        if (currentLogEntryBuffer.empty()) {
             currentLogEntryBuffer = lineStr;
             currentLogEntryStartLineNumber = lineNumber;
             currentLogEntrySourceFile = sourceFile; // Store source file for first line of multi-line
             bufferedLineNumbers.push_back(lineNumber);
        } else {
            currentLogEntryBuffer += "\n";
            currentLogEntryBuffer += line;
            bufferedLineNumbers.push_back(lineNumber);
        }
        lastProcessedLineNumber = lineNumber;
        return std::nullopt; // No full entry yet
    }
}

std::vector<Result<LogEntry>> DefaultLogParser::flushRemaining() {
    std::vector<Result<LogEntry>> results;
    if (!currentLogEntryBuffer.empty()) {
        Result<LogEntry> finalResult = parseLineInternal(currentLogEntryBuffer, currentLogEntryStartLineNumber, currentLogEntrySourceFile);

        if (!finalResult.has_value()) {
            const Error& error = finalResult.error();
            if (_parserErrorAction == CLIConfig::ParserErrorAction::Warn) {
                std::cerr << "Warning (LogParser - flushRemaining): " << error.message << std::endl;
                LogEntry defaultEntry;
                defaultEntry.id = currentLogEntryStartLineNumber;
                defaultEntry.sourceLineNumber = currentLogEntryStartLineNumber;
                defaultEntry.sourceFile = currentLogEntrySourceFile;
                defaultEntry.level = LogLevel::UNKNOWN;
                defaultEntry.message = "Parse failed (warn): " + currentLogEntryBuffer;
                results.push_back(defaultEntry);
            } else if (_parserErrorAction == CLIConfig::ParserErrorAction::Throw) {
                throw error;
            } else if (_parserErrorAction == CLIConfig::ParserErrorAction::Ignore) {
                // Ignore, so don't add anything to results.
            } else {
                 results.push_back(finalResult); // Push the unexpected error if not handled by Warn/Throw/Ignore
            }
        } else {
            results.push_back(finalResult); // Push successful result
        }

        currentLogEntryBuffer.clear();
        currentLogEntryStartLineNumber = 0;
        currentLogEntrySourceFile.clear(); // Clear source file on flush
        bufferedLineNumbers.clear();
    }
    return results;
}

std::unique_ptr<ILogParser> DefaultLogParser::clone() const {
  return std::make_unique<DefaultLogParser>(
      patternString,             // Original pattern string
      logPattern,                // Already compiled regex
      fieldMappings,
      customLevelMappings,
      logEntryStartRegex,        // Already compiled regex
      logEntryStartPatternString, // Original log entry start pattern string
      _parserErrorAction);
}
