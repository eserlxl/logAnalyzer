// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer/Log/Reader.h"
#include "analyzer/Core.h" // For LogAnalyzer definition which LogReader needs
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
#include <shared_mutex> // Required for stateMutex_

// Constructor implementation
LogReader::LogReader(LogAnalyzer& analyzer) : analyzer_(analyzer) {}

// Implementation of LogReader::ScopedLogSettings
// Assumes analyzer_.stateMutex_ is already locked (unique_lock) by the caller.
LogReader::ScopedLogSettings::ScopedLogSettings(LogAnalyzer& analyzer, const std::string& pattern_val, LogReader& reader)
    : analyzer_(analyzer), reader_(reader), restorationError_(std::nullopt), settingsRestored_(false) {
    // stateMutex_ is already held by caller; avoid re-locking through getSettings().
    originalSettings_ = analyzer_.currentSettings_;
    LogAnalyzerSettings tempSettings = originalSettings_;

    if (pattern_val == DEFAULT_LOG_REGEX_PATTERN_SV) {
        reader_.setDefaultFieldMappings(tempSettings); // Call helper through reader instance
        // Low-risk cleanup: Redundant assignment of DEFAULT_LOG_REGEX_PATTERN_SV is implicitly removed.
        // The setDefaultFieldMappings or prior settings logic should handle setting this if appropriate.
    } else {
        tempSettings.fieldMappings.clear();
    }
    tempSettings.lineParsePattern = pattern_val; // Set the pattern

    if (auto res = analyzer_.setSettings(tempSettings); !res) {
        // Error setting temporary settings. Store the error, and indicate restoration is not needed.
        initialSetSettingsError_ = res.error();
    }
}

LogReader::ScopedLogSettings::~ScopedLogSettings() {
    if (!initialSetSettingsError_.has_value() && !settingsRestored_) {
        if (auto res = analyzer_.setSettings(originalSettings_); !res) {
            restorationError_ = res.error(); // Store the error for caller to potentially retrieve
            std::cerr << "CRITICAL ERROR: Failed to restore original settings during LogReader cleanup: " << res.error().message << std::endl;
        }
    }
}

void LogReader::ScopedLogSettings::markSettingsRestored() {
    settingsRestored_ = true;
}

std::optional<ErrorCode::Error> LogReader::ScopedLogSettings::getInitialSetSettingsError() const {
    return initialSetSettingsError_;
}

std::optional<ErrorCode::Error> LogReader::ScopedLogSettings::getRestorationError() const {
    return restorationError_;
}

// Private helper for default field mappings
void LogReader::setDefaultFieldMappings(LogAnalyzerSettings& settings) {
    settings.fieldMappings.clear();
    settings.fieldMappings.emplace_back(LogEntryField::TIMESTAMP, std::make_optional<size_t>(1), std::vector<std::string>{"%Y-%m-%d %H:%M:%S"});
    settings.fieldMappings.emplace_back(LogEntryField::LEVEL, std::make_optional<size_t>(2));
    settings.fieldMappings.emplace_back(LogEntryField::MESSAGE, std::make_optional<size_t>(3));
}

