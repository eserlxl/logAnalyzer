// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer/Core.h"
#include "utils/String.h"
#include "core/LogParser.h"
#include "filter/Core.h"
#include "stats/Statistics.h"
#include "export/Exporter.h"
#include "utils/UtilsCore.h"
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
    settings.fieldMappings.emplace_back(LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S");
    settings.fieldMappings.emplace_back(LogEntryField::LEVEL, 2);
    settings.fieldMappings.emplace_back(LogEntryField::MESSAGE, 3);
}

std::pair<std::vector<LogEntry>, AnalysisReport> LogAnalyzer::parseAndReport(std::istream& is, const std::string& sourceIdentifier, CLIConfig::ParserErrorAction errorAction) {
    std::vector<LogEntry> parsedEntries;
    AnalysisReport report;
    report.status = ParseError::SUCCESS;

    std::string line;
    size_t lineNumber = 0;
    while (std::getline(is, line)) {
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

ErrorCode::Result<AnalysisReport> LogAnalyzer::loadAndReplace(const std::string& filePath, CLIConfig::ParserErrorAction errorAction) {
    std::unique_lock<std::shared_mutex> lock(stateMutex_);
    if (!std::filesystem::exists(filePath)) {
        return std::unexpected(ErrorCode::Error::fileNotFound(filePath));
    }
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return std::unexpected(ErrorCode::Error::fileNotReadable(filePath));
    }

    entries_.clear();
    auto [parsedEntries, report] = parseAndReport(file, filePath, errorAction);
    entries_ = std::move(parsedEntries);
    
    // Process statistics for all newly loaded entries
    for (const auto& entry : entries_) {
        processEntryForStatistics(entry);
    }
    
    std::sort(entries_.begin(), entries_.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    lastReport = report;
    return report;
}

ErrorCode::Result<AnalysisReport> LogAnalyzer::loadAndReplace(const std::string& filePath, const std::string& pattern) {
    LogAnalyzerSettings oldSettings = getSettings();
    LogAnalyzerSettings tempSettings = oldSettings;
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
    auto reportResult = loadAndReplace(filePath, CLIConfig::ParserErrorAction::Warn);

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

const std::vector<LogEntry>& LogAnalyzer::getEntries() const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    return entries_;
}

// Add getLastReport method for thread-safe access
const AnalysisReport& LogAnalyzer::getLastReport() const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    return lastReport;
}

ErrorCode::Result<AnalysisReport> LogAnalyzer::load(const std::string& filePath, CLIConfig::ParserErrorAction errorAction) {
    auto reportResult = loadAndReplace(filePath, errorAction);
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

    auto reportResult = loadAndReplace(filePath, CLIConfig::ParserErrorAction::Warn);

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

std::future<ErrorCode::Result<AnalysisReport>> LogAnalyzer::loadAsync(const std::string& filePath, CLIConfig::ParserErrorAction errorAction) {
    return std::async(std::launch::async, [this, filePath, errorAction]() {
        return load(filePath, errorAction);
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

        auto reportResult = loadAndReplace(filePath, CLIConfig::ParserErrorAction::Warn);

        if (auto res = setSettings(oldSettings); !res) {
            std::cerr << "Error restoring settings: " << res.error().message << std::endl;
        }

        return reportResult;
    });
}

ErrorCode::Result<AnalysisReport> LogAnalyzer::streamIn(std::istream& is, const std::string& sourceIdentifier, CLIConfig::ParserErrorAction errorAction) {
    auto [newEntries, report] = parseAndReport(is, sourceIdentifier, errorAction);

    std::sort(newEntries.begin(), newEntries.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    // H1: Check if newEntries is empty AFTER sorting, before acquiring locks.
    if (newEntries.empty()) {
        return report;
    }

    std::vector<LogEntry> mergedEntries;
    mergedEntries.reserve(entries_.size() + newEntries.size());

    { // Scope for shared_lock to read entries_
        std::shared_lock<std::shared_mutex> sharedLock(stateMutex_);
        std::merge(entries_.begin(), entries_.end(),
                   newEntries.begin(), newEntries.end(),
                   std::back_inserter(mergedEntries),
                   [](const LogEntry& a, const LogEntry& b) {
                       return a.timestamp < b.timestamp;
                   });
    } // shared_lock is released here

    { // Scope for unique_lock to modify entries_ and process stats
        std::unique_lock<std::shared_mutex> uniqueLock(stateMutex_);
        entries_.swap(mergedEntries); // Modify entries_ under unique lock
        
        // Process statistics for the newly added entries. This modifies statistics_,
        // so it must be within the unique lock scope.
        for (const auto& entry : newEntries) {
            processEntryForStatistics(entry);
        }
    } // unique_lock is released here

    return report;
}

ErrorCode::Result<void> LogAnalyzer::analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, CLIConfig::ParserErrorAction errorAction) {
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
            auto parseResultOpt = currentParser_->processLine(line, lineNumber, (filePath == Utils::STDIN_FILE_PATH ? "stdin" : filePath));
            if (parseResultOpt.has_value()) {
                const auto& result = parseResultOpt.value();
                if (result.has_value()) {
                    LogEntry entry = result.value();
                    entry.sourceFile = (filePath == Utils::STDIN_FILE_PATH ? "stdin" : filePath);
                    if (!entryCallback(entry)) {
                        shouldContinue = false;
                        break;
                    }
                } else {
                    if(errorAction == CLIConfig::ParserErrorAction::Warn) {
                        std::cerr << "Warning: Failed to parse line " << lineNumber << " in " << filePath << ": " << result.error().message << std::endl;
                    }
                     if(errorAction != CLIConfig::ParserErrorAction::Ignore) {
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
        
        auto flushResults = currentParser_->flushRemaining();
        for (const auto& result : flushResults) {
            if (result.has_value()) {
                LogEntry entry = result.value();
                entry.sourceFile = (filePath == Utils::STDIN_FILE_PATH ? "stdin" : filePath);
                if (!entryCallback(entry)) {
                    shouldContinue = false;
                    break;
                }
            } else {
                 if(errorAction == CLIConfig::ParserErrorAction::Warn) {
                    std::cerr << "Warning: Failed to parse remaining buffer for " << filePath << ": " << result.error().message << std::endl;
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
        tempSettings.fieldMappings.emplace_back(LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S");
        tempSettings.fieldMappings.emplace_back(LogEntryField::LEVEL, 2);
        tempSettings.fieldMappings.emplace_back(LogEntryField::MESSAGE, 3);
    } else {
        tempSettings.fieldMappings.clear();
    }
    
    if (auto res = setSettings(tempSettings); !res) {
        return std::unexpected(LogParseError{ParseError::INVALID_REGEX_PATTERN, res.error().message, 0});
    }

    auto result = analyzeStream(filePaths, entryCallback, CLIConfig::ParserErrorAction::Warn);

    if (auto res = setSettings(oldSettings); !res) {
        std::cerr << "Error restoring settings: " << res.error().message << std::endl;
    }
    
    if(!result) {
        return std::unexpected(LogParseError{ParseError::UNKNOWN_ERROR, result.error().message, 0});
    }
    return {};
}

ErrorCode::Result<AnalysisReport> LogAnalyzer::append(const std::string& filePath, CLIConfig::ParserErrorAction errorAction) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return std::unexpected(ErrorCode::Error::fileNotReadable(filePath));
    }

    auto [newEntries, report] = parseAndReport(file, filePath, errorAction);
    
    std::sort(newEntries.begin(), newEntries.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    // H1: Check if newEntries is empty AFTER sorting, before acquiring locks.
    if (newEntries.empty()) {
        return report;
    }

    std::vector<LogEntry> mergedEntries;
    mergedEntries.reserve(entries_.size() + newEntries.size());

    { // Scope for shared_lock to read entries_
        std::shared_lock<std::shared_mutex> sharedLock(stateMutex_);
        std::merge(entries_.begin(), entries_.end(),
                   newEntries.begin(), newEntries.end(),
                   std::back_inserter(mergedEntries),
                   [](const LogEntry& a, const LogEntry& b) {
                       return a.timestamp < b.timestamp;
                   });
    } // shared_lock is released here

    { // Scope for unique_lock to modify entries_ and process stats
        std::unique_lock<std::shared_mutex> uniqueLock(stateMutex_);
        entries_.swap(mergedEntries); // Modify entries_ under unique lock
        
        // Process statistics for the newly added entries. This modifies statistics_,
        // so it must be within the unique lock scope.
        for (const auto& entry : newEntries) {
            processEntryForStatistics(entry);
        }
    } // unique_lock is released here

    return report;
}

std::expected<void, LogParseError> LogAnalyzer::append(const std::string& filePath, const std::string& pattern) {
    LogAnalyzerSettings oldSettings = getSettings();
    LogAnalyzerSettings tempSettings = oldSettings;
    tempSettings.lineParsePattern = pattern;
    if (pattern == DEFAULT_LOG_REGEX_PATTERN_SV) {
        tempSettings.fieldMappings.clear();
        tempSettings.fieldMappings.emplace_back(LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S");
        tempSettings.fieldMappings.emplace_back(LogEntryField::LEVEL, 2);
        tempSettings.fieldMappings.emplace_back(LogEntryField::MESSAGE, 3);
    } else {
        tempSettings.fieldMappings.clear();
    }

    // Handle errors from setSettings when applying temporary pattern.
    if (auto res = setSettings(tempSettings); !res) {
        return std::unexpected(LogParseError{ParseError::INVALID_REGEX_PATTERN, res.error().message, 0});
    }

    // Corrected call to the other append overload which returns ErrorCode::Result<AnalysisReport>.
    auto reportResult = append(filePath, CLIConfig::ParserErrorAction::Warn);

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

std::span<const LogEntry> LogAnalyzer::getEntriesView() const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    return entries_;
}


