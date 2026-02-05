// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2024 eserlxl

#ifndef LOGANALYZER_ANALYZER_LOGREADER_H
#define LOGANALYZER_ANALYZER_LOGREADER_H

#include "config/CLIConfig.h"
#include "core/LogTypes.h"
#include <future>
#include <string>
#include <vector>

class LogAnalyzer;

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
    
    void setDefaultFieldMappings(LogAnalyzerSettings& settings);
    std::pair<std::vector<LogEntry>, AnalysisReport> parseAndReport(std::istream& is, const std::string& sourceIdentifier, CLIConfig::ParserErrorAction errorAction);
};

#endif // LOGANALYZER_ANALYZER_LOGREADER_H