// Private helper for parsing
std::pair<std::vector<LogEntry>, AnalysisReport> LogReader::parseAndReport(ILogParser* parser, std::istream& is, const std::string& sourceIdentifier, ParserErrorAction errorAction) {
    std::vector<LogEntry> parsedEntries;
    AnalysisReport report;
    report.status = ParseError::SUCCESS;

    std::string line;
    size_t lineNumber = 0;
    while (std::getline(is, line)) {
        lineNumber++;
        report.linesProcessed++;
        
        auto parseResult = parser->parseLine(line, lineNumber, sourceIdentifier);
        if (parseResult.has_value()) {
            LogEntry entry = parseResult.value(); 
            entry.sourceFile = sourceIdentifier;
            parsedEntries.push_back(entry);
            report.successfulParses++;
        } else {
            if (errorAction == ParserErrorAction::Warn) {
                std::cerr << "Warning: Failed to parse line " << lineNumber << " in " << sourceIdentifier << ": " << parseResult.error().message << std::endl;
            } else if (errorAction == ParserErrorAction::Throw) {
                // This will be handled by the caller by checking the Result
            }
            report.parseErrors.emplace_back(LogParseError{ParseError::PARTIAL_FAILURE, parseResult.error().message, lineNumber, parseResult.error()});
        }
    }

    auto flushResults = parser->flushRemaining();
    for (const auto& result : flushResults) {
        if (result.has_value()) {
            LogEntry entry = result.value();
            entry.sourceFile = sourceIdentifier;
            parsedEntries.push_back(entry);
            report.successfulParses++;
        } else {
            if (errorAction == ParserErrorAction::Warn) {
                 std::cerr << "Warning: Failed to parse remaining buffer for " << sourceIdentifier << ": " << result.error().message << std::endl;
            }
            report.parseErrors.emplace_back(LogParseError{ParseError::PARTIAL_FAILURE, result.error().message, 0, result.error()});
        }
    }

    if (!report.parseErrors.empty()) {
        report.status = ParseError::PARTIAL_FAILURE;
    }
    
    return {parsedEntries, report};
}

// Private helper to perform load and replace logic.
// Assumes analyzer_.stateMutex_ is already locked (unique_lock) by the caller.
// The parser passed is expected to be stable for the duration of this call.
ErrorCode::Result<AnalysisReport> LogReader::doLoadAndReplace(ILogParser* parser, const std::string& filePath, ParserErrorAction errorAction) {
    if (!std::filesystem::exists(filePath)) {
        return std::unexpected(ErrorCode::Error::fileNotFound(filePath));
    }
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return std::unexpected(ErrorCode::Error::fileNotReadable(filePath));
    }

    // Use C++17 structured bindings
    auto [parsedEntries, report] = parseAndReport(parser, file, filePath, errorAction);

    // Modify analyzer_.entries_ (requires unique_lock from caller)
    analyzer_.entries_ = std::move(parsedEntries);
    analyzer_.resetStatisticCollectors();
    
    // Process statistics for all newly loaded entries
    for (const auto& entry : analyzer_.entries_) {
        analyzer_.processEntryForStatistics(entry);
    }
    
    std::sort(analyzer_.entries_.begin(), analyzer_.entries_.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    analyzer_.lastReport = report;
    
    return report;
}

// Private helper to perform append logic.
// Assumes analyzer_.stateMutex_ is already locked (unique_lock) by the caller.
// The parser passed is expected to be stable for the duration of this call.
ErrorCode::Result<AnalysisReport> LogReader::doAppend(ILogParser* parser, const std::string& filePath, ParserErrorAction errorAction) {
    if (!std::filesystem::exists(filePath)) {
        return std::unexpected(ErrorCode::Error::fileNotFound(filePath));
    }
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return std::unexpected(ErrorCode::Error::fileNotReadable(filePath));
    }

    // Use C++17 structured bindings
    auto [newEntries, report] = parseAndReport(parser, file, filePath, errorAction);
    
    std::sort(newEntries.begin(), newEntries.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    if (newEntries.empty()) {
        analyzer_.lastReport = report; // Update last report even if nothing new was appended
        return report;
    }

    std::vector<LogEntry> mergedEntries;
    mergedEntries.reserve(analyzer_.entries_.size() + newEntries.size());

    // All modifications to analyzer_.entries_ are done under the assumption of a unique_lock held by the caller.
    std::merge(analyzer_.entries_.begin(), analyzer_.entries_.end(),
               newEntries.begin(), newEntries.end(),
               std::back_inserter(mergedEntries),
               [](const LogEntry& a, const LogEntry& b) {
                   return a.timestamp < b.timestamp;
               });
    
    analyzer_.entries_.swap(mergedEntries);
    
    for (const auto& entry : newEntries) {
        analyzer_.processEntryForStatistics(entry);
    }

    analyzer_.lastReport = report; // Update last report
    return report;
}

