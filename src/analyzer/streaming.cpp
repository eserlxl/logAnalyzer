// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer/Core.h"
#include "analyzer/Log/Reader.h"
#include "utils/String.h"
#include "core/Log/Parser.h"
#include "filter/Core.h"
#include "stats/Core.h"
#include "export/Core.h"
#include "utils/Core.h"
#include "core/Error.h"
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
#include <filesystem>

ErrorCode::Result<AnalysisReport> LogAnalyzer::streamIn(
    std::istream& is, 
    const std::string& sourceIdentifier, 
    ParserErrorAction errorAction,
    std::optional<CancellationToken*> cancellationToken,
    std::optional<ProgressCallback> progressCallback
) {
    auto [newEntries, report] = parseAndReport(is, sourceIdentifier, errorAction, cancellationToken, progressCallback);

    std::sort(newEntries.begin(), newEntries.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    // H1: Check if newEntries is empty AFTER sorting, before acquiring locks.
    if (newEntries.empty()) {
        std::unique_lock<std::shared_mutex> uniqueLock(stateMutex_);
        lastReport = report;
        return report;
    }

    { // Scope for unique_lock to modify entries_ and process stats atomically
        std::unique_lock<std::shared_mutex> uniqueLock(stateMutex_);
        // Collect stats before ownership moves from newEntries.
        for (const auto& entry : newEntries) {
            processEntryForStatistics(entry);
        }

        if (entries_.empty()) {
            entries_ = std::move(newEntries);
        } else if (entries_.back().timestamp <= newEntries.front().timestamp) {
            entries_.reserve(entries_.size() + newEntries.size());
            entries_.insert(entries_.end(),
                            std::make_move_iterator(newEntries.begin()),
                            std::make_move_iterator(newEntries.end()));
        } else if (newEntries.back().timestamp <= entries_.front().timestamp) {
            std::vector<LogEntry> mergedEntries;
            mergedEntries.reserve(entries_.size() + newEntries.size());
            mergedEntries.insert(mergedEntries.end(),
                                 std::make_move_iterator(newEntries.begin()),
                                 std::make_move_iterator(newEntries.end()));
            mergedEntries.insert(mergedEntries.end(),
                                 std::make_move_iterator(entries_.begin()),
                                 std::make_move_iterator(entries_.end()));
            entries_.swap(mergedEntries);
        } else {
            std::vector<LogEntry> mergedEntries;
            mergedEntries.reserve(entries_.size() + newEntries.size());
            std::merge(entries_.begin(), entries_.end(),
                       newEntries.begin(), newEntries.end(),
                       std::back_inserter(mergedEntries),
                       [](const LogEntry& a, const LogEntry& b) {
                           return a.timestamp < b.timestamp;
                       });
            entries_.swap(mergedEntries);
        }
        lastReport = report;
    } // unique_lock is released here

    return report;
}

ErrorCode::Result<void> LogAnalyzer::analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, ParserErrorAction errorAction) {
    std::unique_ptr<ILogParser> streamParser;
    {
        std::shared_lock<std::shared_mutex> lock(stateMutex_);
        if (!currentParser_) {
            return std::unexpected(ErrorCode::Error::unexpected("Parser is not initialized"));
        }
        streamParser = currentParser_->clone();
    }
    if (!streamParser) {
        return std::unexpected(ErrorCode::Error::unexpected("Failed to clone parser for stream analysis"));
    }

    for (const auto& filePath : filePaths) {
        std::istream* input;
        std::ifstream file;
        if (filePath == Utils::STDIN_FILE_PATH) {
            input = &std::cin;
        } else {
            file.open(filePath);
            if (!file.is_open()) {
                 return std::unexpected(ErrorCode::Error::fileNotReadable(filePath));
            }
            input = &file;
        }

        std::string line;
        size_t lineNumber = 0;
        bool shouldContinue = true;
        while (std::getline(*input, line)) {
            lineNumber++;
            std::optional<ErrorCode::Result<LogEntry>> parseResultOpt;
            try {
                parseResultOpt = streamParser->processLine(line, lineNumber, (filePath == Utils::STDIN_FILE_PATH ? "stdin" : filePath));
            } catch (const std::exception& e) {
                if (errorAction == ParserErrorAction::Throw) {
                    return std::unexpected(ErrorCode::Error::unexpected(
                        "Exception while parsing line " + std::to_string(lineNumber) + " in " + filePath + ": " + e.what()));
                }
                if (errorAction == ParserErrorAction::Warn) {
                    std::cerr << "Warning: Exception while parsing line " << lineNumber << " in " << filePath << ": " << e.what() << '\n';
                }
                if (errorAction != ParserErrorAction::Ignore) {
                    LogEntry partialEntry;
                    partialEntry.level = LogLevel::UNKNOWN;
                    partialEntry.message = line;
                    partialEntry.sourceFile = (filePath == Utils::STDIN_FILE_PATH ? "stdin" : filePath);
                    partialEntry.id = lineNumber;
                    if (!entryCallback(partialEntry)) {
                        shouldContinue = false;
                        break;
                    }
                }
                continue;
            }

            if (parseResultOpt) {
                const auto& result = *parseResultOpt;
                if (result) {
                    LogEntry entry = *result;
                    entry.sourceFile = (filePath == Utils::STDIN_FILE_PATH ? "stdin" : filePath);
                    if (!entryCallback(entry)) {
                        shouldContinue = false;
                        break;
                    }
                } else {
                    if(errorAction == ParserErrorAction::Warn) {
                        std::cerr << "Warning: Failed to parse line " << lineNumber << " in " << filePath << ": " << result.error().message << '\n';
                    }
                     if(errorAction != ParserErrorAction::Ignore) {
                        LogEntry partialEntry;
                        partialEntry.level = LogLevel::UNKNOWN;
                        partialEntry.message = line;
                        partialEntry.sourceFile = (filePath == Utils::STDIN_FILE_PATH ? "stdin" : filePath);
                        partialEntry.id = lineNumber;
                        if (!entryCallback(partialEntry)) {
                            shouldContinue = false;
                            break;
                        }
                    }
                }
            }
            if (!shouldContinue) break;
        }
        
        std::vector<ErrorCode::Result<LogEntry>> flushResults;
        try {
            flushResults = streamParser->flushRemaining();
        } catch (const std::exception& e) {
            if (errorAction == ParserErrorAction::Throw) {
                return std::unexpected(ErrorCode::Error::unexpected(
                    "Exception while flushing parser buffer for " + filePath + ": " + e.what()));
            }
            if (errorAction == ParserErrorAction::Warn) {
                std::cerr << "Warning: Exception while flushing parser buffer for " << filePath << ": " << e.what() << '\n';
            }
            flushResults.clear();
        }
        for (const auto& result : flushResults) {
            if (result) {
                LogEntry entry = *result;
                entry.sourceFile = (filePath == Utils::STDIN_FILE_PATH ? "stdin" : filePath);
                if (!entryCallback(entry)) {
                    shouldContinue = false;
                    break;
                }
            } else {
                 if(errorAction == ParserErrorAction::Warn) {
                    std::cerr << "Warning: Failed to parse remaining buffer for " << filePath << ": " << result.error().message << '\n';
                }
            }
            if (!shouldContinue) break;
        }

        if (!shouldContinue) break;
    }
    return {};
}

