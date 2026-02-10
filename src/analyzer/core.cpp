// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer/core.h"
#include "analyzer/log/reader.h"
#include "analyzer/log/writer.h"
#include "core/log/parser.h"
#include "filter/core.h"
#include "stats/core.h"
#include "export/core.h"
#include "utils/core.h"
#include "core/error.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <regex>
#include <iomanip>
#include <sstream>
#include <memory> 
#include <vector>
#include <utility>
#include <future>
#include <functional>
#include <iterator>
#include <map>
#include <mutex>
#include <expected>
#include <string_view>
#include <span>

#include "config/core.h" // Renamed from config/Core.h


LogAnalyzer::LogAnalyzer()
    : currentSettings_(),
      customLogLevelMapping_(currentSettings_.customLogLevelMappings), // Initialize with settings' mappings
      currentParser_(nullptr), // Initialize to nullptr, then assign in body
      logReader_(std::make_unique<LogReader>(*this)),
      logWriter_(std::make_unique<LogWriter>(*this))
{
    auto parser_or_error = DefaultLogParser::create(
        currentSettings_.lineParsePattern,
        currentSettings_.fieldMappings,
        customLogLevelMapping_,
        currentSettings_.logEntryStartPattern,
        currentSettings_.caseSensitiveParsing,
        currentSettings_.parserErrorAction.value_or(ParserErrorAction::Warn),
        currentSettings_.maxMultilineBufferSize.value_or(DefaultLogParser::DEFAULT_MAX_BUFFER_SIZE),
        true,
        std::nullopt
    );

    if (parser_or_error) {
        currentParser_ = std::move(*parser_or_error);
    } else {
        // Fallback to known-good defaults so construction remains no-throw for user input issues.
        std::cerr << "Warning: Failed to initialize parser with current settings in default constructor: "
                  << parser_or_error.error().message << ". Falling back to built-in defaults." << '\n';
        currentSettings_ = LogAnalyzerSettings{};
        customLogLevelMapping_ = currentSettings_.customLogLevelMappings;
        auto fallback_or_error = DefaultLogParser::create(
            currentSettings_.lineParsePattern,
            currentSettings_.fieldMappings,
            customLogLevelMapping_,
            currentSettings_.logEntryStartPattern,
            currentSettings_.caseSensitiveParsing,
            currentSettings_.parserErrorAction.value_or(ParserErrorAction::Warn),
            currentSettings_.maxMultilineBufferSize.value_or(DefaultLogParser::DEFAULT_MAX_BUFFER_SIZE),
            true,
            std::nullopt
        );
        if (!fallback_or_error) {
            std::cerr << "Fatal Error: Failed to initialize fallback parser in default constructor: "
                      << fallback_or_error.error().message << '\n';
            // Keep object constructible; operational APIs return structured errors
            // when parser initialization is unavailable.
            currentParser_.reset();
            return;
        }
        currentParser_ = std::move(*fallback_or_error);
    }

    for (const auto& config : currentSettings_.statisticConfigs) {
        if (auto collector = createStatisticCollector(config)) {
            collectors_.push_back(collector);
        }
    }
}
LogAnalyzer::LogAnalyzer(const LogAnalyzerSettings& settings)
    : currentSettings_(settings), // Initialize currentSettings_ with provided settings
      customLogLevelMapping_(settings.customLogLevelMappings), // Initialize customLogLevelMapping_ from settings
      currentParser_(nullptr), // Initialize to nullptr, then assign in body
      logReader_(std::make_unique<LogReader>(*this)),
      logWriter_(std::make_unique<LogWriter>(*this))
{
    auto parser_or_error = DefaultLogParser::create(
        currentSettings_.lineParsePattern,
        currentSettings_.fieldMappings,
        customLogLevelMapping_,
        currentSettings_.logEntryStartPattern,
        currentSettings_.caseSensitiveParsing,
        currentSettings_.parserErrorAction.value_or(ParserErrorAction::Warn),
        currentSettings_.maxMultilineBufferSize.value_or(DefaultLogParser::DEFAULT_MAX_BUFFER_SIZE),
        true,
        std::nullopt
    );

    if (parser_or_error) {
        currentParser_ = std::move(*parser_or_error);
    } else {
        std::cerr << "Warning: Failed to initialize parser in settings constructor: "
                  << parser_or_error.error().message << ". Falling back to built-in defaults." << '\n';
        currentSettings_ = LogAnalyzerSettings{};
        customLogLevelMapping_ = currentSettings_.customLogLevelMappings;
        auto fallback_or_error = DefaultLogParser::create(
            currentSettings_.lineParsePattern,
            currentSettings_.fieldMappings,
            customLogLevelMapping_,
            currentSettings_.logEntryStartPattern,
            currentSettings_.caseSensitiveParsing,
            currentSettings_.parserErrorAction.value_or(ParserErrorAction::Warn),
            currentSettings_.maxMultilineBufferSize.value_or(DefaultLogParser::DEFAULT_MAX_BUFFER_SIZE),
            true,
            std::nullopt
        );
        if (!fallback_or_error) {
            std::cerr << "Fatal Error: Failed to initialize fallback parser in settings constructor: "
                      << fallback_or_error.error().message << '\n';
            // Keep object constructible; operational APIs return structured errors
            // when parser initialization is unavailable.
            currentParser_.reset();
            return;
        }
        currentParser_ = std::move(*fallback_or_error);
    }

    for (const auto& config : currentSettings_.statisticConfigs) {
        if (auto collector = createStatisticCollector(config)) {
            collectors_.push_back(collector);
        }
    }
}

