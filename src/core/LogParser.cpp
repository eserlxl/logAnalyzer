// Default implementation of ILogParser using regex
#include "core/LogParser.h"

namespace Utils {
    // Forward declarations for functions defined later in this file.
    void parseStructuredData(const std::string& data, std::map<std::string, std::string>& targetMap, const std::regex& kvPattern);
    void parseLegacyStructuredData(const std::string& message, std::map<std::string, std::string>& targetMap);
}

#include "utils/Time.h"
#include "utils/String.h"
#include "config/CLIConfig.h" // For CLIConfig::ParserErrorAction
#include <iostream>
#include <stdexcept>

namespace LogAnalyzerInternal {
    // Definition of the static member for case-insensitive comparison
    const ci_less ci_less_instance = {};
}

// Static default log level mappings
const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> DefaultLogParser::DEFAULT_LEVEL_MAPPINGS = {
    {"TRACE", LogLevel::TRACE},
    {"DEBUG", LogLevel::DEBUG},
    {"INFO", LogLevel::INFO},
    {"WARN", LogLevel::WARNING},
    {"WARNING", LogLevel::WARNING},
    {"ERROR", LogLevel::ERROR},
    {"CRITICAL", LogLevel::CRITICAL},
    {"FATAL", LogLevel::FATAL}
};

// Factory function to handle constructor errors
ErrorCode::Result<std::unique_ptr<DefaultLogParser>> DefaultLogParser::create(
    std::string pattern,
    std::vector<FieldMapping> fieldMappings,
    const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less>& levelMappings,
    std::optional<std::string> logEntryStartPattern,
    std::optional<bool> caseSensitive,
    CLIConfig::ParserErrorAction errorAction,
    size_t maxMultiLineBufferSize,
    bool enableMessageKvParsing,
    std::optional<std::function<void(const std::string&)>> warningLogger) {

    std::regex compiledLogPattern;
    try {
        std::regex::flag_type flags = std::regex::optimize;
        if (caseSensitive.has_value() && !caseSensitive.value()) {
            flags |= std::regex::icase;
        }
        compiledLogPattern = std::regex(pattern, flags);
    } catch (const std::regex_error& e) {
        return std::unexpected(ErrorCode::Error(Code::InvalidRegex, "Invalid main regex pattern: " + std::string(e.what())));
    }

    std::optional<std::regex> compiledLogEntryStartRegex;
    if (logEntryStartPattern) {
        try {
            std::regex::flag_type startFlags = std::regex::optimize;
            if (caseSensitive.has_value() && !caseSensitive.value()) {
                startFlags |= std::regex::icase;
            }
            compiledLogEntryStartRegex = std::regex(*logEntryStartPattern, startFlags);
        } catch (const std::regex_error& e) {
            return std::unexpected(ErrorCode::Error(Code::InvalidRegex, "Invalid log entry start regex pattern: " + std::string(e.what())));
        }
    }

    return std::make_unique<DefaultLogParser>(
        std::move(pattern),
        std::move(compiledLogPattern),
        std::move(fieldMappings),
        levelMappings,
        std::move(compiledLogEntryStartRegex),
        std::move(logEntryStartPattern),
        caseSensitive,
        errorAction,
        maxMultiLineBufferSize,
        enableMessageKvParsing,
        std::move(warningLogger)
    );
}

// Constructor implementation
DefaultLogParser::DefaultLogParser(
    std::string patternString,
    std::regex compiledLogPattern,
    std::vector<FieldMapping> fieldMappings,
    const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less>& levelMappings,
    std::optional<std::regex> compiledLogEntryStartRegex,
    std::optional<std::string> logEntryStartPatternString,
    std::optional<bool> caseSensitive,
    CLIConfig::ParserErrorAction errorAction,
    size_t maxMultiLineBufferSize,
    bool enableMessageKvParsing,
    std::optional<std::function<void(const std::string&)>> warningLogger
) :
    logPattern(std::move(compiledLogPattern)),
    patternString(std::move(patternString)),
    fieldMappings(std::move(fieldMappings)),
    customLevelMappings(levelMappings), // Copy
    logEntryStartRegex(std::move(compiledLogEntryStartRegex)),
    logEntryStartPatternString(std::move(logEntryStartPatternString)),
    caseSensitive(caseSensitive),
    _maxMultiLineBufferSize(maxMultiLineBufferSize),
    _enableMessageKvParsing(enableMessageKvParsing),
    _warningLogger(std::move(warningLogger)),
    _parserErrorAction(errorAction)
{
    // Pre-compile Kv patterns for structured fields
    for (auto& mapping : this->fieldMappings) {
        if (std::holds_alternative<LogEntryField>(mapping.field) && std::get<LogEntryField>(mapping.field) == LogEntryField::STRUCTURED_FIELD) {
            if (!mapping.formats.empty() && !mapping.formats[0].empty()) {
                try {
                    mapping.compiledKvPattern = std::make_shared<const std::regex>(mapping.formats[0], std::regex::optimize);
                } catch (const std::regex_error& e) {
                    if (_warningLogger) {
                        _warningLogger.value()("Warning: Invalid regex pattern for STRUCTURED_FIELD in FieldMapping: " + mapping.formats[0] + " - " + e.what());
                    } else {
                        std::cerr << "Warning: Invalid regex pattern for STRUCTURED_FIELD in FieldMapping: " << mapping.formats[0] << " - " << e.what() << std::endl;
                    }
                    // Continue without the pattern, it will be handled as plain structured data.
                    mapping.compiledKvPattern.reset();
                }
            }
        }
    }
}

