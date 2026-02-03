#ifndef LOG_PARSER_H
#define LOG_PARSER_H

#include "LogTypes.h" // Includes LogEntryField, FieldMapping, etc.
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
    virtual ParseResult parseLine(std::string_view line,
                                  size_t lineNumber) const = 0;
    virtual std::unique_ptr<ILogParser> clone() const = 0;
    virtual std::string getLineFilterRegex() const { return ".*"; }
    virtual std::regex getLineFilterRegexCompiled() const;

    // New API for multi-line log processing
    // Returns an optional ParseResult if a full log entry is formed.
    // Otherwise, it accumulates the line internally.
    virtual std::optional<ParseResult> processLine(std::string_view line, size_t lineNumber) = 0;

    // Call this at the end of input to get any remaining buffered log entries.
    virtual std::vector<ParseResult> flushRemaining() = 0;
};

// Default implementation of ILogParser using regex
class DefaultLogParser : public ILogParser {
public:
    // New constructor with field mappings and level mappings, and optional log entry start pattern
    DefaultLogParser(
        std::string pattern,
        std::vector<FieldMapping> fieldMappings,
        const std::map<std::string, LogLevel, ci_less> &levelMappings = {},
        std::optional<std::string> logEntryStartPattern = std::nullopt);

    // Deprecated constructor, now delegates to the new one
    [[deprecated("Use constructor with fieldMappings for explicit control.")]]
    DefaultLogParser(
        std::string pattern
    );

    // New API for multi-line log processing
    std::optional<ParseResult> processLine(std::string_view line, size_t lineNumber) override;
    std::vector<ParseResult> flushRemaining() override;

    std::unique_ptr<ILogParser> clone() const override;
    std::string getLineFilterRegex() const override { return patternString; }
    std::regex getLineFilterRegexCompiled() const override;

private:
    std::regex logPattern;
    std::string patternString; // Store pattern string to allow cloning
    std::vector<FieldMapping> fieldMappings;
    std::map<std::string, LogLevel, ci_less> customLevelMappings;
    std::optional<std::regex> logEntryStartRegex; // Optional regex to identify the start of a log entry
    std::optional<std::string> logEntryStartPatternString; // For cloning

    std::string currentLogEntryBuffer;
    size_t currentLogEntryStartLineNumber = 0;
    size_t lastProcessedLineNumber = 0; // ADDED: To keep track of the last line processed for multi-line context

    // Public override for ILogParser::parseLine
    ParseResult parseLine(std::string_view line, size_t lineNumber) const override;

    // Internal parsing logic helper
    ParseResult parseLineInternal(std::string_view line, size_t lineNumber) const;

    static const std::map<std::string, LogLevel, ci_less> DEFAULT_LEVEL_MAPPINGS;
};

#endif // LOG_PARSER_H
