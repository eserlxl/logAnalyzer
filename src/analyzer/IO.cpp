// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer/Core.h"
#include "analyzer/LogReader.h"
#include "utils/String.h"
#include "core/LogParser.h"
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
    CLIConfig::ParserErrorAction errorAction,
    std::optional<CancellationToken*> cancellationToken,
    std::optional<ProgressCallback> progressCallback
) {
    (void)progressCallback; // TODO: Implement progress reporting based on stream position if possible
    std::vector<LogEntry> parsedEntries;
    AnalysisReport report;
    report.status = ParseError::SUCCESS;

    std::string line;
    size_t lineNumber = 0;
    while (std::getline(is, line)) {
        if (cancellationToken && (*cancellationToken)->isCancelled()) {
            report.status = ParseError::CANCELLED;
            break;
        }
        lineNumber++;
        report.linesProcessed++;
        
        auto parseResult = currentParser_->parseLine(line, lineNumber, sourceIdentifier);
        if (parseResult.has_value()) {
            LogEntry entry = parseResult.value(); 
            entry.sourceFile = sourceIdentifier;
            parsedEntries.push_back(entry);
            report.successfulParses++;
        } else {
            if (errorAction == CLIConfig::ParserErrorAction::Warn) {
                std::cerr << "Warning: Failed to parse line " << lineNumber << " in " << sourceIdentifier << ": " << parseResult.error().message << std::endl;
            } else if (errorAction == CLIConfig::ParserErrorAction::Throw) {
                // This will be handled by the caller by checking the Result
            }
            report.parseErrors.emplace_back(LogParseError{ParseError::PARTIAL_FAILURE, parseResult.error().message, lineNumber});
        }
    }

    auto flushResults = currentParser_->flushRemaining();
    for (const auto& result : flushResults) {
        if (result.has_value()) {
            LogEntry entry = result.value();
            entry.sourceFile = sourceIdentifier;
            parsedEntries.push_back(entry);
            report.successfulParses++;
        } else {
            if (errorAction == CLIConfig::ParserErrorAction::Warn) {
                 std::cerr << "Warning: Failed to parse remaining buffer for " << sourceIdentifier << ": " << result.error().message << std::endl;
            }
            report.parseErrors.emplace_back(LogParseError{ParseError::PARTIAL_FAILURE, result.error().message, 0});
        }
    }

    if (!report.parseErrors.empty()) {
        report.status = ParseError::PARTIAL_FAILURE;
    }
    
    return {parsedEntries, report};
}

const std::vector<LogEntry>& LogAnalyzer::getEntries() const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    return entries_;
}

// Add getLastReport method for thread-safe access
const AnalysisReport& LogAnalyzer::getLastReport() const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    return lastReport;
}

std::span<const LogEntry> LogAnalyzer::getEntriesView() const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    return entries_;
}