// Clone method implementation
std::unique_ptr<ILogParser> DefaultLogParser::clone() const {
    // Use the factory method to recreate a new parser instance
    // This handles potential errors during regex compilation if patterns were invalid.
    auto result = DefaultLogParser::create(
        patternString,
        fieldMappings,
        customLevelMappings,
        logEntryStartPatternString, // Pass the original string pattern for start regex
        caseSensitive,
        _parserErrorAction,
        _maxMultiLineBufferSize,
        _enableMessageKvParsing,
        _warningLogger
    );

    if (result) {
        // Transfer internal state if cloning is for multi-line parsing in a new context
        // For simple cloning, this might not be strictly necessary if the clone starts fresh.
        // However, if the intent is to resume parsing from a buffered state, then that state
        // needs to be copied/moved. For now, assume a fresh parser.
        auto newParser = std::move(result.value());
        newParser->currentLogEntryBuffer = currentLogEntryBuffer;
        newParser->currentLogEntrySourceFile = currentLogEntrySourceFile;
        newParser->currentLogEntryStartLineNumber = currentLogEntryStartLineNumber;
        newParser->lastProcessedLineNumber = lastProcessedLineNumber;
        newParser->bufferedLineNumbers = bufferedLineNumbers; // Copy buffered line numbers
        return newParser;
    } else {
        // Handle error during cloning, e.g., log it or throw an exception
        // For now, let's just return a nullptr or rethrow for simplicity if `create` throws.
        // Given `create` returns Result, we should handle the error case.
        if (_warningLogger) {
             _warningLogger.value()("Error during cloning DefaultLogParser: " + result.error().message);
        } else {
            std::cerr << "Error during cloning DefaultLogParser: " << result.error().message << std::endl;
        }
        return nullptr; // Or throw an exception if appropriate for cloning failures
    }
}


