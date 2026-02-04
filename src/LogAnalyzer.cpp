#include "LogAnalyzer.h"
#include "LogParser.h"
#include "Filter.h"
#include "Statistics.h"
#include "Exporter.h"
#include "Utils.h"
#include "Error.h"
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
#include <mutex>
#include <expected>
#include <string_view>
#include <span>

#include "LogAnalyzerConfig.h"
#include "CLIConfig.h"

using namespace ErrorCode;

LogAnalyzer::LogAnalyzer() : currentSettings_(), currentParser_(std::make_unique<DefaultLogParser>(currentSettings_.lineParsePattern, currentSettings_.fieldMappings, currentSettings_.customLogLevelMappings)) {}

LogAnalyzer::LogAnalyzer(LogAnalyzerSettings settings)
    : currentSettings_(std::move(settings)),
      currentParser_(std::make_unique<DefaultLogParser>(currentSettings_.lineParsePattern, currentSettings_.fieldMappings, currentSettings_.customLogLevelMappings)) {
}

Result<void> LogAnalyzer::setSettings(LogAnalyzerSettings settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentSettings_ = std::move(settings);
    auto parser_or_error = DefaultLogParser::create(currentSettings_.lineParsePattern, currentSettings_.fieldMappings, currentSettings_.customLogLevelMappings);
    if (parser_or_error.has_value()) {
        currentParser_ = std::move(parser_or_error.value());
        return {};
    } else {
        return std::unexpected(Error(Error::Code::InvalidRegex, "Failed to create parser with new settings."));
    }
}

const LogAnalyzerSettings& LogAnalyzer::getSettings() const {
    return currentSettings_;
}

// New statistics methods
void LogAnalyzer::addStatisticCollector(std::shared_ptr<IStatisticCollector> collector) {
    if (collector) {
        _collectors.push_back(collector);
    }
}

void LogAnalyzer::processEntryForStatistics(const LogEntry& entry) {
    for (const auto& collector : _collectors) {
        collector->collect(entry);
    }
}

std::map<std::string, json> LogAnalyzer::getAllStatisticReports() const {
    std::map<std::string, json> reports;
    for (const auto& collector : _collectors) {
        reports[collector->getName()] = collector->generateReport();
    }
    return reports;
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
            LogEntry& entry = parseResult.value();
            entry.sourceFile = sourceIdentifier;
            parsedEntries.push_back(entry);
            processEntryForStatistics(entry); // Process for stats
            report.successfulParses++;
        } else {
            if (errorAction == CLIConfig::ParserErrorAction::Warn) {
                std::cerr << "Warning: Failed to parse line " << lineNumber << " in " << sourceIdentifier << ": " << parseResult.error().message << std::endl;
            } else if (errorAction == CLIConfig::ParserErrorAction::Fail) {
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
            processEntryForStatistics(entry); // Process for stats
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

void LogAnalyzer::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
    levelCounts.clear();
    lastReport = {};
}

Result<AnalysisReport> LogAnalyzer::loadAndReplace(const std::string& filePath, CLIConfig::ParserErrorAction errorAction) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return std::unexpected(Error::fileNotFound(filePath));
    }

    entries_.clear();
    auto [parsedEntries, report] = parseAndReport(file, filePath, errorAction);
    entries_ = std::move(parsedEntries);
    
    std::sort(entries_.begin(), entries_.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    lastReport = report;
    return report;
}

AnalysisReport LogAnalyzer::loadAndReplace(const std::string& filePath, const std::string& pattern) {
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
        AnalysisReport report;
        report.status = ParseError::INVALID_REGEX_PATTERN;
        report.parseErrors.push_back({ParseError::INVALID_REGEX_PATTERN, res.error().message, 0});
        std::cerr << "Error setting temporary settings: " << res.error().message << std::endl;
        
        if (auto restore_res = setSettings(oldSettings); !restore_res) {
            std::cerr << "Error restoring settings after temp set failed: " << restore_res.error().message << std::endl;
        }
        return report;
    }

    auto reportResult = loadAndReplace(filePath, CLIConfig::ParserErrorAction::Warn); // Use new version with a default
    AnalysisReport report = reportResult.value_or(AnalysisReport{});

    if (auto res = setSettings(oldSettings); !res) {
        std::cerr << "Error restoring settings: " << res.error().message << std::endl;
    }
    return report;
}

