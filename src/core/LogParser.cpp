// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 eserlxl

#include "core/LogParser.h"
#include "core/LogTypes.h"
#include "core/Error.h"
#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// Removed global using directive

// Definition of the static member function for legacy kv pattern
const std::regex& DefaultLogParser::getLegacyKvPattern() {
    static const std::regex pattern("([\\w.-]+)\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s,.]+))");
    return pattern;
}

std::vector<FieldMapping> getDefaultFieldMappings() {
    std::vector<FieldMapping> inferredMappings;
    inferredMappings.push_back(FieldMapping(LogEntryField::TIMESTAMP, 1, std::string("%Y-%m-%d %H:%M:%S")));
    inferredMappings.push_back(FieldMapping(LogEntryField::LEVEL, 2));
    inferredMappings.push_back(FieldMapping(LogEntryField::MESSAGE, 3));
    return inferredMappings;
}

ErrorCode::Result<std::unique_ptr<DefaultLogParser>> DefaultLogParser::create(
    std::string pattern,
    std::vector<FieldMapping> fieldMappings,
    const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less>& levelMappings,
    std::optional<std::string> logEntryStartPattern,
    CLIConfig::ParserErrorAction errorAction,
    size_t maxMultiLineBufferSize,
    bool enableMessageKvParsing, // New parameter
    std::optional<std::function<void(const std::string&)>> warningLogger) { // New parameter
    
    // Compile main log pattern
    std::regex compiledLogPattern;
    try {
        compiledLogPattern = std::regex(pattern);
    } catch (const std::regex_error& e) {
        return std::unexpected(ErrorCode::Error(Code::InvalidRegex, "Invalid log pattern: " + std::string(e.what())));
    }

    // Compile optional log entry start pattern
    std::optional<std::regex> compiledLogEntryStartRegex;
    if (logEntryStartPattern.has_value()) {
        try {
            compiledLogEntryStartRegex = std::regex(logEntryStartPattern.value());
        } catch (const std::regex_error& e) {
            return std::unexpected(ErrorCode::Error(Code::InvalidRegex, "Invalid log entry start pattern: " + std::string(e.what())));
        }
    }

    // Compile regex for structured fields if present in mappings
    for (auto& mapping : fieldMappings) {
        if (std::holds_alternative<LogEntryField>(mapping.field) &&
            std::get<LogEntryField>(mapping.field) == LogEntryField::STRUCTURED_FIELD &&
            !mapping.formats.empty()) { // Check if formats has a delimiter pattern
            try {
                mapping.compiledKvPattern = std::regex(mapping.formats[0]); // Use the first format as the delimiter pattern
            } catch (const std::regex_error& e) {
                return std::unexpected(ErrorCode::Error(Code::InvalidRegex, "Invalid structured field delimiter pattern: " + std::string(e.what())));
            }
        }
    }

    // Create a new parser instance and return it
    return std::make_unique<DefaultLogParser>(
        std::move(pattern),
        std::move(compiledLogPattern),
        std::move(fieldMappings),
        levelMappings,
        std::move(compiledLogEntryStartRegex),
        std::move(logEntryStartPattern),
        errorAction,
        maxMultiLineBufferSize,
        enableMessageKvParsing, // Pass new parameter
        std::move(warningLogger)); // Pass new parameter
}

// Definition of static DEFAULT_LEVEL_MAPPINGS
const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> DefaultLogParser::DEFAULT_LEVEL_MAPPINGS = {
    {"trace", LogLevel::TRACE},
    {"debug", LogLevel::DEBUG},
    {"info", LogLevel::INFO},
    {"warn", LogLevel::WARNING},
    {"warning", LogLevel::WARNING},
    {"error", LogLevel::ERROR},
    {"critical", LogLevel::CRITICAL},
    {"fatal", LogLevel::FATAL}
};

// Full constructor
DefaultLogParser::DefaultLogParser(
    std::string patternString,
    std::regex compiledLogPattern,
    std::vector<FieldMapping> fieldMappings,
    const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less>& levelMappings,
    std::optional<std::regex> compiledLogEntryStartRegex,
    std::optional<std::string> logEntryStartPatternString,
    CLIConfig::ParserErrorAction errorAction,
    size_t maxMultiLineBufferSize,
    bool enableMessageKvParsing, // New parameter
    std::optional<std::function<void(const std::string&)>> warningLogger) // New parameter
    : logPattern(std::move(compiledLogPattern)),
      patternString(std::move(patternString)),
      fieldMappings(std::move(fieldMappings)),
      customLevelMappings(levelMappings), // Map is copied
      logEntryStartRegex(std::move(compiledLogEntryStartRegex)),
      logEntryStartPatternString(std::move(logEntryStartPatternString)),
      _maxMultiLineBufferSize(maxMultiLineBufferSize),
      _enableMessageKvParsing(enableMessageKvParsing), // Initialize new member
      _warningLogger(std::move(warningLogger)), // Initialize new member
      _parserErrorAction(errorAction)
{
    currentLogEntryStartLineNumber = 0;
    lastProcessedLineNumber = 0;
}

