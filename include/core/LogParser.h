#ifndef LOG_PARSER_H
#define LOG_PARSER_H

#include "core/LogTypes.h" // Includes LogEntryField, FieldMapping, etc.
#include "config/CLIConfig.h" // For CLIConfig::ParserErrorAction
#include "core/Error.h" // For Error struct and Result alias
#include "core/CiLess.h" // For LogAnalyzer::LogAnalyzerInternal::ci_less
#include <functional>
#include <istream>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

// Interface for pluggable log parsers
class ILogParser {
public:
    virtual ~ILogParser() = default;
    virtual ErrorCode::Result<LogEntry> parseLine(std::string_view line,
                                  size_t lineNumber,
                                  const std::string& sourceFile) const = 0; // Added sourceFile and ErrorCode::Result<LogEntry>
    virtual std::unique_ptr<ILogParser> clone() const = 0;
    virtual std::string getLineFilterRegex() const { return ".*"; } // Returns the primary regex string used to match log lines, serving as a general line filter.
    // virtual std::regex getLineFilterRegexCompiled() const; // Removed as per design

    // New API for multi-line log processing
    // Returns an optional ErrorCode::Result<LogEntry> if a full log entry is formed.
    // Otherwise, it accumulates the line internally.
    virtual std::optional<ErrorCode::Result<LogEntry>> processLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) = 0; // Added sourceFile and ErrorCode::Result<LogEntry>

    // Call this at the end of input to get any remaining buffered log entries.
    virtual std::vector<ErrorCode::Result<LogEntry>> flushRemaining() = 0; // Changed from ParseResult to ErrorCode::Result<LogEntry>

    /**
     * @brief Processes a whole stream of log data.
     * @param inputStream The input stream to read lines from.
     * @param onEntry A callback function invoked for each parsed LogEntry (or error).
     * @param sourceFile The filename associated with the stream (for error reporting).
     */
    virtual void processStream(
        std::istream& inputStream, 
        const std::function<void(ErrorCode::Result<LogEntry>)>& onEntry,
        const std::string& sourceFile = "stream") = 0;

    // New: Returns the raw pattern string used by the parser.
    virtual std::string getPatternString() const = 0;

    // New: Returns the field mappings configured for the parser.
    virtual const std::vector<FieldMapping>& getFieldMappings() const = 0;

    // New: Returns the custom log level mappings.
    virtual const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less>& getCustomLevelMappings() const = 0;

    // New: Returns the optional pattern string used to identify the start of a log entry.
    virtual std::optional<std::string> getLogEntryStartPatternString() const = 0;

    // New: Returns the number of lines currently buffered for a multi-line entry.
    virtual size_t getCurrentBufferedLineCount() const = 0;

    // New: Returns the content currently buffered for a multi-line entry.
    virtual std::string_view getCurrentBufferedContent() const = 0;
};

// Default implementation of ILogParser using regex
//
// THIS CLASS IS NOT THREAD-SAFE.
// Instances of DefaultLogParser maintain internal state related to multi-line log entry
// processing (e.g., currentLogEntryBuffer, bufferedLineNumbers). Concurrent calls to
// state-modifying methods like processLine(), processStream(), or flushRemaining()
// on the same instance from multiple threads will lead to race conditions and
// undefined behavior.
//
// For multi-threaded environments, ensure that:
// 1. Each thread uses its own independent DefaultLogParser instance (e.g., by calling clone()).
// 2. Access to a shared DefaultLogParser instance is protected by external synchronization
//    mechanisms (e.g., mutexes).
class DefaultLogParser : public ILogParser {
public:
    static constexpr size_t DEFAULT_MAX_BUFFER_SIZE = 10 * 1024 * 1024; // 10 MiB

    // Factory function to handle constructor errors
    static ErrorCode::Result<std::unique_ptr<DefaultLogParser>> create( // Changed to Result
        std::string pattern,
        std::vector<FieldMapping> fieldMappings,
        const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> &levelMappings,
        std::optional<std::string> logEntryStartPattern, // Reverted to string
        CLIConfig::ParserErrorAction errorAction,
        size_t maxMultiLineBufferSize,
        bool enableMessageKvParsing, // New parameter: Explicitly enable legacy KV parsing in MESSAGE field
        std::optional<std::function<void(const std::string&)>> warningLogger = std::nullopt // New: Configurable warning logger
        );