// Private helper to perform analyzeStream logic.
// Assumes analyzer_.stateMutex_ is already locked (shared_lock or unique_lock) by the caller.
// The parser passed is expected to be stable for the duration of this call.
ErrorCode::Result<void> LogReader::doAnalyzeStreamInternal(ILogParser* parser, const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, ParserErrorAction errorAction) {
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
            auto parseResultOpt = parser->processLine(line, lineNumber, (filePath == Utils::STDIN_FILE_PATH ? "stdin" : filePath));
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
                    if(errorAction == ParserErrorAction::Warn) {
                        std::cerr << "Warning: Failed to parse line " << lineNumber << " in " << filePath << ": " << result.error().message << std::endl;
                    }
                     if(errorAction != ParserErrorAction::Ignore) {
                        LogEntry partialEntry;
                        partialEntry.level = LogLevel::UNKNOWN;
                        partialEntry.message = line;
                        partialEntry.sourceFile = (filePath == Utils::STDIN_FILE_PATH ? "stdin" : filePath);
                        partialEntry.id = lineNumber;
                        partialEntry.sourceLineNumber = lineNumber; 
                        if (!entryCallback(partialEntry)) {
                            shouldContinue = false;
                            break;
                        }
                    }
                }
            }
            if (!shouldContinue) break;
        }
        
        auto flushResults = parser->flushRemaining();
        for (const auto& result : flushResults) {
            if (result.has_value()) {
                LogEntry entry = result.value();
                entry.sourceFile = (filePath == Utils::STDIN_FILE_PATH ? "stdin" : filePath);
                if (!entryCallback(entry)) {
                    shouldContinue = false;
                    break;
                }
            } else {
                 if(errorAction == ParserErrorAction::Warn) {
                    std::cerr << "Warning: Failed to parse remaining buffer for " << filePath << ": " << result.error().message << std::endl;
                }
            }
            if (!shouldContinue) break;
        }

        if (!shouldContinue) break;
    }
    return {};
}

// Public methods for loading/replacing/appending
ErrorCode::Result<AnalysisReport> LogReader::loadAndReplace(const std::string& filePath, ParserErrorAction errorAction) {
    std::unique_lock<std::shared_mutex> lock(analyzer_.stateMutex_); // Acquire unique lock at start
    ILogParser* currentParser = analyzer_.getCurrentParser(); // Get parser while lock is held
    return doLoadAndReplace(currentParser, filePath, errorAction);
}

ErrorCode::Result<AnalysisReport> LogReader::loadAndReplace(const std::string& filePath, const std::string& pattern) {
    std::unique_lock<std::shared_mutex> lock(analyzer_.stateMutex_); // Protect entire sequence
    
    ScopedLogSettings scopedSettings(analyzer_, pattern, *this);
    if (scopedSettings.getInitialSetSettingsError().has_value()) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, scopedSettings.getInitialSetSettingsError()->message));
    }

    ILogParser* currentParser = analyzer_.getCurrentParser();
    auto reportResult = doLoadAndReplace(currentParser, filePath, ParserErrorAction::Warn);

    if (scopedSettings.getRestorationError().has_value()) {
        if (reportResult.has_value()) {
            // Parsing was successful, but restoration failed. Add to report.
            reportResult.value().parseErrors.emplace_back(
                LogParseError{ParseError::SETTINGS_RESTORE_FAILED, scopedSettings.getRestorationError()->message, 0, scopedSettings.getRestorationError().value()}
            );
            if (reportResult.value().status == ParseError::SUCCESS) {
                reportResult.value().status = ParseError::PARTIAL_FAILURE; // Indicate some issue
            }
        }
        // If reportResult had an error, we don't need to add the restoration error to it again,
        // as the primary parsing error is more relevant. The critical error for restoration will be logged.
    }
    
    return reportResult;
}