LogAnalyzer::~LogAnalyzer() {
    // Wait for any pending async tasks to complete.
    std::unique_lock<std::mutex> lock(pendingAsyncTasksMutex_);
    for (auto& future : pendingAsyncTasks_) {
        if (future.valid()) {
            future.wait();
        }
    }
}

ErrorCode::Result<void> LogAnalyzer::setSettings(const LogAnalyzerSettings& settings) {
    auto newCustomLogLevelMapping = settings.customLogLevelMappings;
    std::vector<std::shared_ptr<IStatisticCollector>> newCollectors;
    newCollectors.reserve(settings.statisticConfigs.size());
    for (const auto& config : settings.statisticConfigs) {
        if (auto collector = createStatisticCollector(config)) {
            newCollectors.push_back(collector);
        }
    }

    auto parser_or_error = DefaultLogParser::create(
        settings.lineParsePattern,
        settings.fieldMappings,
        newCustomLogLevelMapping,
        settings.logEntryStartPattern,
        settings.caseSensitiveParsing,
        settings.parserErrorAction.value_or(ParserErrorAction::Warn),
        settings.maxMultilineBufferSize.value_or(DefaultLogParser::DEFAULT_MAX_BUFFER_SIZE),
        true, // threadSafe: LogAnalyzer should use a thread-safe parser
        std::nullopt // errorHandler: No specific error handler for now, default to internal logging
    );
    if (parser_or_error) {
        std::unique_lock<std::shared_mutex> lock(stateMutex_);
        currentSettings_ = settings;
        customLogLevelMapping_ = std::move(newCustomLogLevelMapping);
        collectors_ = std::move(newCollectors);
        currentParser_ = std::move(*parser_or_error);
        return {};
    } else {
        return std::unexpected(ErrorCode::Error(Code::InvalidRegex, "Failed to create parser with new settings: " + parser_or_error.error().message));
    }
}

const LogAnalyzerSettings& LogAnalyzer::getSettings() const {
    static thread_local LogAnalyzerSettings snapshot;
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    snapshot = currentSettings_;
    return snapshot;
}

void LogAnalyzer::clear() {
    std::unique_lock<std::shared_mutex> lock(stateMutex_);
    entries_.clear();
    levelCounts.clear();
    resetStatisticCollectors();
    lastReport = {};
}