// Internal parsing logic
ErrorCode::Result<LogEntry> DefaultLogParser::parseLineInternal(std::string_view line, size_t lineNumber, const std::string& sourceFile) const {
    std::smatch matches;
    std::string s_line(line); // Convert string_view to string for std::regex_match

    if (!std::regex_match(s_line, matches, logPattern)) {
        return std::unexpected(ErrorCode::Error(Code::MalformedLogEntry, "Line does not match log pattern (Line: " + std::to_string(lineNumber) + ")"));
    }

    LogEntry entry;
    entry.sourceFile = sourceFile;
    entry.sourceLineNumber = lineNumber;

    for (const auto& mapping : fieldMappings) {
        size_t groupIndex = 0;
        if (mapping.groupIndex.has_value()) {
            groupIndex = mapping.groupIndex.value();
            if (groupIndex == 0 || groupIndex >= matches.size()) {
                // Ignore if groupIndex is 0 (full match) or out of bounds for the current line's matches
                // For a LogEntryField::CUSTOM, this will mean it won't be mapped from a regex group,
                // which is acceptable for custom fields if they're derived otherwise.
                if (std::holds_alternative<std::string>(mapping.field)) {
                    // Custom fields don't strictly require a groupIndex if they are to be derived or set differently
                    continue;
                } else if (std::get<LogEntryField>(mapping.field) != LogEntryField::CUSTOM) {
                     entry.parsingErrors.emplace_back(ErrorCode::Error(Code::FieldNotFound, "Regex group index " + std::to_string(groupIndex) + " out of bounds for " + Utils::logEntryFieldToString(std::get<LogEntryField>(mapping.field)) + " (Line: " + std::to_string(lineNumber) + ")"));
                     continue; // Skip this mapping if group index is invalid
                }
            }
        } else {
            // No group index specified, cannot extract from regex for standard fields
            if (std::holds_alternative<LogEntryField>(mapping.field) && std::get<LogEntryField>(mapping.field) != LogEntryField::CUSTOM) {
                entry.parsingErrors.emplace_back(ErrorCode::Error(Code::FieldNotFound, "No regex group index specified for " + Utils::logEntryFieldToString(std::get<LogEntryField>(mapping.field)) + " (Line: " + std::to_string(lineNumber) + ")"));
                continue; // Skip this mapping
            }
        }
        
        std::string extractedValue;
        if (mapping.groupIndex.has_value()) {
            extractedValue = matches[groupIndex].str();
        }

        if (std::holds_alternative<LogEntryField>(mapping.field)) {
            LogEntryField field = std::get<LogEntryField>(mapping.field);
            switch (field) {
                case LogEntryField::TIMESTAMP: {
                    if (extractedValue.empty() && mapping.groupIndex.has_value()) { // Only consider empty if it was supposed to be extracted
                        entry.parsingErrors.emplace_back(ErrorCode::Error(Code::TimestampParsingFailed, "Timestamp field empty (Line: " + std::to_string(lineNumber) + ")"));
                        break;
                    }
        if (auto result = Utils::parseTime(std::string(extractedValue)); result.has_value()) {
            entry.timestamp = result.value();
        } else {
            entry.parsingErrors.emplace_back(result.error());
        }
                    break;
                }
                case LogEntryField::LEVEL: {
                    entry.level = Utils::stringToLogLevel(std::string(extractedValue), customLevelMappings);
                    if (entry.level == LogLevel::UNKNOWN) {
                        entry.parsingErrors.emplace_back(ErrorCode::Error(Code::MalformedLogEntry, "Unknown log level: " + std::string(extractedValue) + " (Line: " + std::to_string(lineNumber) + ")"));
                    }
                    break;
                }
                case LogEntryField::MESSAGE:
                    entry.message = std::string(extractedValue);
                    if (_enableMessageKvParsing) {
                        Utils::parseLegacyStructuredData(entry.message, entry.customFields);
                    }
                    break;
                case LogEntryField::THREAD_ID:
                    entry.threadId = std::string(extractedValue);
                    break;
                case LogEntryField::SOURCE_FILE:
                    // Note: This mapping is only for extracting from regex, typically sourceFile is set directly.
                    // We'll prioritize the direct sourceFile over regex extraction here if both are present.
                    if (sourceFile == "stream" || sourceFile == "stdin") { // Only overwrite if source is not a real file
                        entry.sourceFile = std::string(extractedValue);
                    }
                    break;
                case LogEntryField::LINE_NUMBER: {
                    if (!extractedValue.empty()) {
                        try {
                            entry.sourceLineNumber = std::stoul(std::string(extractedValue));
                        } catch (const std::exception& e) {
                            entry.parsingErrors.emplace_back(ErrorCode::Error(Code::ConversionError, "Failed to convert line number to unsigned long: " + std::string(e.what()), std::to_string(lineNumber)));
                        }
                    }
                    break;
                }
                case LogEntryField::MODULE:
                    entry.module = std::string(extractedValue);
                    break;
                case LogEntryField::HOST:
                    entry.host = std::string(extractedValue);
                    break;
                case LogEntryField::STRUCTURED_FIELD: {
                    entry.structuredData = std::string(extractedValue);
                    if (mapping.compiledKvPattern) {
                        Utils::parseStructuredData(entry.structuredData.value(), entry.customFields, *mapping.compiledKvPattern);
                    } else {
                        // Fallback for unstructured structuredData if no pattern is compiled
                        // e.g., if kv pattern was invalid or not provided.
                        // Can add a default structured data parser here if desired.
                    }
                    break;
                }
                case LogEntryField::ID:
                    // ID is usually assigned externally, not parsed from a line.
                    // If needed, implement parsing logic here.
                    break;
                case LogEntryField::CUSTOM:
                    // Custom fields as LogEntryField::CUSTOM should not happen,
                    // they should be handled by the std::string alternative.
                    break;
                case LogEntryField::UNKNOWN:
                    entry.parsingErrors.emplace_back(ErrorCode::Error(Code::MalformedLogEntry, "Mapping to UNKNOWN field type", std::to_string(lineNumber)));
                    break;
            }
        } else { // std::holds_alternative<std::string>(mapping.field)
            entry.customFields[std::get<std::string>(mapping.field)] = std::string(extractedValue);
        }
    }
    
    return entry;
}