ErrorCode::Result<AnalysisReport> LogReader::load(const std::string& filePath, ParserErrorAction errorAction) {
    auto reportResult = loadAndReplace(filePath, errorAction);
    if (!reportResult) {
        // No need to convert error type as load() returns ErrorCode::Result<AnalysisReport>
        return std::unexpected(reportResult.error());
    }
    return reportResult;
}

std::expected<void, LogParseError> LogReader::load(const std::string& filePath, const std::string& pattern) {
    std::unique_lock<std::shared_mutex> lock(analyzer_.stateMutex_);

    ScopedLogSettings scopedSettings(analyzer_, pattern, *this);
    if (scopedSettings.getInitialSetSettingsError().has_value()) {
        return std::unexpected(LogParseError(scopedSettings.getInitialSetSettingsError().value()));
    }

    ILogParser* currentParser = analyzer_.getCurrentParser();
    auto reportResult = doLoadAndReplace(currentParser, filePath, ParserErrorAction::Warn);

    if (scopedSettings.getRestorationError().has_value()) {
        // If restoration failed, return an error indicating that.
        // This addresses audit point 4 for this method.
        return std::unexpected(LogParseError(scopedSettings.getRestorationError().value()));
    }
    
    if (!reportResult) {
        // Convert ErrorCode::Result error to LogParseError. This addresses audit point 3.
        return std::unexpected(LogParseError(reportResult.error()));
    }
    
    // If all successful (parsing and settings management)
    return {};
}

std::future<ErrorCode::Result<AnalysisReport>> LogReader::loadAsync(const std::string& filePath, ParserErrorAction errorAction) {
    // Use std::launch::async to ensure it runs in a separate thread
    // The synchronous load method will acquire its own lock.
    return std::async(std::launch::async, [this, filePath, errorAction]() {
        return load(filePath, errorAction);
    });
}

std::future<ErrorCode::Result<AnalysisReport>> LogReader::loadAsync(const std::string& filePath, const std::string& pattern) {
    return std::async(std::launch::async, [this, filePath, pattern]() -> ErrorCode::Result<AnalysisReport> {
        std::unique_lock<std::shared_mutex> lock(analyzer_.stateMutex_); // Protect entire sequence in async thread
        
        ScopedLogSettings scopedSettings(analyzer_, pattern, *this);
        if (scopedSettings.getInitialSetSettingsError().has_value()) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, scopedSettings.getInitialSetSettingsError()->message));
        }

        ILogParser* currentParser = analyzer_.getCurrentParser();
        auto reportResult = doLoadAndReplace(currentParser, filePath, ParserErrorAction::Warn);

        if (scopedSettings.getRestorationError().has_value()) {
            if (reportResult.has_value()) {
                reportResult.value().parseErrors.emplace_back(
                    LogParseError{ParseError::SETTINGS_RESTORE_FAILED, scopedSettings.getRestorationError()->message, 0, scopedSettings.getRestorationError().value()}
                );
                if (reportResult.value().status == ParseError::SUCCESS) {
                    reportResult.value().status = ParseError::PARTIAL_FAILURE;
                }
            }
        }

        return reportResult;
    });
}