void LogAnalyzer::setCustomLogLevelMapping(std::string_view levelString, LogLevel mappedLevel) {
    std::unique_lock<std::shared_mutex> lock(stateMutex_);
    auto previousMapping = customLogLevelMapping_;
    customLogLevelMapping_[std::string(levelString)] = mappedLevel;
    // Recreate the parser with the updated customLogLevelMapping_
    // This assumes that other settings (pattern, fieldMappings) are not changing,
    // and customLogLevelMapping_ is independent from currentSettings_.
    auto parser_or_error = DefaultLogParser::create(
        currentSettings_.lineParsePattern,
        currentSettings_.fieldMappings,
        customLogLevelMapping_,
        currentSettings_.logEntryStartPattern,
        currentSettings_.caseSensitiveParsing, // Pass caseSensitiveParsing
        currentSettings_.parserErrorAction.value_or(ParserErrorAction::Warn),
        currentSettings_.maxMultilineBufferSize.value_or(DefaultLogParser::DEFAULT_MAX_BUFFER_SIZE),
        true, // threadSafe: LogAnalyzer should use a thread-safe parser
        std::nullopt // errorHandler: No specific error handler for now, default to internal logging
    );

    if (parser_or_error) {
        currentParser_ = std::move(*parser_or_error);
    } else {
        customLogLevelMapping_ = std::move(previousMapping);
        std::cerr << "Warning: Failed to re-create parser after updating custom log levels: "
                  << parser_or_error.error().message << ". Keeping previous parser and mappings." << '\n';
    }
}

filter::FilterExpression LogAnalyzer::createFilterExpressionFromCriteria(const filter::FilterCriteria& criteria) const {
    std::vector<filter::FilterExpression> expressions;

    // Keyword filter
    if (!criteria.keyword.empty()) {
        filter::FilterOperator op = criteria.keywordCaseSensitive ? filter::FilterOperator::CONTAINS : filter::FilterOperator::CONTAINS_I;
        expressions.emplace_back(filter::FilterCondition::createString(LogEntryField::MESSAGE, op, criteria.keyword).value());
    }

    // Regex pattern filter
    if (!criteria.regexPattern.empty()) {
        expressions.emplace_back(filter::FilterCondition::createString(LogEntryField::MESSAGE, filter::FilterOperator::REGEX, criteria.regexPattern).value());
    }

    // Log levels filter (combine with OR if multiple, or single EQUALS)
    if (!criteria.levels.empty()) {
        if (criteria.levels.size() == 1) {
            expressions.emplace_back(filter::FilterCondition::createString(LogEntryField::LEVEL, filter::FilterOperator::EQUALS, Utils::logLevelToString(criteria.levels[0])).value());
        } else {
            std::vector<filter::FilterExpression> levelExpressions;
            for (LogLevel level : criteria.levels) {
                levelExpressions.emplace_back(filter::FilterCondition::createString(LogEntryField::LEVEL, filter::FilterOperator::EQUALS, Utils::logLevelToString(level)).value());
            }
            expressions.emplace_back(filter::FilterLogicalOperator::OR, levelExpressions);
        }
    }

    // Time range filters
    if (criteria.startTime.has_value()) {
        std::string dtFormat = "%Y-%m-%d %H:%M:%S"; // Example default
        expressions.emplace_back(filter::FilterCondition::createDatetime(LogEntryField::TIMESTAMP, filter::FilterOperator::GREATER_THAN_OR_EQUAL, Utils::formatTimestamp(criteria.startTime.value(), dtFormat), dtFormat).value());
    }
    if (criteria.endTime.has_value()) {
        std::string dtFormat = "%Y-%m-%d %H:%M:%S"; // Example default
        expressions.emplace_back(filter::FilterCondition::createDatetime(LogEntryField::TIMESTAMP, filter::FilterOperator::LESS_THAN_OR_EQUAL, Utils::formatTimestamp(criteria.endTime.value(), dtFormat), dtFormat).value());
    }

    if (expressions.empty()) {
        return filter::FilterExpression(); // Empty expression means always true
    } else if (expressions.size() == 1) {
        return expressions[0];
    } else {
        return filter::FilterExpression(filter::FilterLogicalOperator::AND, expressions);
    }
}
