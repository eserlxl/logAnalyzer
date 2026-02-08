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

ErrorCode::Result<AnalysisReport> LogAnalyzer::loadAndReplace(
    const std::string& filePath, 
    ParserErrorAction errorAction,
    std::optional<CancellationToken*> cancellationToken,
    std::optional<ProgressCallback> progressCallback
) {
    if (!std::filesystem::exists(filePath)) {
        return std::unexpected(ErrorCode::Error::fileNotFound(filePath));
    }
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return std::unexpected(ErrorCode::Error::fileNotReadable(filePath));
    }

    auto [parsedEntries, report] = parseAndReport(file, filePath, errorAction, cancellationToken, progressCallback);

    std::sort(parsedEntries.begin(), parsedEntries.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    {
        std::unique_lock<std::shared_mutex> lock(stateMutex_);
        entries_ = std::move(parsedEntries);
        resetStatisticCollectors();

        // Process statistics for all newly loaded entries
        for (const auto& entry : entries_) {
            processEntryForStatistics(entry);
        }

        lastReport = report;
    }

    return report;
}

ErrorCode::Result<AnalysisReport> LogAnalyzer::loadAndReplace(const std::string& filePath, const std::string& pattern) {
    LogAnalyzerSettings oldSettings = getSettings();
    LogAnalyzerSettings tempSettings = oldSettings;
    tempSettings.lineParsePattern = pattern;
    // Existing code before the if block:
    // tempSettings.fieldMappings.clear(); // This is handled by setDefaultFieldMappings

    // Call helper to set default mappings if pattern matches
    if (pattern == DEFAULT_LOG_REGEX_PATTERN_SV) {
        setDefaultFieldMappings(tempSettings); // Use helper to set mappings
    } else {
        tempSettings.fieldMappings.clear();
    }
    
    // Attempt to set temporary settings. Handle potential errors from setSettings.
    // H3: Ensure setSettings errors are propagated. Assuming setSettings returns ErrorCode::Result<void> or similar.
    if (auto res = setSettings(tempSettings); !res) {
        // Create a report for the error, but return a Result for the function.
        AnalysisReport report_error; // This report_error is local and only used to extract details.
        report_error.status = ParseError::INVALID_REGEX_PATTERN;
        report_error.parseErrors.push_back({ParseError::INVALID_REGEX_PATTERN, res.error().message, 0});
        std::cerr << "Error setting temporary settings: " << res.error().message << std::endl;
        
        // Attempt to restore settings. Log error if it fails, but prioritize the original error.
        if (auto restore_res = setSettings(oldSettings); !restore_res) {
            std::cerr << "Error restoring settings after temp set failed: " << restore_res.error().message << std::endl;
            // Potentially combine errors here, but for now, return the primary error from setting temp settings.
        }
        // Return an unexpected result with an appropriate error.
        // Mapping LogParseError details to a generic Error type.
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, report_error.parseErrors[0].message));
    }

    // Load the file using the temporary settings. This call returns ErrorCode::Result<AnalysisReport>.
    auto reportResult = loadAndReplace(filePath, ParserErrorAction::Warn);

    // Restore original settings. This must happen regardless of whether loading succeeded or failed.
    if (auto res = setSettings(oldSettings); !res) {
        std::cerr << "Error restoring settings: " << res.error().message << std::endl;
        // If loading also failed, we might want to combine errors, or prioritize the loading error.
        // For now, we'll log the restore error and return the loading result if it was an error.
        if (!reportResult.has_value()) {
             // If load failed, and restore also failed, return the load error.
             // We might want to capture the restore error as well if possible, but for now, focus on propagating the load error.
             // Assuming the underlying error type for `ErrorCode::Result<AnalysisReport>` is `Error`.
             // If `reportResult.error()` is not an `Error` object, this would need adjustment.
             // For now, assume it's compatible or implicitly convertible.
             return std::unexpected(ErrorCode::Error(Code::SettingsRestoreFailed, "Failed to restore original settings after load: " + res.error().message));
        }
        // If load succeeded but restore failed, we still return the successful load result, but log the restore error.
    }
    
    // Return the result of the load operation. If it was an error, return that.
    // If it was successful, return the AnalysisReport.
    return reportResult;
}

ErrorCode::Result<AnalysisReport> LogAnalyzer::load(
    const std::string& filePath, 
    ParserErrorAction errorAction,
    std::optional<CancellationToken*> cancellationToken,
    std::optional<ProgressCallback> progressCallback
) {
    auto reportResult = loadAndReplace(filePath, errorAction, cancellationToken, progressCallback);
    if (!reportResult) {
        return std::unexpected(reportResult.error());
    }
    return reportResult;
}