// Public override for ILogParser::parseLine
ErrorCode::Result<LogEntry> DefaultLogParser::parseLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) const {
    auto result = parseLineInternal(line, lineNumber, sourceFile);
    if (result.has_value()) {
        return result;
    }

    if (_parserErrorAction == CLIConfig::ParserErrorAction::Throw) {
        throw ErrorCode::Error(result.error());
    }

    return applyParserErrorAction(result, line, lineNumber, sourceFile);
}

// Helper to apply _parserErrorAction
LogEntry DefaultLogParser::applyParserErrorAction(const ErrorCode::Result<LogEntry>& parseResult,
                                                std::string_view originalLine,
                                                size_t lineNumber,
                                                const std::string& sourceFile) const {
    if (parseResult.has_value()) {
        return parseResult.value();
    }

    // Handle error based on _parserErrorAction
    if (_parserErrorAction == CLIConfig::ParserErrorAction::Throw) {
        throw std::runtime_error(parseResult.error().message); // Re-throw the error
    }

    LogEntry partialEntry;
    partialEntry.level = LogLevel::UNKNOWN;
    if (parseResult.error().code == Code::BufferLimitExceeded && !currentLogEntryBuffer.empty()) {
        partialEntry.message = currentLogEntryBuffer;
    } else {
        if (_parserErrorAction == CLIConfig::ParserErrorAction::Ignore) {
            partialEntry.message = "Parse ignored: " + std::string(originalLine);
        } else { // Warn
            partialEntry.message = "Parse failed (warn): " + std::string(originalLine);
        }
    }
    partialEntry.sourceFile = sourceFile;
    partialEntry.sourceLineNumber = lineNumber;
    partialEntry.parsingErrors.push_back(parseResult.error());

    if (_parserErrorAction == CLIConfig::ParserErrorAction::Warn) {
        if (_warningLogger) {
            _warningLogger.value()("Warning: Failed to parse line " + std::to_string(lineNumber) + " in " + sourceFile + ": " + parseResult.error().message);
        } else {
            std::cerr << "Warning: Failed to parse line " << lineNumber << " in " << sourceFile << ": " << parseResult.error().message << std::endl;
        }
    }
    // If action is Ignore or Warn, return the partial entry
    return partialEntry;
}


// New API for multi-line log processing
std::optional<ErrorCode::Result<LogEntry>> DefaultLogParser::processLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) {
    std::optional<ErrorCode::Result<LogEntry>> result;
    bool isNewEntryStart = false;

    if (logEntryStartRegex) {
        // If a start pattern is defined, check if the current line marks a new entry.
        isNewEntryStart = std::regex_search(std::string(line), *logEntryStartRegex);
    } else {
        // If no explicit start pattern, assume each line is a new entry unless it clearly
        // continues the previous one (e.g., doesn't match the main log pattern).
        // For simplicity and robustness, if no start pattern, every line is a potential new entry.
        // We'll rely on parseLineInternal to validate if it's a valid log entry.
        // If currentLogEntryBuffer is empty, it's always a new entry.
        // If it's not empty, and no start regex, we need a way to decide if the current line
        // is a continuation or a new log. The simplest is to treat every line that matches
        // the main pattern as a new entry.
        // If the buffer is not empty and we receive a new line, it means the previous one
        // was a single line entry OR the new line is a continuation that doesn't trigger
        // a new log.
        isNewEntryStart = currentLogEntryBuffer.empty() || std::regex_search(std::string(line), logPattern);
    }

    if (isNewEntryStart && !currentLogEntryBuffer.empty()) {
        // If it's a new entry start and we have a buffered entry, flush the buffered one.
        result = parseLineInternal(currentLogEntryBuffer, currentLogEntryStartLineNumber, currentLogEntrySourceFile);
        
        // Reset buffer and prepare for new entry
        currentLogEntryBuffer.clear();
        bufferedLineNumbers.clear();
        currentLogEntryStartLineNumber = 0;
        currentLogEntrySourceFile.clear();
    }

    if (currentLogEntryBuffer.empty() && isNewEntryStart) {
        // This is the start of a new log entry.
        currentLogEntryBuffer = std::string(line);
        bufferedLineNumbers.push_back(lineNumber);
        currentLogEntryStartLineNumber = lineNumber;
        currentLogEntrySourceFile = sourceFile;
    } else {
        // This line is a continuation of the current buffered entry.
        if (!currentLogEntryBuffer.empty()) {
            currentLogEntryBuffer += "\n" + std::string(line);
            bufferedLineNumbers.push_back(lineNumber);
        } else {
            // This case should ideally not happen if logic is sound, but as a fallback,
            // treat it as a new entry if the buffer is somehow empty but not marked as new start.
            currentLogEntryBuffer = std::string(line);
            bufferedLineNumbers.push_back(lineNumber);
            currentLogEntryStartLineNumber = lineNumber;
            currentLogEntrySourceFile = sourceFile;
        }
    }

    // Check buffer size to prevent excessive memory usage
    if (currentLogEntryBuffer.length() > _maxMultiLineBufferSize) {
        if (_warningLogger) {
            _warningLogger.value()("Warning: Multi-line log buffer exceeded maximum size (" + std::to_string(_maxMultiLineBufferSize) + " bytes). Flushing partial entry.");
        } else {
            std::cerr << "Warning: Multi-line log buffer exceeded maximum size (" << _maxMultiLineBufferSize << " bytes). Flushing partial entry." << std::endl;
        }
                // Force flush the current buffer as an error
                ErrorCode::Error bufferError(Code::BufferLimitExceeded, "Multi-line log entry truncated due to buffer limit", std::to_string(currentLogEntryStartLineNumber));
                result = std::unexpected(bufferError);
        
                // Reset the buffer to contain ONLY the current line that caused the overflow
                currentLogEntryBuffer = std::string(line);
                bufferedLineNumbers.clear();
                bufferedLineNumbers.push_back(lineNumber);
                currentLogEntryStartLineNumber = lineNumber;
                currentLogEntrySourceFile = sourceFile;
            }
    lastProcessedLineNumber = lineNumber;
    return result;
}

