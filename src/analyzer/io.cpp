// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer/core.h"
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

void LogAnalyzer::setDefaultFieldMappings(LogAnalyzerSettings& settings) {
    settings.fieldMappings.clear();
    settings.fieldMappings.emplace_back(LogEntryField::TIMESTAMP, std::make_optional<size_t>(1), std::vector<std::string>{"%Y-%m-%d %H:%M:%S"});
    settings.fieldMappings.emplace_back(LogEntryField::LEVEL, std::make_optional<size_t>(2));
    settings.fieldMappings.emplace_back(LogEntryField::MESSAGE, std::make_optional<size_t>(3));
}

std::pair<std::vector<LogEntry>, AnalysisReport> LogAnalyzer::parseAndReport(
    std::istream& is, 
    const std::string& sourceIdentifier, 
    ParserErrorAction errorAction,
    std::optional<CancellationToken*> cancellationToken,
    std::optional<ProgressCallback> progressCallback
) {
    std::vector<LogEntry> parsedEntries;
    AnalysisReport report;
    report.status = ParseError::SUCCESS;
    std::streamoff totalBytes = 0;

    auto emitProgress = [&](double percentage, std::string_view message) {
        if (!progressCallback.has_value()) {
            return;
        }
        try {
            (*progressCallback)(percentage, message);
        } catch (...) {
            // Ignore callback exceptions to keep parsing resilient.
        }
    };

    // Best-effort progress support for seekable streams.
    const std::streampos originalPos = is.tellg();
    if (progressCallback.has_value() && originalPos != std::streampos(-1)) {
        is.seekg(0, std::ios::end);
        const std::streampos endPos = is.tellg();
        if (endPos != std::streampos(-1) && endPos >= originalPos) {
            totalBytes = static_cast<std::streamoff>(endPos - originalPos);
        }
        is.clear();
        is.seekg(originalPos);
    }
    emitProgress(0.0, "Parsing started");

    std::unique_ptr<ILogParser> parser;
    {
        std::shared_lock<std::shared_mutex> lock(stateMutex_);
        if (!currentParser_) {
            report.status = ParseError::UNKNOWN_ERROR;
            report.parseErrors.emplace_back(LogParseError{
                ParseError::UNKNOWN_ERROR,
                "Parser is not initialized",
                0
            });
            return {std::move(parsedEntries), report};
        }
        parser = currentParser_->clone();
    }
    if (!parser) {
        report.status = ParseError::UNKNOWN_ERROR;
        report.parseErrors.emplace_back(LogParseError{
            ParseError::UNKNOWN_ERROR,
            "Failed to clone parser",
            0
        });
        return {std::move(parsedEntries), report};
    }

    std::string line;
    size_t lineNumber = 0;
    while (std::getline(is, line)) {
        if (cancellationToken && (*cancellationToken)->isCancelled()) {
            report.status = ParseError::CANCELLED;
            emitProgress(100.0, "Parsing cancelled");
            break;
        }
        lineNumber++;
        report.linesProcessed++;
        if (totalBytes > 0) {
            const std::streampos currentPos = is.tellg();
            if (currentPos != std::streampos(-1) && currentPos >= originalPos) {
                const auto consumed = static_cast<std::streamoff>(currentPos - originalPos);
                const double progress = std::clamp(
                    (100.0 * static_cast<double>(consumed)) / static_cast<double>(totalBytes),
                    0.0,
                    100.0);
                emitProgress(progress, "Parsing stream");
            }
        }

        std::optional<ErrorCode::Result<LogEntry>> parseResultOpt;
        try {
            parseResultOpt = parser->processLine(line, lineNumber, sourceIdentifier);
        } catch (const std::exception& e) {
            report.parseErrors.emplace_back(LogParseError{
                ParseError::PARTIAL_FAILURE,
                std::string("Exception while parsing line: ") + e.what(),
                lineNumber
            });
            if (errorAction == ParserErrorAction::Throw) {
                report.status = ParseError::UNKNOWN_ERROR;
                return {std::move(parsedEntries), report};
            }
            continue;
        }
        if (!parseResultOpt.has_value()) {
            continue;
        }

        if (parseResultOpt->has_value()) {
            LogEntry entry = std::move(parseResultOpt->value());
            entry.sourceFile = sourceIdentifier;
            parsedEntries.push_back(std::move(entry));
            report.successfulParses++;
        } else {
            if (errorAction == ParserErrorAction::Warn) {
                std::cerr << "Warning: Failed to parse line " << lineNumber << " in " << sourceIdentifier << ": " << parseResultOpt->error().message << '\n';
            }
            report.parseErrors.emplace_back(LogParseError{ParseError::PARTIAL_FAILURE, parseResultOpt->error().message, lineNumber});
        }
    }

    std::vector<ErrorCode::Result<LogEntry>> flushResults;
    try {
        flushResults = parser->flushRemaining();
    } catch (const std::exception& e) {
        report.parseErrors.emplace_back(LogParseError{
            ParseError::PARTIAL_FAILURE,
            std::string("Exception while flushing parser buffer: ") + e.what(),
            0
        });
        if (errorAction == ParserErrorAction::Throw) {
            report.status = ParseError::UNKNOWN_ERROR;
            return {std::move(parsedEntries), report};
        }
    }
    for (auto& result : flushResults) {
        if (result.has_value()) {
            LogEntry entry = std::move(result.value());
            entry.sourceFile = sourceIdentifier;
            parsedEntries.push_back(std::move(entry));
            report.successfulParses++;
        } else {
            if (errorAction == ParserErrorAction::Warn) {
                 std::cerr << "Warning: Failed to parse remaining buffer for " << sourceIdentifier << ": " << result.error().message << '\n';
            }
            report.parseErrors.emplace_back(LogParseError{ParseError::PARTIAL_FAILURE, result.error().message, 0});
        }
    }

    if (report.status == ParseError::SUCCESS && !report.parseErrors.empty()) {
        report.status = ParseError::PARTIAL_FAILURE;
    }
    if (report.status != ParseError::CANCELLED) {
        emitProgress(100.0, "Parsing completed");
    }
    
    return {std::move(parsedEntries), report};
}

const std::vector<LogEntry>& LogAnalyzer::getEntries() const {
    static thread_local std::vector<LogEntry> snapshot;
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    snapshot = entries_;
    return snapshot;
}

std::vector<LogEntry> LogAnalyzer::getEntriesSnapshot() const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    return entries_;
}

// Add getLastReport method for thread-safe access
const AnalysisReport& LogAnalyzer::getLastReport() const {
    static thread_local AnalysisReport snapshot;
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    snapshot = lastReport;
    return snapshot;
}

AnalysisReport LogAnalyzer::getLastReportSnapshot() const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    return lastReport;
}

std::span<const LogEntry> LogAnalyzer::getEntriesView() const {
    const auto& snapshot = getEntries();
    return std::span<const LogEntry>(snapshot.data(), snapshot.size());
}