    // New constructor with field mappings and level mappings, and optional log entry start pattern
    DefaultLogParser(
        std::string patternString, // The original pattern string
        std::regex compiledLogPattern, // The compiled pattern
        std::vector<FieldMapping> fieldMappings,
        const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> &levelMappings,
        std::optional<std::regex> compiledLogEntryStartRegex, // The compiled start regex
        std::optional<std::string> logEntryStartPatternString, // The original start regex string
        CLIConfig::ParserErrorAction errorAction,
        size_t maxMultiLineBufferSize,
        bool enableMessageKvParsing, // New parameter
        std::optional<std::function<void(const std::string&)>> warningLogger = std::nullopt // New parameter
        );

    // Removed deprecated constructor

    // New API for multi-line log processing
    std::optional<ErrorCode::Result<LogEntry>> processLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) override; // Changed to ErrorCode::Result<LogEntry>
    std::vector<ErrorCode::Result<LogEntry>> flushRemaining() override; // Changed to ErrorCode::Result<LogEntry>

    // processStream is not const because it calls processLine which modifies internal state.
    void processStream(
        std::istream& inputStream, 
        const std::function<void(ErrorCode::Result<LogEntry>)>& onEntry,
        const std::string& sourceFile) override;

    std::unique_ptr<ILogParser> clone() const override;
    std::string getLineFilterRegex() const override { return patternString; }
    // std::regex getLineFilterRegexCompiled() const override; // Removed

    // New ILogParser overrides for introspection
    std::string getPatternString() const override { return patternString; }
    const std::vector<FieldMapping>& getFieldMappings() const override { return fieldMappings; }
    const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less>& getCustomLevelMappings() const override { return customLevelMappings; }
    std::optional<std::string> getLogEntryStartPatternString() const override { return logEntryStartPatternString; }
    size_t getCurrentBufferedLineCount() const override { return bufferedLineNumbers.size(); }
    std::string_view getCurrentBufferedContent() const override { return currentLogEntryBuffer; }

private:
    std::regex logPattern;
    std::string patternString; // Store pattern string to allow cloning
    std::vector<FieldMapping> fieldMappings;
    std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> customLevelMappings;
    std::optional<std::regex> logEntryStartRegex; // Optional regex to identify the start of a log entry
    std::optional<std::string> logEntryStartPatternString; // For cloning

    std::string currentLogEntryBuffer;
    std::string currentLogEntrySourceFile; // New: To store the source file of the first line of a multi-line entry
    size_t currentLogEntryStartLineNumber = 0;
    size_t lastProcessedLineNumber = 0;
    size_t _maxMultiLineBufferSize;
    bool _enableMessageKvParsing; // New: Member to store the flag for MESSAGE field KV parsing
    std::optional<std::function<void(const std::string&)>> _warningLogger; // New: Configurable warning logger

    std::vector<size_t> bufferedLineNumbers;

public:
    // Public override for ILogParser::parseLine
    // parseLine is const as it performs parsing based on immutable configuration
    // and returns results without modifying the parser's internal state.
    ErrorCode::Result<LogEntry> parseLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) const override; // Changed to ErrorCode::Result<LogEntry>

    // Internal parsing logic helper
    ErrorCode::Result<LogEntry> parseLineInternal(std::string_view line, size_t lineNumber, const std::string& sourceFile) const; // Changed to ErrorCode::Result<LogEntry>

    static const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> DEFAULT_LEVEL_MAPPINGS;

    CLIConfig::ParserErrorAction _parserErrorAction; // New: To store the error action

private:
    static const std::regex& getLegacyKvPattern(); // For legacy structured field parsing

    // Helper to apply _parserErrorAction
    LogEntry applyParserErrorAction(const ErrorCode::Result<LogEntry>& parseResult,
                                    std::string_view originalLine,
                                    size_t lineNumber,
                                    const std::string& sourceFile) const;
};
#endif // LOG_PARSER_H
