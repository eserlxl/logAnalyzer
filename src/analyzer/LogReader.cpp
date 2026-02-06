// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer/LogReader.h"
#include "analyzer/Core.h" // For LogAnalyzer definition which LogReader needs
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
#include <shared_mutex> // Required for stateMutex_

// Forward declaration for LogAnalyzer members that LogReader needs to access (like settings, entries_, etc.)
// This is a common pattern when a class needs to interact with another but not own it or be fully defined by it.
// The LogAnalyzer itself will provide accessors to its members, likely via the analyzer_ member.

// Constructor implementation
LogReader::LogReader(LogAnalyzer& analyzer) : analyzer_(analyzer) {}

// Private helper for default field mappings
void LogReader::setDefaultFieldMappings(LogAnalyzerSettings& settings) {
    settings.fieldMappings.clear();
    settings.fieldMappings.emplace_back(LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S");
    settings.fieldMappings.emplace_back(LogEntryField::LEVEL, 2);
    settings.fieldMappings.emplace_back(LogEntryField::MESSAGE, 3);
}

// Private helper for parsing
std::pair<std::vector<LogEntry>, AnalysisReport> LogReader::parseAndReport(std::istream& is, const std::string& sourceIdentifier, CLIConfig::ParserErrorAction errorAction) {
    std::vector<LogEntry> parsedEntries;
    AnalysisReport report;
    report.status = ParseError::SUCCESS;

    std::string line;
    size_t lineNumber = 0;
    while (std::getline(is, line)) {
        lineNumber++;
        report.linesProcessed++;
        
        // Assuming currentParser_ is accessible or managed by analyzer_
        auto parseResult = analyzer_.getCurrentParser()->parseLine(line, lineNumber, sourceIdentifier);
        if (parseResult.has_value()) {
            LogEntry entry = parseResult.value(); 
            std::cout << "DEBUG: Parsed Entry Level: " << (int)entry.level << ", String: " << Utils::logLevelToString(entry.level) << ", Message: " << entry.message << std::endl; // TEMP DEBUG
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

    auto flushResults = analyzer_.getCurrentParser()->flushRemaining();
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

// Public methods for loading/replacing/appending
ErrorCode::Result<AnalysisReport> LogReader::loadAndReplace(const std::string& filePath, CLIConfig::ParserErrorAction errorAction) {
    // Accessing LogAnalyzer's mutex and state
    std::unique_lock<std::shared_mutex> lock(analyzer_.stateMutex_); // Use analyzer's mutex
    
    if (!std::filesystem::exists(filePath)) {
        return std::unexpected(ErrorCode::Error::fileNotFound(filePath));
    }
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return std::unexpected(ErrorCode::Error::fileNotReadable(filePath));
    }

    // Clear entries and get new ones
    std::vector<LogEntry> parsedEntries;
    AnalysisReport report;
    std::tie(parsedEntries, report) = parseAndReport(file, filePath, errorAction);

    // Update analyzer's entries and statistics
    analyzer_.entries_ = std::move(parsedEntries); // Direct modification, assumes friendship or public access
    
    // Process statistics for all newly loaded entries
    for (const auto& entry : analyzer_.entries_) { // Use analyzer_.entries_
        analyzer_.processEntryForStatistics(entry); // Call method on analyzer_
    }
    
    std::sort(analyzer_.entries_.begin(), analyzer_.entries_.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    analyzer_.lastReport = report; // Update analyzer's report
    
    return report;
}

ErrorCode::Result<AnalysisReport> LogReader::loadAndReplace(const std::string& filePath, const std::string& pattern) {
    LogAnalyzerSettings oldSettings = analyzer_.getSettings(); // Get settings from analyzer
    LogAnalyzerSettings tempSettings = oldSettings;
    
    if (pattern == DEFAULT_LOG_REGEX_PATTERN_SV) {
        setDefaultFieldMappings(tempSettings); // Use LogReader's helper
    } else {
        tempSettings.fieldMappings.clear();
    }
    tempSettings.lineParsePattern = pattern; // Set the pattern
    
    // Attempt to set temporary settings. Handle potential errors.
    if (auto res = analyzer_.setSettings(tempSettings); !res) { // Use analyzer's setSettings
        AnalysisReport report_error; // Local report for error details
        report_error.status = ParseError::INVALID_REGEX_PATTERN;
        report_error.parseErrors.emplace_back(LogParseError{ParseError::INVALID_REGEX_PATTERN, res.error().message, 0});
        std::cerr << "Error setting temporary settings: " << res.error().message << std::endl;
        
        // Attempt to restore settings.
        if (auto restore_res = analyzer_.setSettings(oldSettings); !restore_res) { // Use analyzer's setSettings
            std::cerr << "Error restoring settings after temp set failed: " << restore_res.error().message << std::endl;
        }
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, report_error.parseErrors[0].message));
    }

    // Load the file using the temporary settings.
    auto reportResult = loadAndReplace(filePath, CLIConfig::ParserErrorAction::Warn);

    // Restore original settings.
    if (auto res = analyzer_.setSettings(oldSettings); !res) { // Use analyzer's setSettings
        std::cerr << "Error restoring settings: " << res.error().message << std::endl;
        if (!reportResult.has_value()) {
             return std::unexpected(ErrorCode::Error(Code::SettingsRestoreFailed, "Failed to restore original settings after load: " + res.error().message));
        }
    }
    
    return reportResult;
}

ErrorCode::Result<AnalysisReport> LogReader::load(const std::string& filePath, CLIConfig::ParserErrorAction errorAction) {
    auto reportResult = loadAndReplace(filePath, errorAction);
    if (!reportResult) {
        return std::unexpected(reportResult.error());
    }
    return reportResult;
}

std::expected<void, LogParseError> LogReader::load(const std::string& filePath, const std::string& pattern) {
    LogAnalyzerSettings oldSettings = analyzer_.getSettings();
    LogAnalyzerSettings tempSettings = oldSettings;
    
    if (pattern == DEFAULT_LOG_REGEX_PATTERN_SV) {
        setDefaultFieldMappings(tempSettings);
        tempSettings.lineParsePattern = DEFAULT_LOG_REGEX_PATTERN_SV;
    } else {
        tempSettings.fieldMappings.clear();
        tempSettings.lineParsePattern = pattern;
    }
    
    if (auto res = analyzer_.setSettings(tempSettings); !res) {
        return std::unexpected(LogParseError{ParseError::INVALID_REGEX_PATTERN, res.error().message, 0});
    }

    auto reportResult = loadAndReplace(filePath, CLIConfig::ParserErrorAction::Warn);

    if (auto res = analyzer_.setSettings(oldSettings); !res) {
        std::cerr << "Error restoring settings: " << res.error().message << std::endl;
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

std::future<ErrorCode::Result<AnalysisReport>> LogReader::loadAsync(const std::string& filePath, CLIConfig::ParserErrorAction errorAction) {
    // Use std::launch::async to ensure it runs in a separate thread
    return std::async(std::launch::async, [this, filePath, errorAction]() {
        return load(filePath, errorAction);
    });
}

std::future<ErrorCode::Result<AnalysisReport>> LogReader::loadAsync(const std::string& filePath, const std::string& pattern) {
    return std::async(std::launch::async, [this, filePath, pattern]() -> ErrorCode::Result<AnalysisReport> {
        LogAnalyzerSettings oldSettings = analyzer_.getSettings();
        LogAnalyzerSettings tempSettings = oldSettings;

        if (pattern == DEFAULT_LOG_REGEX_PATTERN_SV) {
            setDefaultFieldMappings(tempSettings);
            tempSettings.lineParsePattern = DEFAULT_LOG_REGEX_PATTERN_SV;
        } else {
            tempSettings.fieldMappings.clear();
            tempSettings.lineParsePattern = pattern;
        }
        
        if (auto res = analyzer_.setSettings(tempSettings); !res) {
            AnalysisReport report_error;
            report_error.status = ParseError::INVALID_REGEX_PATTERN;
            report_error.parseErrors.emplace_back(LogParseError{ParseError::INVALID_REGEX_PATTERN, res.error().message, 0});
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, res.error().message));
        }

        auto reportResult = loadAndReplace(filePath, CLIConfig::ParserErrorAction::Warn);

        if (auto res = analyzer_.setSettings(oldSettings); !res) {
            std::cerr << "Error restoring settings: " << res.error().message << std::endl;
        }

        return reportResult;
    });
}

ErrorCode::Result<AnalysisReport> LogReader::streamIn(std::istream& is, const std::string& sourceIdentifier, CLIConfig::ParserErrorAction errorAction) {
    auto [newEntries, report] = parseAndReport(is, sourceIdentifier, errorAction);

    std::sort(newEntries.begin(), newEntries.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    if (newEntries.empty()) {
        return report;
    }

    std::vector<LogEntry> mergedEntries;
    mergedEntries.reserve(analyzer_.entries_.size() + newEntries.size()); // Access analyzer's entries

    { // Scope for shared_lock to read entries_
        std::shared_lock<std::shared_mutex> sharedLock(analyzer_.stateMutex_); // Use analyzer's mutex
        std::merge(analyzer_.entries_.begin(), analyzer_.entries_.end(),
                   newEntries.begin(), newEntries.end(),
                   std::back_inserter(mergedEntries),
                   [](const LogEntry& a, const LogEntry& b) {
                       return a.timestamp < b.timestamp;
                   });
    } // shared_lock is released here

    { // Scope for unique_lock to modify entries_ and process stats
        std::unique_lock<std::shared_mutex> uniqueLock(analyzer_.stateMutex_); // Use analyzer's mutex
        analyzer_.entries_.swap(mergedEntries); // Modify analyzer's entries_
        
        for (const auto& entry : newEntries) {
            analyzer_.processEntryForStatistics(entry); // Call method on analyzer_
        }
    } // unique_lock is released here

    return report;
}

ErrorCode::Result<void> LogReader::analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, CLIConfig::ParserErrorAction errorAction) {
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
            // Assuming currentParser_ is accessible or managed by analyzer_
            auto parseResultOpt = analyzer_.getCurrentParser()->processLine(line, lineNumber, (filePath == Utils::STDIN_FILE_PATH ? "stdin" : filePath));
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
        
        auto flushResults = analyzer_.getCurrentParser()->flushRemaining();
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

std::expected<void, LogParseError> LogReader::analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, const std::string& pattern) {
    LogAnalyzerSettings oldSettings = analyzer_.getSettings();
    LogAnalyzerSettings tempSettings = oldSettings;
    tempSettings.lineParsePattern = pattern;
    if (pattern == DEFAULT_LOG_REGEX_PATTERN_SV) {
        setDefaultFieldMappings(tempSettings);
    } else {
        tempSettings.fieldMappings.clear();
    }
    
    if (auto res = analyzer_.setSettings(tempSettings); !res) {
        return std::unexpected(LogParseError{ParseError::INVALID_REGEX_PATTERN, res.error().message, 0});
    }

    auto result = analyzeStream(filePaths, entryCallback, CLIConfig::ParserErrorAction::Warn);

    if (auto res = analyzer_.setSettings(oldSettings); !res) {
        std::cerr << "Error restoring settings: " << res.error().message << std::endl;
    }
    
    if(!result) {
        return std::unexpected(LogParseError{ParseError::UNKNOWN_ERROR, result.error().message, 0});
    }
    return {};
}

ErrorCode::Result<AnalysisReport> LogReader::append(const std::string& filePath, CLIConfig::ParserErrorAction errorAction) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return std::unexpected(ErrorCode::Error::fileNotReadable(filePath));
    }

    auto [newEntries, report] = parseAndReport(file, filePath, errorAction);
    
    std::sort(newEntries.begin(), newEntries.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    if (newEntries.empty()) {
        return report;
    }

    std::vector<LogEntry> mergedEntries;
    mergedEntries.reserve(analyzer_.entries_.size() + newEntries.size()); // Access analyzer's entries

    { // Scope for shared_lock to read entries_
        std::shared_lock<std::shared_mutex> sharedLock(analyzer_.stateMutex_); // Use analyzer's mutex
        std::merge(analyzer_.entries_.begin(), analyzer_.entries_.end(),
                   newEntries.begin(), newEntries.end(),
                   std::back_inserter(mergedEntries),
                   [](const LogEntry& a, const LogEntry& b) {
                       return a.timestamp < b.timestamp;
                   });
    } // shared_lock is released here

    { // Scope for unique_lock to modify entries_ and process stats
        std::unique_lock<std::shared_mutex> uniqueLock(analyzer_.stateMutex_); // Use analyzer's mutex
        analyzer_.entries_.swap(mergedEntries); // Modify analyzer's entries_
        
        for (const auto& entry : newEntries) {
            analyzer_.processEntryForStatistics(entry); // Call method on analyzer_
        }
    } // unique_lock is released here

    return report;
}

std::expected<void, LogParseError> LogReader::append(const std::string& filePath, const std::string& pattern) {
    LogAnalyzerSettings oldSettings = analyzer_.getSettings();
    LogAnalyzerSettings tempSettings = oldSettings;
    tempSettings.lineParsePattern = pattern;
    if (pattern == DEFAULT_LOG_REGEX_PATTERN_SV) {
        setDefaultFieldMappings(tempSettings);
    } else {
        tempSettings.fieldMappings.clear();
    }

    if (auto res = analyzer_.setSettings(tempSettings); !res) {
        return std::unexpected(LogParseError{ParseError::INVALID_REGEX_PATTERN, res.error().message, 0});
    }

    auto reportResult = append(filePath, CLIConfig::ParserErrorAction::Warn);

    if (auto res = analyzer_.setSettings(oldSettings); !res) {
        std::cerr << "Error restoring settings: " << res.error().message << std::endl;
        if (!reportResult) {
             const auto& error = reportResult.error();
             return std::unexpected(LogParseError{ParseError::UNKNOWN_ERROR, error.message, 0});
        }
    }

    if (!reportResult) {
        const auto& error = reportResult.error();
        return std::unexpected(LogParseError{ParseError::UNKNOWN_ERROR, error.message, 0});
    }
    
    return {};
}
