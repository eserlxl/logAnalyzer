// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef LOGANALYZER_ANALYZER_LOGREADER_H
#define LOGANALYZER_ANALYZER_LOGREADER_H

#include "config/CLI.h"
#include "core/Log/Types.h"
#include <future>
#include <future>
#include <string>
#include <vector>
#include <memory> // Required for std::unique_ptr in ScopedLogSettings

class LogAnalyzer;
class ILogParser; // Forward declare ILogParser

class LogReader {
public:
    explicit LogReader(LogAnalyzer& analyzer);

    ErrorCode::Result<AnalysisReport> loadAndReplace(const std::string& filePath, CLIConfig::ParserErrorAction errorAction);
    ErrorCode::Result<AnalysisReport> loadAndReplace(const std::string& filePath, const std::string& pattern);
    
    ErrorCode::Result<AnalysisReport> load(const std::string& filePath, CLIConfig::ParserErrorAction errorAction);
    std::expected<void, LogParseError> load(const std::string& filePath, const std::string& pattern);
    
    std::future<ErrorCode::Result<AnalysisReport>> loadAsync(const std::string& filePath, CLIConfig::ParserErrorAction errorAction);
    std::future<ErrorCode::Result<AnalysisReport>> loadAsync(const std::string& filePath, const std::string& pattern);

    ErrorCode::Result<AnalysisReport> append(const std::string& filePath, CLIConfig::ParserErrorAction errorAction);
    std::expected<void, LogParseError> append(const std::string& filePath, const std::string& pattern);
    
    ErrorCode::Result<AnalysisReport> streamIn(std::istream& is, const std::string& sourceIdentifier, CLIConfig::ParserErrorAction errorAction);
    ErrorCode::Result<void> analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, CLIConfig::ParserErrorAction errorAction);
    std::expected<void, LogParseError> analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, const std::string& pattern);

private:
    LogAnalyzer& analyzer_;

    // Private RAII helper class for managing temporary settings.
    // Assumes analyzer_.stateMutex_ is already locked (unique_lock) by the caller.
    class ScopedLogSettings {
    public:
        ScopedLogSettings(LogAnalyzer& analyzer, const std::string& pattern_val, LogReader& reader);
        ~ScopedLogSettings();

        void markSettingsRestored();
        std::optional<ErrorCode::Error> getInitialSetSettingsError() const;
        std::optional<ErrorCode::Error> getRestorationError() const; // New getter

    private:
        LogAnalyzer& analyzer_;
        LogReader& reader_;
        LogAnalyzerSettings originalSettings_;
        std::optional<ErrorCode::Error> initialSetSettingsError_;
        std::optional<ErrorCode::Error> restorationError_; // New member to store restoration error
        bool settingsRestored_;
    };
    
    // Private helpers that assume locks are held by the caller
    ErrorCode::Result<AnalysisReport> doLoadAndReplace(ILogParser* parser, const std::string& filePath, CLIConfig::ParserErrorAction errorAction);
    ErrorCode::Result<AnalysisReport> doAppend(ILogParser* parser, const std::string& filePath, CLIConfig::ParserErrorAction errorAction);
    ErrorCode::Result<void> doAnalyzeStreamInternal(ILogParser* parser, const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, CLIConfig::ParserErrorAction errorAction);
    
    void setDefaultFieldMappings(LogAnalyzerSettings& settings);
    std::pair<std::vector<LogEntry>, AnalysisReport> parseAndReport(ILogParser* parser, std::istream& is, const std::string& sourceIdentifier, CLIConfig::ParserErrorAction errorAction);
};

#endif // LOGANALYZER_ANALYZER_LOGREADER_H