void DefaultLogParser::processStream(
    std::istream& inputStream, 
    const std::function<void(ErrorCode::Result<LogEntry>)>& onEntry,
    const std::string& sourceFile) {
    
    std::string line;
    size_t lineNumber = 0;
    while (std::getline(inputStream, line)) {
        lineNumber++;
        if (auto entryResultOpt = processLine(line, lineNumber, sourceFile)) {
            onEntry(std::move(*entryResultOpt));
        }
    }
    // Don't forget to flush the last buffered entry!
    for (auto& finalEntryResult : flushRemaining()) {
        onEntry(std::move(finalEntryResult));
    }
}

ErrorCode::Result<LogEntry> DefaultLogParser::parseLineInternal(std::string_view line, size_t lineNumber, const std::string& sourceFile) const {
  LogEntry entry;
  entry.id = lineNumber;
  entry.sourceLineNumber = lineNumber;
  entry.sourceFile = sourceFile;
  entry.level = LogLevel::UNKNOWN;

  if (patternString.empty()) {
    return std::unexpected(ErrorCode::Error(::Code::MalformedLogEntry, "No regex pattern provided to parser."));
  }

  std::string lineStr(line);
  if (!lineStr.empty() && lineStr.back() == '\r') {
      lineStr.pop_back();
  }
  std::smatch match;

  if (!std::regex_match(lineStr, match, logPattern)) {
    return std::unexpected(ErrorCode::Error(::Code::MalformedLogEntry, "Line does not match log pattern."));
  }

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
                            entry.parsingErrors.push_back(ErrorCode::Error(::Code::TimestampParsingFailed, "Failed to parse timestamp from '" + capturedValue + "' with formats."));
                        }
                        break;
                    }
                    case LogEntryField::LEVEL: {
                        entry.level = Utils::stringToLogLevel(capturedValue, customLevelMappings);
                        break;
                    }
                    case LogEntryField::MESSAGE: {
                        entry.message = capturedValue;
                        // High-risk #2: Ambiguity in LogEntryField::MESSAGE parsing fallback
                        // Only perform legacy KV parsing if explicitly enabled AND no STRUCTURED_FIELD was explicitly mapped.
                        if (_enableMessageKvParsing && !customFieldsExplicitlyMapped) {
                            auto words_begin = std::sregex_iterator(entry.message.begin(), entry.message.end(), DefaultLogParser::getLegacyKvPattern());
                            auto words_end = std::sregex_iterator();
                            for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
                                std::smatch kvMatch = *i;
                                std::string k = kvMatch[1].str();
                                // Medium-risk #2: Readability of KV Value Extraction
                                std::string v;
                                if (kvMatch[2].matched) {
                                    v = kvMatch[2].str();
                                } else if (kvMatch[3].matched) {
                                    v = kvMatch[3].str();
                                } else if (kvMatch[4].matched) {
                                    v = kvMatch[4].str();
                                }
                                entry.customFields[k] = v;
                            }
                        }
                        break;
                    }
                    case LogEntryField::STRUCTURED_FIELD: {
                        if (mapping.compiledKvPattern.has_value()) {
                            const std::regex& kvPattern = mapping.compiledKvPattern.value();
                            auto kv_begin = std::sregex_iterator(capturedValue.begin(), capturedValue.end(), kvPattern);
                            auto kv_end = std::sregex_iterator();
                            for (std::sregex_iterator i = kv_begin; i != kv_end; ++i) {
                                std::smatch kvMatch = *i;
                                std::string k = kvMatch[1].str();
                                // Medium-risk #2: Readability of KV Value Extraction
                                std::string v;
                                if (kvMatch[2].matched) {
                                    v = kvMatch[2].str();
                                } else if (kvMatch[3].matched) {
                                    v = kvMatch[3].str();
                                } else if (kvMatch[4].matched) {
                                    v = kvMatch[4].str();
                                }
                                entry.customFields[k] = v;
                            }
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

        // The function now always returns the entry if the main regex matches.

        // The caller (parseLine) is responsible for checking entry.hasParsingErrors()

        // and deciding on the action.

        return entry;

      }

      

            // Helper function to centralize error handling logic

      

            LogEntry DefaultLogParser::applyParserErrorAction(const ErrorCode::Result<LogEntry>& parseResult,

      

                                                            std::string_view originalLine,

      

                                                            size_t lineNumber,

      

                                                            const std::string& sourceFile) const {

      

                if (parseResult.has_value()) {

      

                    const LogEntry& entry = parseResult.value();

      

                    if (entry.hasParsingErrors()) {

      

                        // These are sub-parsing errors (e.g., timestamp parsing failed)

      

                        switch (_parserErrorAction) {

      

                            case CLIConfig::ParserErrorAction::Warn: {

      

                                std::string warningMsg = "Warning (LogParser): " + entry.getParsingErrorsAsString() + " for line: '" + std::string(originalLine) + "' in file: " + sourceFile + " at line: " + std::to_string(lineNumber);

      

                                if (_warningLogger) {

      

                                    _warningLogger.value()(warningMsg);

      

                                } else {

      

                                    std::cerr << warningMsg << '\n';

      

                                }

      

                                return entry; // Return the entry with errors, but it was logged as a warning

      

                            }

      

                            case CLIConfig::ParserErrorAction::Throw: {

      

                                // Throw the first parsing error found

      

                                throw entry.parsingErrors[0];

      

                            }

      

                            case CLIConfig::ParserErrorAction::Ignore: {

      

                                // Return a new LogEntry representing the ignored line, or the original entry if partial parsing is acceptable

      

                                LogEntry ignoredEntry;

      

                                ignoredEntry.id = lineNumber;

      

                                ignoredEntry.sourceLineNumber = lineNumber;

      

                                ignoredEntry.sourceFile = sourceFile;

      

                                ignoredEntry.level = LogLevel::UNKNOWN;

      

                                ignoredEntry.message = "Parse ignored (sub-errors): " + std::string(originalLine);

      

                                ignoredEntry.parsingErrors = entry.parsingErrors; // Retain specific error details

      

                                return ignoredEntry;

      

                            }

      

                        }

      

                    }

      

                    return entry; // No parsing errors, return as is

      

                } else {

      

                    // This is a complete failure (e.g., regex mismatch)

      

                    const ErrorCode::Error& error = parseResult.error();

      

                    switch (_parserErrorAction) {

      

                        case CLIConfig::ParserErrorAction::Warn: {

      

                            std::string warningMsg = "Warning (LogParser): " + error.message + " for line: '" + std::string(originalLine) + "' in file: " + sourceFile + " at line: " + std::to_string(lineNumber);

      

                            if (_warningLogger) {

      

                                _warningLogger.value()(warningMsg);

      

                            } else {

      

                                std::cerr << warningMsg << '\n';

      

                            }

      

                            // Return a default-constructed LogEntry with basic info

      

                            LogEntry defaultEntry;

      

                            defaultEntry.id = lineNumber;

      

                            defaultEntry.sourceLineNumber = lineNumber;

      

                            defaultEntry.sourceFile = sourceFile;

      

                            defaultEntry.level = LogLevel::UNKNOWN;

      

                            defaultEntry.message = "Parse failed (warn): " + std::string(originalLine);

      

                            defaultEntry.parsingErrors.push_back(error); // Retain specific error details

      

                            return defaultEntry;

      

                        }

      

                        case CLIConfig::ParserErrorAction::Throw: {

      

                            throw error;

      

                        }

      

                        case CLIConfig::ParserErrorAction::Ignore: {

      

                            // Return a default-constructed LogEntry, effectively ignoring the error.

      

                            LogEntry defaultEntry;

      

                            defaultEntry.id = lineNumber;

      

                            defaultEntry.sourceLineNumber = lineNumber;

      

                            defaultEntry.sourceFile = sourceFile;

      

                            defaultEntry.level = LogLevel::UNKNOWN;

      

                            defaultEntry.message = "Parse ignored: " + std::string(originalLine);

      

                            defaultEntry.parsingErrors.push_back(error); // Retain specific error details

      

                            return defaultEntry;

      

                        }

      

                    }

      

                }

      

                // Should not be reached

      

                throw ErrorCode::Error(::Code::Unexpected, "Unhandled parser error action in applyParserErrorAction.");

      

            }

          

              

          ErrorCode::Result<LogEntry> DefaultLogParser::parseLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) const {
    ErrorCode::Result<LogEntry> result = parseLineInternal(line, lineNumber, sourceFile);
    return applyParserErrorAction(result, line, lineNumber, sourceFile);
}

    

    

