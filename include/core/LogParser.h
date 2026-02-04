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
    virtual std::string getLineFilterRegex() const { return ".*"; }
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
class DefaultLogParser : public ILogParser {
public:
    static constexpr size_t DEFAULT_MAX_BUFFER_SIZE = 10 * 1024 * 1024; // 10 MiB

    // Factory function to handle constructor errors
    static ErrorCode::Result<std::unique_ptr<DefaultLogParser>> create( // Changed to Result
        std::string pattern,
        std::vector<FieldMapping> fieldMappings,
        const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> &levelMappings = {},
        std::optional<std::string> logEntryStartPattern = std::nullopt, // Reverted to string
        CLIConfig::ParserErrorAction errorAction = CLIConfig::ParserErrorAction::Warn,
        size_t maxMultiLineBufferSize = DEFAULT_MAX_BUFFER_SIZE); // New parameter

    // New constructor with field mappings and level mappings, and optional log entry start pattern
    DefaultLogParser(
        std::string patternString, // The original pattern string
        std::regex compiledLogPattern, // The compiled pattern
        std::vector<FieldMapping> fieldMappings,
        const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> &levelMappings,
        std::optional<std::regex> compiledLogEntryStartRegex, // The compiled start regex
        std::optional<std::string> logEntryStartPatternString, // The original start regex string
        CLIConfig::ParserErrorAction errorAction,
        size_t maxMultiLineBufferSize);

    // Deprecated constructor, now delegates to the new one
    [[deprecated("Use constructor with fieldMappings for explicit control.")]]
    DefaultLogParser(
        std::string pattern
    );

    // New API for multi-line log processing
    std::optional<ErrorCode::Result<LogEntry>> processLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) override; // Changed to ErrorCode::Result<LogEntry>
    std::vector<ErrorCode::Result<LogEntry>> flushRemaining() override; // Changed to ErrorCode::Result<LogEntry>

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

    std::vector<size_t> bufferedLineNumbers;

public:
    // Public override for ILogParser::parseLine
    ErrorCode::Result<LogEntry> parseLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) const override; // Changed to ErrorCode::Result<LogEntry>

    // Internal parsing logic helper
    ErrorCode::Result<LogEntry> parseLineInternal(std::string_view line, size_t lineNumber, const std::string& sourceFile) const; // Changed to ErrorCode::Result<LogEntry>

    static const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> DEFAULT_LEVEL_MAPPINGS;

    CLIConfig::ParserErrorAction _parserErrorAction; // New: To store the error action
};
#endif // LOG_PARSER_H