const std::vector<LogEntry>& LogAnalyzer::getEntries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_;
}

std::vector<LogEntry> LogAnalyzer::getFilteredEntries(std::function<bool(const LogEntry&)> predicate) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<LogEntry> filtered;
    for (const auto& entry : entries_) {
        if (predicate(entry)) {
            filtered.push_back(entry);
        }
    }
    return filtered;
}

void LogAnalyzer::setCustomLogLevelMapping(std::string_view levelString, LogLevel mappedLevel) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentSettings_.customLogLevelMappings[std::string(levelString)] = mappedLevel;
    currentParser_ = std::make_unique<DefaultLogParser>(currentSettings_.lineParsePattern, currentSettings_.fieldMappings, currentSettings_.customLogLevelMappings);
}

Result<AnalysisReport> LogAnalyzer::load(const std::string& filePath, CLIConfig::ParserErrorAction errorAction) {
    auto reportResult = loadAndReplace(filePath, errorAction);
    if (!reportResult) {
        return std::unexpected(reportResult.error());
    }
    return reportResult;
}

std::expected<void, LogParseError> LogAnalyzer::load(const std::string& filePath, const std::string& pattern) {
    AnalysisReport report = loadAndReplace(filePath, pattern);
    if (report.status != ParseError::SUCCESS && report.status != ParseError::PARTIAL_FAILURE) {
        return std::unexpected(LogParseError{report.status, report.parseErrors.empty() ? "" : report.parseErrors[0].message, 0});
    }
    return {};
}

std::future<Result<AnalysisReport>> LogAnalyzer::loadAsync(const std::string& filePath, CLIConfig::ParserErrorAction errorAction) {
    return std::async(std::launch::async, [this, filePath, errorAction]() {
        return load(filePath, errorAction);
    });
}

std::future<AnalysisReport> LogAnalyzer::loadAsync(const std::string& filePath, const std::string& pattern) {
    return std::async(std::launch::async, [this, filePath, pattern]() {
        // This is deprecated, but we keep its logic for now.
        return loadAndReplace(filePath, pattern);
    });
}

Result<AnalysisReport> LogAnalyzer::streamIn(std::istream& is, const std::string& sourceIdentifier, CLIConfig::ParserErrorAction errorAction) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto [newEntries, report] = parseAndReport(is, sourceIdentifier, errorAction);

    std::sort(newEntries.begin(), newEntries.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    if (!newEntries.empty()) {
        entries_.insert(entries_.end(), 
                        std::make_move_iterator(newEntries.begin()), 
                        std::make_move_iterator(newEntries.end()));
        
        std::inplace_merge(entries_.begin(), entries_.end() - newEntries.size(), entries_.end(),
                           [](const LogEntry& a, const LogEntry& b) {
                               return a.timestamp < b.timestamp;
                           });
    }

    return report;
}

