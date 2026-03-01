// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef REGEX_LOG_PARSER_H
#define REGEX_LOG_PARSER_H

#include "core/log/parser.h"
#include "config/common_types.h" // For FieldMapping
#include <string>
#include <vector>
#include <map>
#include <regex>

class RegexLogParser : public ILogParser {
public:
    RegexLogParser(const std::string& pattern,
                   const std::vector<FieldMapping>& fieldMappings,
                   const std::map<std::string, LogLevel, LogAnalyzerInternal::CaseInsensitiveLess>& levelMappings,
                   ParserErrorAction errorAction);

    ErrorCode::Result<LogEntry> parseLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) const override;
    std::unique_ptr<ILogParser> clone() const override;

private:
    std::string patternString;
    std::regex logRegex;
    std::vector<FieldMapping> fieldMappings;
    std::map<std::string, LogLevel, LogAnalyzerInternal::CaseInsensitiveLess> customLevelMappings;
    ParserErrorAction parserErrorAction;
};

#endif // REGEX_LOG_PARSER_H
