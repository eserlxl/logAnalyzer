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
    virtual std::regex getLineFilterRegexCompiled() const; // Add back missing declaration
};

// Default implementation of ILogParser using regex
class DefaultLogParser : public ILogParser {
public:
    // New constructor with field mappings and level mappings
    DefaultLogParser(
        std::string pattern,
        std::vector<FieldMapping> fieldMappings,
        const std::map<std::string, LogLevel, ci_less> &levelMappings = {});

    // Deprecated constructor, now delegates to the new one
    [[deprecated("Use constructor with fieldMappings for explicit control.")]]
    DefaultLogParser(
        std::string pattern = ""
    );

    ParseResult parseLine(std::string_view line,
                          size_t lineNumber) const override;
    std::unique_ptr<ILogParser> clone() const override;
    std::string getLineFilterRegex() const override { return patternString; }
    std::regex getLineFilterRegexCompiled() const override;

private:
    std::regex logPattern;
    std::string patternString; // Store pattern string to allow cloning
    std::vector<FieldMapping> fieldMappings; // Renamed from fieldMappings_
    std::map<std::string, LogLevel, ci_less> customLevelMappings;

    static const std::map<std::string, LogLevel, ci_less> DEFAULT_LEVEL_MAPPINGS;
};

#endif // LOG_PARSER_H