std::vector<ErrorCode::Result<LogEntry>> DefaultLogParser::flushRemaining() {
    std::vector<ErrorCode::Result<LogEntry>> flushedEntries;
    if (!currentLogEntryBuffer.empty()) {
        flushedEntries.push_back(parseLine(currentLogEntryBuffer, currentLogEntryStartLineNumber, currentLogEntrySourceFile));
        currentLogEntryBuffer.clear();
        bufferedLineNumbers.clear();
        currentLogEntryStartLineNumber = 0;
        currentLogEntrySourceFile.clear();
    }
    return flushedEntries;
}

void DefaultLogParser::processStream(
    std::istream& inputStream, 
    const std::function<void(ErrorCode::Result<LogEntry>)>& onEntry,
    const std::string& sourceFile)
{
    std::string line;
    size_t lineNumber = 0;
    while (std::getline(inputStream, line)) {
        lineNumber++;
        auto optionalResult = processLine(line, lineNumber, sourceFile);
        if (optionalResult) {
            onEntry(*optionalResult);
        }
    }
    // Flush any remaining buffered log entries at the end of the stream
    for (auto& result : flushRemaining()) {
        onEntry(result);
    }
}

// Define the static constant for legacy KV pattern
const std::regex& DefaultLogParser::getLegacyKvPattern() {
    static const std::regex kvPattern("([a-zA-Z0-9_.-]+)\\s*=\\s*(?:\"(.*?)\"|'([^']*)'|([^\\s,]+))[, ]*", std::regex::optimize);
    return kvPattern;
}





namespace Utils {
    // Function to parse structured data from a string into a map
    void parseStructuredData(const std::string& data, std::map<std::string, std::string>& targetMap, const std::regex& kvPattern) {
        std::sregex_iterator next(data.begin(), data.end(), kvPattern);
        std::sregex_iterator end;
        while (next != end) {
            std::smatch match = *next;
            std::string key = match[1].str();
            std::string value;

            // Check for quoted values (groups 2 and 3) or unquoted (group 4)
            if (match[2].matched) { // Double quotes
                value = match[2].str();
            } else if (match[3].matched) { // Single quotes
                value = match[3].str();
            } else if (match[4].matched) { // Unquoted
                value = match[4].str();
            }
            targetMap[key] = value;
            next++;
        }
    }

    void parseLegacyStructuredData(const std::string& message, std::map<std::string, std::string>& targetMap) {
        // Re-use the new structured data parser with the legacy pattern
        parseStructuredData(message, targetMap, DefaultLogParser::getLegacyKvPattern());
    }
} // namespace Utils