std::expected<void, LogParseError> LogAnalyzer::analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, const std::string& pattern) {
    LogAnalyzerSettings oldSettings = getSettings();
    LogAnalyzerSettings tempSettings = oldSettings;
    tempSettings.lineParsePattern = pattern;
    if (pattern == DEFAULT_LOG_REGEX_PATTERN_SV) {
        tempSettings.fieldMappings.clear();
        tempSettings.fieldMappings.emplace_back(LogEntryField::TIMESTAMP, std::make_optional<size_t>(1), std::vector<std::string>{"%Y-%m-%d %H:%M:%S"});
        tempSettings.fieldMappings.emplace_back(LogEntryField::LEVEL, std::make_optional<size_t>(2));
        tempSettings.fieldMappings.emplace_back(LogEntryField::MESSAGE, std::make_optional<size_t>(3));
    } else {
        tempSettings.fieldMappings.clear();
    }
    
    if (auto res = setSettings(tempSettings); !res) {
        return std::unexpected(LogParseError{ParseError::INVALID_REGEX_PATTERN, res.error().message, 0});
    }

    auto result = analyzeStream(filePaths, entryCallback, ParserErrorAction::Warn);

    if (auto res = setSettings(oldSettings); !res) {
        std::cerr << "Error restoring settings: " << res.error().message << '\n';
    }
    
    if(!result) {
        return std::unexpected(LogParseError{ParseError::UNKNOWN_ERROR, result.error().message, 0});
    }
    return {};
}
