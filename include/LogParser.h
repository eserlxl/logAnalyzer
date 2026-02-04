#ifndef LOG_PARSER_H
#define LOG_PARSER_H

#include "LogTypes.h" // Includes LogEntryField, FieldMapping, etc.
#include "CLIConfig.h" // For CLIConfig::ParserErrorAction
#include "Error.h" // For Error struct and Result alias
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
    virtual Result<LogEntry> parseLine(std::string_view line,
                                  size_t lineNumber,
                                  const std::string& sourceFile) const = 0; // Added sourceFile and Result<LogEntry>
    virtual std::unique_ptr<ILogParser> clone() const = 0;
    virtual std::string getLineFilterRegex() const { return ".*"; }
    // virtual std::regex getLineFilterRegexCompiled() const; // Removed as per design

    // New API for multi-line log processing
    // Returns an optional Result<LogEntry> if a full log entry is formed.
    // Otherwise, it accumulates the line internally.
    virtual std::optional<Result<LogEntry>> processLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) = 0; // Added sourceFile and Result<LogEntry>

    // Call this at the end of input to get any remaining buffered log entries.
    virtual std::vector<Result<LogEntry>> flushRemaining() = 0; // Changed from ParseResult to Result<LogEntry>

    // New: Returns the raw pattern string used by the parser.
    virtual std::string getPatternString() const = 0;

    // New: Returns the field mappings configured for the parser.
    virtual const std::vector<FieldMapping>& getFieldMappings() const = 0;

    // New: Returns the custom log level mappings.
    virtual const std::map<std::string, LogLevel, ci_less>& getCustomLevelMappings() const = 0;

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
    // Factory function to handle constructor errors
    static Result<std::unique_ptr<DefaultLogParser>> create( // Changed to Result
        std::string pattern,
        std::vector<FieldMapping> fieldMappings,
        const std::map<std::string, LogLevel, ci_less> &levelMappings = {},
        std::optional<std::string> logEntryStartPattern = std::nullopt,
        CLIConfig::ParserErrorAction errorAction = CLIConfig::ParserErrorAction::Warn); // Added errorAction

    // New constructor with field mappings and level mappings, and optional log entry start pattern
    DefaultLogParser(
        std::string pattern,
        std::vector<FieldMapping> fieldMappings,
        const std::map<std::string, LogLevel, ci_less> &levelMappings = {},
        std::optional<std::string> logEntryStartPattern = std::nullopt,
        CLIConfig::ParserErrorAction errorAction = CLIConfig::ParserErrorAction::Warn); // Added errorAction

    // Deprecated constructor, now delegates to the new one
    [[deprecated("Use constructor with fieldMappings for explicit control.")]]
    DefaultLogParser(
        std::string pattern
    );

    // New API for multi-line log processing
    std::optional<Result<LogEntry>> processLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) override; // Changed to Result<LogEntry>
    std::vector<Result<LogEntry>> flushRemaining() override; // Changed to Result<LogEntry>

    std::unique_ptr<ILogParser> clone() const override;
    std::string getLineFilterRegex() const override { return patternString; }
    // std::regex getLineFilterRegexCompiled() const override; // Removed

    // New ILogParser overrides for introspection
    std::string getPatternString() const override { return patternString; }
    const std::vector<FieldMapping>& getFieldMappings() const override { return fieldMappings; }
    const std::map<std::string, LogLevel, ci_less>& getCustomLevelMappings() const override { return customLevelMappings; }
    std::optional<std::string> getLogEntryStartPatternString() const override { return logEntryStartPatternString; }
    size_t getCurrentBufferedLineCount() const override { return bufferedLineNumbers.size(); }
    std::string_view getCurrentBufferedContent() const override { return currentLogEntryBuffer; }

private:
    std::regex logPattern;
    std::string patternString; // Store pattern string to allow cloning
    std::vector<FieldMapping> fieldMappings;
    std::map<std::string, LogLevel, ci_less> customLevelMappings;
    std::optional<std::regex> logEntryStartRegex; // Optional regex to identify the start of a log entry
    std::optional<std::string> logEntryStartPatternString; // For cloning

    std::string currentLogEntryBuffer;
    size_t currentLogEntryStartLineNumber = 0;
    size_t lastProcessedLineNumber = 0;

    std::vector<size_t> bufferedLineNumbers;

    // Public override for ILogParser::parseLine
    Result<LogEntry> parseLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) const override; // Changed to Result<LogEntry>

    // Internal parsing logic helper
    Result<LogEntry> parseLineInternal(std::string_view line, size_t lineNumber, const std::string& sourceFile) const; // Changed to Result<LogEntry>

    static const std::map<std::string, LogLevel, ci_less> DEFAULT_LEVEL_MAPPINGS;

    CLIConfig::ParserErrorAction _parserErrorAction; // New: To store the error action
};
#endif // LOG_PARSER_H
