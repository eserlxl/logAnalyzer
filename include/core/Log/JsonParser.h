// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef JSON_LOG_PARSER_H
#define JSON_LOG_PARSER_H

#include "core/Log/Parser.h" // For ILogParser
#include <nlohmann/json.hpp> // For JSON parsing

class JsonLogParser : public ILogParser {
public:
    JsonLogParser(const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less>& levelMappings,
                  CLIConfig::ParserErrorAction errorAction,
                  std::optional<std::function<void(const std::string&)>> warningLogger = std::nullopt);

    // ILogParser overrides
    ErrorCode::Result<LogEntry> parseLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) const override;
    std::unique_ptr<ILogParser> clone() const override;
    std::string getLineFilterRegex() const override { return ".*"; } // JSON lines don't typically need filtering by regex

    std::optional<ErrorCode::Result<LogEntry>> processLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) override;
    std::vector<ErrorCode::Result<LogEntry>> flushRemaining() override;

    void processStream(std::istream& inputStream, 
                       const std::function<void(ErrorCode::Result<LogEntry>)>& onEntry,
                       const std::string& sourceFile) override;

    // These are not applicable for a pure JSON parser that doesn't use regex patterns
    std::string getPatternString() const override { return "json"; }
    const std::vector<FieldMapping>& getFieldMappings() const override { return emptyFieldMappings_; }
    const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less>& getCustomLevelMappings() const override { return customLevelMappings_; }
    std::optional<std::string> getLogEntryStartPatternString() const override { return std::nullopt; }
    size_t getCurrentBufferedLineCount() const override { return 0; } // JSON parsing is line-by-line, no buffering
    std::string_view getCurrentBufferedContent() const override { return ""; } // No buffering

private:
    const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> customLevelMappings_;
    CLIConfig::ParserErrorAction parserErrorAction_;
    std::optional<std::function<void(const std::string&)>> warningLogger_;
    std::vector<FieldMapping> emptyFieldMappings_; // JSON parser doesn't use traditional FieldMappings

    // Helper to apply parserErrorAction
    LogEntry applyParserErrorAction(const ErrorCode::Result<LogEntry>& parseResult,
                                    std::string_view originalLine,
                                    size_t lineNumber,
                                    const std::string& sourceFile) const;
};

#endif // JSON_LOG_PARSER_H
