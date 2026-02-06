// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef JSON_LOG_PARSER_FACTORY_H
#define JSON_LOG_PARSER_FACTORY_H

#include "ILogParserFactory.h"
#include "JsonLogParser.h" // For JsonLogParser

class JsonLogParserFactory : public ILogParserFactory {
public:
    std::unique_ptr<ILogParser> createParser(const LogAnalyzerSettings& settings) override {
        // JsonLogParser doesn't use all settings, primarily customLogLevelMapping and parserErrorAction
        return std::make_unique<JsonLogParser>(
            settings.customLogLevelMappings,
            settings.parserErrorAction.value_or(ParserErrorAction::Warn)
        );
    }

    std::string_view getFormatIdentifier() const override { return "json"; }
};

#endif // JSON_LOG_PARSER_FACTORY_H