Result<void> LogAnalyzer::analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, CLIConfig::ParserErrorAction errorAction) {
    for (const auto& filePath : filePaths) {
        std::istream* input;
        std::ifstream file;
        if (filePath == Utils::STDIN_FILE_PATH) {
            input = &std::cin;
        } else {
            file.open(filePath);
            if (!file.is_open()) {
                 return std::unexpected(Error::fileNotFound(filePath));
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
                     if(errorAction != CLIConfig::ParserErrorAction::Skip) {
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
    if (pattern == std::string(DEFAULT_LOG_REGEX_PATTERN_SV)) {
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

Result<AnalysisReport> LogAnalyzer::append(const std::string& filePath, CLIConfig::ParserErrorAction errorAction) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return std::unexpected(Error::fileNotFound(filePath));
    }

    auto [newEntries, report] = parseAndReport(file, filePath, errorAction);
    
    std::sort(newEntries.begin(), newEntries.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    if (!newEntries.empty()) {
        entries_.insert(entries_.end(), 
                        std::make_move_iterator(newEntries.begin()), 
                        std::make_move_iterator(newEntries.end()));
        
        std::inplace_merge(entries_.begin(), entries_.end() - newEntries.size(), entries_.end(),
                           [](const LogEntry& a, const LogEntry& b) {
                               return a.timestamp < b.timestamp;
                           });
    }

    return report;
}

std::expected<void, LogParseError> LogAnalyzer::append(const std::string& filePath, const std::string& pattern) {
    LogAnalyzerSettings oldSettings = getSettings();
    LogAnalyzerSettings tempSettings = oldSettings;
    tempSettings.lineParsePattern = pattern;
    if (pattern == std::string(DEFAULT_LOG_REGEX_PATTERN_SV)) {
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

    auto result = append(filePath, CLIConfig::ParserErrorAction::Warn);

    if (auto res = setSettings(oldSettings); !res) {
        std::cerr << "Error restoring settings: " << res.error().message << std::endl;
    }

    if (!result.has_value()) {
        return std::unexpected(LogParseError{ParseError::UNKNOWN_ERROR, result.error().message, 0});
    }
    return {};
}

std::span<const LogEntry> LogAnalyzer::getEntriesView() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_;
}

std::string LogAnalyzer::formatEntry(const LogEntry& entry, std::string_view format, const FormattingOptions& options) const {
    std::string formattedString(format);

    Utils::replaceAll(formattedString, "{timestamp}", formatTimestamp(entry.timestamp, options.dateTimeFormat));
    
    std::string levelString = logLevelToString(entry.level);
    if (options.useColor) {
        std::string colorCode;
        switch (entry.level) {
            case LogLevel::FATAL:
            case LogLevel::ERROR:   colorCode = "\033[31m"; break;
            case LogLevel::WARNING: colorCode = "\033[33m"; break;
            case LogLevel::INFO:    colorCode = "\033[32m"; break;
            case LogLevel::DEBUG:   colorCode = "\033[34m"; break;
            case LogLevel::TRACE:   colorCode = "\033[36m"; break;
            default:                colorCode = "\033[0m";  break;
        }
        Utils::replaceAll(formattedString, "{level}", colorCode + levelString + "\033[0m");
    } else {
        Utils::replaceAll(formattedString, "{level}", levelString);
    }
    
    Utils::replaceAll(formattedString, "{message}", entry.message);
    Utils::replaceAll(formattedString, "{sourceFile}", entry.sourceFile);
    Utils::replaceAll(formattedString, "{id}", std::to_string(entry.id));

    if (options.includeStructuredFields && !entry.customFields.empty()) {
        std::ostringstream ss;
        bool firstField = true;
        for (const auto& [key, value] : entry.customFields) {
            if (!firstField) {
                ss << options.structuredFieldDelimiter;
            }
            ss << key << options.structuredFieldKvDelimiter << value;
            firstField = false;
        }
        Utils::replaceAll(formattedString, "{customFields}", ss.str());
    } else {
        Utils::replaceAll(formattedString, "{customFields}", "");
    }
    
    return formattedString;
}

std::string LogAnalyzer::formatEntry(const LogEntry& entry, std::string_view format, bool useColor) const {
    FormattingOptions options;
    options.useColor = useColor;
    options.dateTimeFormat = std::string(format);
    
    std::string defaultFormat = "{timestamp} {level}: {message}";
    if (format.empty() || format == "%Y-%m-%d %H:%M:%S") {
        options.dateTimeFormat = "%Y-%m-%d %H:%M:%S";
    } else {
        options.dateTimeFormat = std::string(format);
    }

    return formatEntry(entry, defaultFormat, options);
}

// Removed obsolete functions:
//

void LogAnalyzer::printFilteredEntries(std::ostream& out, const FilterCriteria& criteria, const FormattingOptions& options) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto filteredEntriesExpected = getFilteredEntries_NoLock(criteria);
    if (filteredEntriesExpected.has_value()) {
        const auto& filteredEntries = filteredEntriesExpected.value();
        for (const auto& entry : filteredEntries) {
            out << formatEntry(entry, "{timestamp} {level}: {message}", options) << std::endl;
        }
    } else {
        out << "Error filtering entries: " << filteredEntriesExpected.error().message << std::endl;
    }
}

void LogAnalyzer::printFilteredEntries(std::ostream& out, const FilterCriteria& criteria, std::string_view formatString) const {
    FormattingOptions options;
    options.dateTimeFormat = std::string(formatString);
    printFilteredEntries(out, criteria, options);
}

Result<std::vector<LogEntry>> LogAnalyzer::getFilteredEntries(const FilterCriteria& criteria) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return getFilteredEntries_NoLock(criteria);
}

Result<std::vector<LogEntry>> LogAnalyzer::getFilteredEntries_NoLock(const FilterCriteria& criteria) const {
    // Implementation of filtering logic
    std::vector<LogEntry> filtered;
    // Dummy implementation for now
    return filtered;
}

