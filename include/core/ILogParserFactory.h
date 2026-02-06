// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef I_LOG_PARSER_FACTORY_H
#define I_LOG_PARSER_FACTORY_H

#include <memory>
#include <string_view>

#include "core/LogParser.h" // For ILogParser
#include "config/Core.h" // For LogAnalyzerSettings

class ILogParserFactory {
public:
    virtual ~ILogParserFactory() = default;
    virtual std::unique_ptr<ILogParser> createParser(const LogAnalyzerSettings& settings) = 0;
    virtual std::string_view getFormatIdentifier() const = 0; // e.g., "regex", "json", "syslog"
};

#endif // I_LOG_PARSER_FACTORY_H
