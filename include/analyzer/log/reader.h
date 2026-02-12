// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef ANALYZER_LOG_READER_H
#define ANALYZER_LOG_READER_H

#include "core/log/types.h"
#include "core/error.h"
#include "config/core.h"
#include <string>
#include <vector>
#include <functional>
#include <future>
#include <expected>
#include <iosfwd>

// Forward declarations
class LogAnalyzer;
class ILogParser;

class LogReader {
public:
    explicit LogReader(LogAnalyzer& analyzer);

    ErrorCode::Result<AnalysisReport> loadAndReplace(const std::string& filePath, ParserErrorAction errorAction);
    ErrorCode::Result<AnalysisReport> loadAndReplace(const std::string& filePath, const std::string& pattern);
    
    ErrorCode::Result<AnalysisReport> load(const std::string& filePath, ParserErrorAction errorAction);
    std::expected<void, LogParseError> load(const std::string& filePath, const std::string& pattern);
    
    std::future<ErrorCode::Result<AnalysisReport>> loadAsync(const std::string& filePath, ParserErrorAction errorAction);
    std::future<ErrorCode::Result<AnalysisReport>> loadAsync(const std::string& filePath, const std::string& pattern);
    
    ErrorCode::Result<AnalysisReport> append(const std::string& filePath, ParserErrorAction errorAction);
    std::expected<void, LogParseError> append(const std::string& filePath, const std::string& pattern);
    
    ErrorCode::Result<AnalysisReport> streamIn(std::istream& is, const std::string& sourceIdentifier, ParserErrorAction errorAction);
    
    ErrorCode::Result<void> analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, ParserErrorAction errorAction);
    std::expected<void, LogParseError> analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, const std::string& pattern);

private:
    LogAnalyzer& analyzer_;

    // Helper class for scoping log settings changes
    class ScopedLogSettings {
    public:
        ScopedLogSettings(LogAnalyzer& analyzer, const std::string& pattern_val, LogReader& reader);
        ~ScopedLogSettings();
        void markSettingsRestored();
        std::optional<ErrorCode::Error> getInitialSetSettingsError() const;
        std::optional<ErrorCode::Error> getRestorationError() const;
    private:
        LogAnalyzer& analyzer_;
        LogReader& reader_;
        LogAnalyzerSettings originalSettings_;
        std::optional<ErrorCode::Error> initialSetSettingsError_;
        std::optional<ErrorCode::Error> restorationError_;
        bool settingsRestored_;
    };

    static void setDefaultFieldMappings(LogAnalyzerSettings& settings);
    static std::pair<std::vector<LogEntry>, AnalysisReport> parseAndReport(ILogParser* parser, std::istream& is, const std::string& sourceIdentifier, ParserErrorAction errorAction);
    ErrorCode::Result<AnalysisReport> doLoadAndReplace(ILogParser* parser, const std::string& filePath, ParserErrorAction errorAction);
    ErrorCode::Result<AnalysisReport> doAppend(ILogParser* parser, const std::string& filePath, ParserErrorAction errorAction);
    static ErrorCode::Result<void> doAnalyzeStreamInternal(ILogParser* parser, const std::vector<std::string>& filePaths, const std::function<bool(const LogEntry&)>& entryCallback, ParserErrorAction errorAction);
};

#endif // ANALYZER_LOG_READER_H