std::expected<void, LogParseError> LogAnalyzer::load(const std::string& filePath, const std::string& pattern) {
    LogAnalyzerSettings oldSettings = getSettings();
    LogAnalyzerSettings tempSettings = oldSettings;
    
    if (pattern == DEFAULT_LOG_REGEX_PATTERN_SV) {
        setDefaultFieldMappings(tempSettings);
        tempSettings.lineParsePattern = DEFAULT_LOG_REGEX_PATTERN_SV;
    } else {
        tempSettings.fieldMappings.clear();
        tempSettings.lineParsePattern = pattern;
    }
    
    if (auto res = setSettings(tempSettings); !res) {
        return std::unexpected(LogParseError{ParseError::INVALID_REGEX_PATTERN, res.error().message, 0});
    }

    auto reportResult = loadAndReplace(filePath, ParserErrorAction::Warn);

    if (auto res = setSettings(oldSettings); !res) {
        std::cerr << "Error restoring settings: " << res.error().message << std::endl;
        if (!reportResult.has_value()) {
            return std::unexpected(LogParseError{ParseError::UNKNOWN_ERROR, reportResult.error().message, 0});
        }
    }
    
    if (!reportResult) {
        return std::unexpected(LogParseError{ParseError::UNKNOWN_ERROR, reportResult.error().message, 0});
    }
    
    AnalysisReport report = *reportResult;
    if (report.status != ParseError::SUCCESS && report.status != ParseError::PARTIAL_FAILURE) {
        return std::unexpected(LogParseError{report.status, report.parseErrors.empty() ? "" : report.parseErrors[0].message, 0});
    }
    
    return {};
}

std::future<ErrorCode::Result<AnalysisReport>> LogAnalyzer::loadAsync(
    const std::string& filePath, 
    ParserErrorAction errorAction,
    std::shared_ptr<CancellationToken> cancellationToken,
    std::optional<ProgressCallback> progressCallback
) {
    return std::async(std::launch::async, [this, filePath, errorAction, cancellationToken, progressCallback]() {
        std::optional<CancellationToken*> tokenOpt;
        if (cancellationToken) {
            tokenOpt = cancellationToken.get();
        }
        return load(filePath, errorAction, tokenOpt, progressCallback);
    });
}

std::future<ErrorCode::Result<AnalysisReport>> LogAnalyzer::loadAsync(const std::string& filePath, const std::string& pattern) {
    return std::async(std::launch::async, [this, filePath, pattern]() -> ErrorCode::Result<AnalysisReport> {
        LogAnalyzerSettings oldSettings = getSettings();
        LogAnalyzerSettings tempSettings = oldSettings;

        if (pattern == DEFAULT_LOG_REGEX_PATTERN_SV) {
            setDefaultFieldMappings(tempSettings);
            tempSettings.lineParsePattern = DEFAULT_LOG_REGEX_PATTERN_SV;
        } else {
            tempSettings.fieldMappings.clear();
            tempSettings.lineParsePattern = pattern;
        }
        
        if (auto res = setSettings(tempSettings); !res) {
            AnalysisReport report_error;
            report_error.status = ParseError::INVALID_REGEX_PATTERN;
            report_error.parseErrors.push_back({ParseError::INVALID_REGEX_PATTERN, res.error().message, 0});
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, res.error().message));
        }

        auto reportResult = loadAndReplace(filePath, ParserErrorAction::Warn);

        if (auto res = setSettings(oldSettings); !res) {
            std::cerr << "Error restoring settings: " << res.error().message << std::endl;
        }

        return reportResult;
    });
}

ErrorCode::Result<AnalysisReport> LogAnalyzer::append(
    const std::string& filePath, 
    ParserErrorAction errorAction,
    std::optional<CancellationToken*> cancellationToken,
    std::optional<ProgressCallback> progressCallback
) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return std::unexpected(ErrorCode::Error::fileNotReadable(filePath));
    }

    auto [newEntries, report] = parseAndReport(file, filePath, errorAction, cancellationToken, progressCallback);
    
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

std::expected<void, LogParseError> LogAnalyzer::append(const std::string& filePath, const std::string& pattern) {
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

    // Handle errors from setSettings when applying temporary pattern.
    if (auto res = setSettings(tempSettings); !res) {
        return std::unexpected(LogParseError{ParseError::INVALID_REGEX_PATTERN, res.error().message, 0});
    }

    // Corrected call to the other append overload which returns ErrorCode::Result<AnalysisReport>.
    auto reportResult = append(filePath, ParserErrorAction::Warn);

    // Restore original settings. Log errors if they occur, but prioritize the outcome of append.
    if (auto res = setSettings(oldSettings); !res) {
        std::cerr << "Error restoring settings: " << res.error().message << std::endl;
        // If append operation also failed, we should propagate its error.
        // If append succeeded but restore failed, we log the restore error and return success for the append operation.
        if (!reportResult) {
             // Propagate the error from append.
             // Map ErrorCode::Result<AnalysisReport> error to LogParseError.
             const auto& error = reportResult.error(); // Assuming this is an Error object.
             return std::unexpected(LogParseError{ParseError::UNKNOWN_ERROR, error.message, 0});
        }
        // If append succeeded, we return success here, but the restore error is logged.
    }

    // Check if the append operation itself failed.
    if (!reportResult) {
        // Propagate the error from append.
        const auto& error = reportResult.error(); // Assuming this is an Error object.
        return std::unexpected(LogParseError{ParseError::UNKNOWN_ERROR, error.message, 0});
    }

    // If append succeeded and settings were restored (or restore error was logged but not prioritized), return success.
    return {};
}