ErrorCode::Result<AnalysisReport> LogReader::streamIn(std::istream& is, const std::string& sourceIdentifier, ParserErrorAction errorAction) {
    std::unique_lock<std::shared_mutex> lock(analyzer_.stateMutex_); // Acquire unique lock at start (High-risk 1)
    ILogParser* currentParser = analyzer_.getCurrentParser(); // Get parser while lock is held

    // Use C++17 structured bindings
    auto [newEntries, report] = parseAndReport(currentParser, is, sourceIdentifier, errorAction);

    std::sort(newEntries.begin(), newEntries.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    if (newEntries.empty()) {
        analyzer_.lastReport = report; // Update last report even if nothing new was appended
        return report;
    }

    std::vector<LogEntry> mergedEntries;
    mergedEntries.reserve(analyzer_.entries_.size() + newEntries.size());

    // All modifications to analyzer_.entries_ are done under the unique_lock held for the entire function.
    std::merge(analyzer_.entries_.begin(), analyzer_.entries_.end(),
               newEntries.begin(), newEntries.end(),
               std::back_inserter(mergedEntries),
               [](const LogEntry& a, const LogEntry& b) {
                   return a.timestamp < b.timestamp;
               });
    
    analyzer_.entries_.swap(mergedEntries); // Modify analyzer's entries_
    
    for (const auto& entry : newEntries) {
        analyzer_.processEntryForStatistics(entry); // Call method on analyzer_
    }

    analyzer_.lastReport = report; // Update last report
    return report;
}

ErrorCode::Result<void> LogReader::analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, ParserErrorAction errorAction) {
    std::shared_lock<std::shared_mutex> lock(analyzer_.stateMutex_); // Shared lock for reading parser state (High-risk 1)
    ILogParser* currentParser = analyzer_.getCurrentParser(); // Get parser while lock is held

    return doAnalyzeStreamInternal(currentParser, filePaths, entryCallback, errorAction);
}

std::expected<void, LogParseError> LogReader::analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, const std::string& pattern) {
    std::unique_lock<std::shared_mutex> lock(analyzer_.stateMutex_); // Protect entire sequence (High-risk 2)

    ScopedLogSettings scopedSettings(analyzer_, pattern, *this);
    if (scopedSettings.getInitialSetSettingsError().has_value()) {
        return std::unexpected(LogParseError(scopedSettings.getInitialSetSettingsError().value()));
    }

    ILogParser* currentParser = analyzer_.getCurrentParser(); // Get parser after settings are applied
    auto result = doAnalyzeStreamInternal(currentParser, filePaths, entryCallback, ParserErrorAction::Warn);

    if (scopedSettings.getRestorationError().has_value()) {
        // If restoration failed, return an error indicating that.
        return std::unexpected(LogParseError(scopedSettings.getRestorationError().value()));
    }
    
    if (!result) {
        // Convert ErrorCode::Result error to LogParseError. This addresses audit point 3.
        return std::unexpected(LogParseError(result.error()));
    }
    return {};
}

ErrorCode::Result<AnalysisReport> LogReader::append(const std::string& filePath, ParserErrorAction errorAction) {
    std::unique_lock<std::shared_mutex> lock(analyzer_.stateMutex_); // Acquire unique lock at start
    ILogParser* currentParser = analyzer_.getCurrentParser(); // Get parser while lock is held
    return doAppend(currentParser, filePath, errorAction);
}

std::expected<void, LogParseError> LogReader::append(const std::string& filePath, const std::string& pattern) {
    std::unique_lock<std::shared_mutex> lock(analyzer_.stateMutex_); // Protect entire sequence

    ScopedLogSettings scopedSettings(analyzer_, pattern, *this);
    if (scopedSettings.getInitialSetSettingsError().has_value()) {
        return std::unexpected(LogParseError(scopedSettings.getInitialSetSettingsError().value()));
    }

    ILogParser* currentParser = analyzer_.getCurrentParser();
    auto reportResult = doAppend(currentParser, filePath, ParserErrorAction::Warn);

    if (scopedSettings.getRestorationError().has_value()) {
        // If restoration failed, return an error indicating that.
        return std::unexpected(LogParseError(scopedSettings.getRestorationError().value()));
    }

    if (!reportResult) {
        // Convert ErrorCode::Result error to LogParseError. This addresses audit point 3.
        return std::unexpected(LogParseError(reportResult.error()));
    }
    
    return {};
}