std::optional<ErrorCode::Result<LogEntry>> DefaultLogParser::processLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) {
    if (!logEntryStartRegex.has_value()) {
        currentLogEntryBuffer.clear();
        bufferedLineNumbers.clear();
        lastProcessedLineNumber = lineNumber;
        // Delegate to parseLine for single-line handling, which already has _parserErrorAction logic
        // parseLine returns ErrorCode::Result<LogEntry>, processLine expects optional<ErrorCode::Result<LogEntry>>
        return std::make_optional(parseLine(line, lineNumber, sourceFile));
    }

    std::string lineStr(line);

    // Check if adding this line would exceed the buffer limit
    if (!currentLogEntryBuffer.empty() && (currentLogEntryBuffer.size() + lineStr.size() + 1 > _maxMultiLineBufferSize)) {
        // The current line will cause the buffer to exceed its limit if appended.
        // So, we finalize the *current* buffered content as a log entry, marking it as truncated.
        ErrorCode::Result<LogEntry> resultToEmit = parseLine(currentLogEntryBuffer, currentLogEntryStartLineNumber, currentLogEntrySourceFile);

        if (resultToEmit.has_value()) {
            resultToEmit.value().level = LogLevel::WARNING; // Mark as warning for a truncated entry
            resultToEmit.value().parsingErrors.insert(
                resultToEmit.value().parsingErrors.begin(),
                ErrorCode::Error(::Code::BufferLimitExceeded, "Multi-line log entry truncated due to buffer limit (" + std::to_string(_maxMultiLineBufferSize) + " bytes). This line was not appended."));
            resultToEmit.value().message.clear(); // Clear the message as per test expectation
        } else {
            // If even the buffered content couldn't be parsed, create a new error entry that combines the original parsing error
            // with the buffer limit exceeded information.
            LogEntry combinedErrorEntry;
            combinedErrorEntry.id = currentLogEntryStartLineNumber;
            combinedErrorEntry.sourceLineNumber = currentLogEntryStartLineNumber;
            combinedErrorEntry.sourceFile = currentLogEntrySourceFile;
            combinedErrorEntry.level = LogLevel::ERROR;
            combinedErrorEntry.message = "Parsing failed for multi-line content due to: " + resultToEmit.error().message + ". Additionally, buffer limit (" + std::to_string(_maxMultiLineBufferSize) + " bytes) was exceeded.";
            combinedErrorEntry.parsingErrors.push_back(
                ErrorCode::Error(::Code::BufferLimitExceeded, "Buffer limit (" + std::to_string(_maxMultiLineBufferSize) + " bytes) was exceeded."));
            combinedErrorEntry.parsingErrors.push_back(resultToEmit.error()); // Include the original parsing error

            resultToEmit = combinedErrorEntry; // Overwrite with the new combined error LogEntry
        }

        // After processing and emitting the truncated entry, reset the buffer for the *current* line.
        // This 'lineStr' will start a new potential multi-line entry.
        currentLogEntryBuffer = lineStr;
        currentLogEntryStartLineNumber = lineNumber;
        currentLogEntrySourceFile = sourceFile;
        bufferedLineNumbers = {lineNumber};
        lastProcessedLineNumber = lineNumber;

        // Return the `ErrorCode::Result<LogEntry>` for the truncated content.
        return std::make_optional(std::move(resultToEmit));
    }

    bool startsNewEntry = std::regex_search(lineStr, *logEntryStartRegex);

    if (startsNewEntry) {
        if (!currentLogEntryBuffer.empty()) {
            // Process the buffered entry first
            ErrorCode::Result<LogEntry> prevResult = parseLine(currentLogEntryBuffer, currentLogEntryStartLineNumber, currentLogEntrySourceFile);

            // Start new entry for the current line
            currentLogEntryBuffer = lineStr;
            currentLogEntryStartLineNumber = lineNumber;
            currentLogEntrySourceFile = sourceFile; // Update source file
            bufferedLineNumbers = {lineNumber};
            lastProcessedLineNumber = lineNumber;

            return std::make_optional(prevResult);
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

std::vector<ErrorCode::Result<LogEntry>> DefaultLogParser::flushRemaining() {
    std::vector<ErrorCode::Result<LogEntry>> results;
    if (!currentLogEntryBuffer.empty()) {
        ErrorCode::Result<LogEntry> finalResult = parseLine(currentLogEntryBuffer, currentLogEntryStartLineNumber, currentLogEntrySourceFile);
        results.push_back(finalResult);

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
      _parserErrorAction,
      _maxMultiLineBufferSize,
      _enableMessageKvParsing,   // Pass new parameter
      _warningLogger);           // Pass new parameter
}
