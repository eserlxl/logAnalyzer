// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef REGEX_LOG_PARSER_FACTORY_H
#define REGEX_LOG_PARSER_FACTORY_H

#include "IParserFactory.h"
#include "core/Log/Parser.h" // For DefaultLogParser

class RegexLogParserFactory : public ILogParserFactory {
public:
    std::unique_ptr<ILogParser> createParser(const LogAnalyzerSettings& settings) override {
        // Here we use the static create method of DefaultLogParser,
        // which handles error cases during construction (e.g., invalid regex).
        // The factory should return a valid parser or throw an exception if creation fails,
        // but the design suggests ErrorCode::Result, so we'll adapt.
        // For now, let's assume DefaultLogParser::create returns a valid unique_ptr
        // or a Result type that we can unwrap or throw from if needed.
        // The LogAnalyzerSettings contain all the necessary parameters for DefaultLogParser.

        // DefaultLogParser::create returns ErrorCode::Result<std::unique_ptr<DefaultLogParser>>
        // We need to handle this result. If it's an error, we should log it and
        // return an empty unique_ptr or throw, depending on the error handling strategy.
        auto result = DefaultLogParser::create(
            settings.lineParsePattern,
            settings.fieldMappings,
            settings.customLogLevelMappings,
            settings.logEntryStartPattern,
            settings.caseSensitiveParsing, // Pass caseSensitiveParsing
            settings.parserErrorAction.value_or(ParserErrorAction::Warn),
            settings.maxMultilineBufferSize.value_or(10 * 1024 * 1024),
            false, // enableKvParsing - assuming false as default
            std::nullopt // onParseError callback
        );

        if (result.has_value()) {
            return std::move(result.value());
        } else {
            // In a real scenario, we might log the error: result.error().message
            // For now, return nullptr or rethrow a specific exception.
            // The LogAnalyzer::selectParser will need to handle this.
            // Or, we could adapt ILogParserFactory::createParser to return ErrorCode::Result<unique_ptr<ILogParser>>
            // but the design specified unique_ptr<ILogParser>.
            // Let's assume that if creation fails, a default/fallback parser might be used,
            // or an error propagated higher. For now, returning nullptr indicates failure.
            // This needs refinement based on how the LogAnalyzer will consume this.
            // The design mentioned ErrorCode::Result<void> for selectParser, implying
            // parser creation success is checked there.
            return nullptr; // Indicate failure to create parser
        }
    }

    std::string_view getFormatIdentifier() const override { return "regex"; }
};

#endif // REGEX_LOG_PARSER_FACTORY_H
