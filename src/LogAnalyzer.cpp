#include "LogAnalyzer.h"
#include "LogParser.h"
#include "Filter.h"
#include "Statistics.h"
#include "Exporter.h"
#include "Utils.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <regex>
#include <iomanip>
#include <sstream>
#include <memory> // For std::make_unique and std::move
#include <vector> // For std::vector
#include <utility> // For std::move
#include <future> // For std::future
#include <functional> // For std::function
#include <iterator> // For std::make_move_iterator
#include <map> // For std::map
#include <mutex> // For std::lock_guard, std::mutex
#include <expected> // For std::expected
#include <string_view> // For std::string_view
#include <span> // For std::span

#include "LogAnalyzerConfig.h"

LogAnalyzer::LogAnalyzer() : currentSettings_(), currentParser_(std::make_unique<DefaultLogParser>(currentSettings_.lineParsePattern, currentSettings_.fieldMappings, currentSettings_.customLogLevelMappings)) {}

LogAnalyzer::LogAnalyzer(LogAnalyzerSettings settings)
    : currentSettings_(std::move(settings)),
      currentParser_(std::make_unique<DefaultLogParser>(currentSettings_.lineParsePattern, currentSettings_.fieldMappings, currentSettings_.customLogLevelMappings)) {
}

std::expected<void, LogParseError> LogAnalyzer::setSettings(LogAnalyzerSettings settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentSettings_ = std::move(settings);
    // Re-initialize parser with new settings
    auto parser_or_error = DefaultLogParser::create(currentSettings_.lineParsePattern, currentSettings_.fieldMappings, currentSettings_.customLogLevelMappings);
    if (parser_or_error.has_value()) {
        currentParser_ = std::move(parser_or_error.value());
        return {};
    } else {
        return std::unexpected(parser_or_error.error());
    }
}

const LogAnalyzerSettings& LogAnalyzer::getSettings() const {
    return currentSettings_;
}

// Internal helper for core parsing logic
std::pair<std::vector<LogEntry>, AnalysisReport> LogAnalyzer::parseAndReport(std::istream& is, const std::string& sourceIdentifier) {
    std::vector<LogEntry> parsedEntries;
    AnalysisReport report;
    report.status = ParseError::SUCCESS; // Assume success initially

    std::string line;
    size_t lineNumber = 0;
    while (std::getline(is, line)) {
        lineNumber++;
        report.linesProcessed++;
        
        auto parseResultOpt = currentParser_->processLine(line, lineNumber);
        if (parseResultOpt.has_value()) {
            const auto& result = parseResultOpt.value();
            if (result.success) {
                LogEntry entry = result.entry;
                entry.sourceFile = sourceIdentifier;
                parsedEntries.push_back(entry);
                report.successfulParses++;
            } else {
                // Construct LogParseError using errorMessage and failingPart from ParseResult
                std::string errorMessageStr = result.errorMessage;
                std::string combinedMessage = errorMessageStr + " - Failing part: " + result.failingPart;
                report.parseErrors.emplace_back(LogParseError{ParseError::PARTIAL_FAILURE, combinedMessage, lineNumber});
            }
        }
    }

    // Flush any remaining buffered multi-line entries
    auto flushResults = currentParser_->flushRemaining();
    for (const auto& result : flushResults) {
        if (result.success) {
            LogEntry entry = result.entry;
            entry.sourceFile = sourceIdentifier;
            parsedEntries.push_back(entry);
            report.successfulParses++;
        } else {
            // Construct LogParseError for flushed entries
            std::string errorMessageStr = result.errorMessage;
            std::string combinedMessage = errorMessageStr + " - Failing part: " + result.failingPart;
            // Use 0 or a special value for flushed entries if line number is not applicable
            report.parseErrors.emplace_back(LogParseError{ParseError::PARTIAL_FAILURE, combinedMessage, 0}); 
        }
    }

    if (!report.parseErrors.empty()) {
        report.status = ParseError::PARTIAL_FAILURE;
    }
    
    return {parsedEntries, report};
}

void LogAnalyzer::clear() {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    entries_.clear();
    levelCounts.clear();
    lastReport = {};
}

// New: loadAndReplace using current settings
AnalysisReport LogAnalyzer::loadAndReplace(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    std::ifstream file(filePath);
    if (!file.is_open()) {
        AnalysisReport report;
        report.status = ParseError::FILE_OPEN_FAILED;
        report.parseErrors.emplace_back(LogParseError{ParseError::FILE_OPEN_FAILED, "Could not open file.", 0});
        lastReport = report; // Update lastReport
        return report;
    }

    entries_.clear(); // Clear existing entries
    auto [parsedEntries, report] = parseAndReport(file, filePath);
    entries_ = std::move(parsedEntries); // Replace entries
    
    // Sort entries by timestamp
    std::sort(entries_.begin(), entries_.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    lastReport = report; // Update lastReport
    return report;
}

// Deprecated: loadAndReplace with explicit pattern
AnalysisReport LogAnalyzer::loadAndReplace(const std::string& filePath, const std::string& pattern) {
    // Temporarily change settings for this call
    LogAnalyzerSettings oldSettings = getSettings();
    LogAnalyzerSettings tempSettings = oldSettings;
    tempSettings.lineParsePattern = pattern;
    if (pattern == DEFAULT_LOG_REGEX_PATTERN) {
        tempSettings.fieldMappings.clear();
        tempSettings.fieldMappings.emplace_back(LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S");
        tempSettings.fieldMappings.emplace_back(LogEntryField::LEVEL, 2);
        tempSettings.fieldMappings.emplace_back(LogEntryField::MESSAGE, 3);
    } else {
        tempSettings.fieldMappings.clear();
    }
    
    if (auto res = setSettings(tempSettings); !res) { // This re-initializes currentParser_
        AnalysisReport report;
        report.status = res.error().code;
        report.parseErrors.push_back(res.error());
        // Log the error, but still try to restore settings if possible
        std::cerr << "Error setting temporary settings: " << res.error().message << std::endl;
        
        if (auto restore_res = setSettings(oldSettings); !restore_res) {
            std::cerr << "Error restoring settings after temp set failed: " << restore_res.error().message << std::endl;
        }
        return report;
    }

    AnalysisReport report = loadAndReplace(filePath); // Call the new, non-deprecated version

    if (auto res = setSettings(oldSettings); !res) { // Restore original settings
        std::cerr << "Error restoring settings: " << res.error().message << std::endl;
        // If restoring fails, we should consider how this affects the report.
        // For now, we'll return the report from the loadAndReplace call.
    }
    return report;
}

const std::vector<LogEntry>& LogAnalyzer::getEntries() const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    return entries_;
}

// New: Get filtered entries using a custom predicate function.
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
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    currentSettings_.customLogLevelMappings[std::string(levelString)] = mappedLevel;
    // Re-initialize parser with new settings
    currentParser_ = std::make_unique<DefaultLogParser>(currentSettings_.lineParsePattern, currentSettings_.fieldMappings, currentSettings_.customLogLevelMappings);
}

// New: Load from a file using current settings
std::expected<AnalysisReport, LogParseError> LogAnalyzer::load(const std::string& filePath) {
    AnalysisReport report = loadAndReplace(filePath);
    if (report.status == ParseError::FILE_OPEN_FAILED || report.status == ParseError::INVALID_REGEX_PATTERN) {
        return std::unexpected(LogParseError{report.status, report.parseErrors.empty() ? "" : report.parseErrors[0].message, 0});
    }
    return report;
}

// Deprecated: load with explicit pattern
std::expected<void, LogParseError> LogAnalyzer::load(const std::string& filePath, const std::string& pattern) {
    // This calls loadAndReplace (deprecated version) which is already locked.
    // No additional lock needed here unless we were directly manipulating entries_ or lastReport.
    AnalysisReport report = loadAndReplace(filePath, pattern); // Call the deprecated loadAndReplace
    if (report.status == ParseError::FILE_OPEN_FAILED || report.status == ParseError::INVALID_REGEX_PATTERN) {
        return std::unexpected(LogParseError{report.status, report.parseErrors.empty() ? "" : report.parseErrors[0].message, 0});
    }
    return {};
}

// New: Asynchronous load using current settings
std::future<std::expected<AnalysisReport, LogParseError>> LogAnalyzer::loadAsync(const std::string& filePath) {
    return std::async(std::launch::async, [this, filePath]() {
        return load(filePath); // Call the new, non-deprecated load
    });
}

// Deprecated: loadAsync with explicit pattern
std::future<AnalysisReport> LogAnalyzer::loadAsync(const std::string& filePath, const std::string& pattern) {
    return std::async(std::launch::async, [this, filePath, pattern]() {
        // Temporarily change settings for this call within the async task
        LogAnalyzerSettings oldSettings = getSettings();
        LogAnalyzerSettings tempSettings = oldSettings;
        tempSettings.lineParsePattern = pattern;
        if (pattern == DEFAULT_LOG_REGEX_PATTERN) {
            tempSettings.fieldMappings.clear();
            tempSettings.fieldMappings.emplace_back(LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S");
            tempSettings.fieldMappings.emplace_back(LogEntryField::LEVEL, 2);
            tempSettings.fieldMappings.emplace_back(LogEntryField::MESSAGE, 3);
        } else {
            tempSettings.fieldMappings.clear();
        }

        if (auto res = setSettings(tempSettings); !res) {
            AnalysisReport report;
            report.status = res.error().code;
            report.parseErrors.push_back(res.error());
            return report;
        }

        AnalysisReport report = loadAndReplace(filePath); // Call the deprecated loadAndReplace

        if (auto res = setSettings(oldSettings); !res) { // Restore original settings
            std::cerr << "Error restoring settings: " << res.error().message << std::endl;
        }
        return report;
    });
}

// New: Stream in log entries from an istream using current settings
std::expected<AnalysisReport, LogParseError> LogAnalyzer::streamIn(std::istream& is, const std::string& sourceIdentifier) {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    auto [newEntries, report] = parseAndReport(is, sourceIdentifier);

    // Sort new entries
    std::sort(newEntries.begin(), newEntries.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    // Merge new entries into existing sorted entries_ using std::inplace_merge
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

// New: analyzeStream overload using current settings.
std::expected<void, LogParseError> LogAnalyzer::analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback) {
    // This function processes streams and doesn't directly modify `entries_` or `lastReport`.
    // It's intended for callbacks and doesn't directly interact with the thread-safe members.
    // Therefore, no lock is needed here, as it operates independently of the analyzer's main state.
    for (const auto& filePath : filePaths) {
        std::istream* input;
        std::ifstream file;
        if (filePath == Utils::STDIN_FILE_PATH) {
            input = &std::cin;
        } else {
            file.open(filePath);
            if (!file.is_open()) {
                 return std::unexpected(LogParseError{ParseError::FILE_OPEN_FAILED, "Could not open file: " + filePath, 0});
            }
            input = &file;
        }

        std::string line;
        size_t lineNumber = 0;
        bool shouldContinue = true;
        while (std::getline(*input, line)) {
            lineNumber++;
            auto parseResultOpt = currentParser_->processLine(line, lineNumber);
            if (parseResultOpt.has_value()) {
                const auto& result = parseResultOpt.value();
                if (result.success) {
                    LogEntry entry = result.entry;
                    entry.sourceFile = (filePath == "-" ? "stdin" : filePath);
                    if (!entryCallback(entry)) {
                        shouldContinue = false;
                        break;
                    }
                } else {
                    LogEntry partialEntry;
                    partialEntry.level = LogLevel::UNKNOWN;
                    partialEntry.message = line;
                    partialEntry.sourceFile = (filePath == "-" ? "stdin" : filePath);
                    partialEntry.id = lineNumber;
                    if (!entryCallback(partialEntry)) {
                        shouldContinue = false;
                        break;
                    }
                }
            }
            if (!shouldContinue) break;
        }
        // Flush any remaining buffered multi-line entries
        auto flushResults = currentParser_->flushRemaining();
        for (const auto& result : flushResults) {
            if (result.success) {
                LogEntry entry = result.entry;
                entry.sourceFile = (filePath == "-" ? "stdin" : filePath);
                if (!entryCallback(entry)) {
                    shouldContinue = false;
                    break;
                }
            } else {
                LogEntry partialEntry;
                partialEntry.level = LogLevel::UNKNOWN;
                partialEntry.message = result.failingPart.empty() ? "Unparseable buffered multi-line entry" : result.failingPart;
                partialEntry.sourceFile = (filePath == "-" ? "stdin" : filePath);
                partialEntry.id = 0; // No specific line number for flushed entries
                if (!entryCallback(partialEntry)) {
                    shouldContinue = false;
                    break;
                }
            }
            if (!shouldContinue) break;
        }


        if (!shouldContinue) break;
    }
    return {};
}

std::expected<void, LogParseError> LogAnalyzer::analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, const std::string& pattern) {
    // Temporarily change settings for this call
    LogAnalyzerSettings oldSettings = getSettings();
    LogAnalyzerSettings tempSettings = oldSettings;
    tempSettings.lineParsePattern = pattern;
    if (pattern == DEFAULT_LOG_REGEX_PATTERN) {
        tempSettings.fieldMappings.clear();
        tempSettings.fieldMappings.emplace_back(LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S");
        tempSettings.fieldMappings.emplace_back(LogEntryField::LEVEL, 2);
        tempSettings.fieldMappings.emplace_back(LogEntryField::MESSAGE, 3);
    } else {
        tempSettings.fieldMappings.clear();
    }
    
    // Use the new setSettings which returns std::expected
    if (auto res = setSettings(tempSettings); !res) {
        // Propagate the error from setSettings if parser creation fails
        return std::unexpected(res.error());
    }

    auto result = analyzeStream(filePaths, entryCallback); // Call the new, non-deprecated version

    if (auto res = setSettings(oldSettings); !res) { // Restore original settings
        // Log error during restore but still return the result of the operation
        std::cerr << "Error restoring settings: " << res.error().message << std::endl;
    }
    
    return result;
}

// Audit: Inefficient append() sorting. Change to use std::inplace_merge.
// The current implementation uses std::merge into a new vector, which is not ideal but functional.
// Let's try to optimize it to use inplace_merge.
// New: Append using current settings
std::expected<AnalysisReport, LogParseError> LogAnalyzer::append(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return std::unexpected(LogParseError{ParseError::FILE_OPEN_FAILED, "Could not open file.", 0});
    }

    auto [newEntries, report] = parseAndReport(file, filePath);
    
    // Sort new entries
    std::sort(newEntries.begin(), newEntries.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    // Merge new entries into existing sorted entries_ using std::inplace_merge
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

// Deprecated: append with explicit pattern
std::expected<void, LogParseError> LogAnalyzer::append(const std::string& filePath, const std::string& pattern) {
    // Temporarily change settings for this call
    LogAnalyzerSettings oldSettings = getSettings();
    LogAnalyzerSettings tempSettings = oldSettings;
    tempSettings.lineParsePattern = pattern;
    if (pattern == DEFAULT_LOG_REGEX_PATTERN) {
        tempSettings.fieldMappings.clear();
        tempSettings.fieldMappings.emplace_back(LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S");
        tempSettings.fieldMappings.emplace_back(LogEntryField::LEVEL, 2);
        tempSettings.fieldMappings.emplace_back(LogEntryField::MESSAGE, 3);
    } else {
        tempSettings.fieldMappings.clear();
    }

    if (auto res = setSettings(tempSettings); !res) {
        return std::unexpected(res.error());
    }

    auto result = append(filePath); // Call the new, non-deprecated version

    if (auto res = setSettings(oldSettings); !res) { // Restore original settings
        std::cerr << "Error restoring settings: " << res.error().message << std::endl;
    }

    if (!result.has_value()) {
        return std::unexpected(result.error());
    }
    return {};
}

std::span<const LogEntry> LogAnalyzer::getEntriesView() const { // Renamed from entries_view
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    return entries_;
}

// New: Format an entry with advanced options
std::string LogAnalyzer::formatEntry(const LogEntry& entry, std::string_view format, const FormattingOptions& options) const {
    std::string formattedString(format);

    // Replace common placeholders
    Utils::replaceAll(formattedString, "{timestamp}", formatTimestamp(entry.timestamp, options.dateTimeFormat));
    
    std::string levelString = logLevelToString(entry.level);
    if (options.useColor) {
        std::string colorCode;
        switch (entry.level) {
            case LogLevel::FATAL:
            case LogLevel::ERROR:   colorCode = "\033[31m"; break; // Red
            case LogLevel::WARNING: colorCode = "\033[33m"; break; // Yellow
            case LogLevel::INFO:    colorCode = "\033[32m"; break; // Green
            case LogLevel::DEBUG:   colorCode = "\033[34m"; break; // Blue
            case LogLevel::TRACE:   colorCode = "\033[36m"; break; // Cyan
            default:                colorCode = "\033[0m";  break; // Reset
        }
        Utils::replaceAll(formattedString, "{level}", colorCode + levelString + "\033[0m");
    } else {
        Utils::replaceAll(formattedString, "{level}", levelString);
    }
    
    Utils::replaceAll(formattedString, "{message}", entry.message);
    Utils::replaceAll(formattedString, "{sourceFile}", entry.sourceFile);
    Utils::replaceAll(formattedString, "{id}", std::to_string(entry.id));

    // Handle structured fields
    if (options.includeStructuredFields && !entry.structuredFields.empty()) {
        std::ostringstream ss;
        bool firstField = true;
        for (const auto& [key, value] : entry.structuredFields) {
            if (!firstField) {
                ss << options.structuredFieldDelimiter;
            }
            ss << key << options.structuredFieldKvDelimiter << value;
            firstField = false;
        }
        Utils::replaceAll(formattedString, "{structuredFields}", ss.str());
    } else {
        Utils::replaceAll(formattedString, "{structuredFields}", "");
    }
    
    return formattedString;
}

// Deprecated: Original formatEntry
std::string LogAnalyzer::formatEntry(const LogEntry& entry, std::string_view format, bool useColor) const {
    FormattingOptions options;
    options.useColor = useColor;
    options.dateTimeFormat = std::string(format); // Assuming format string is for datetime
    // Other options are default, structured fields are not included by default for deprecated version
    
    // We need a proper format string for the message and level.
    // For the deprecated version, we assume a default output like "{timestamp} {level}: {message}"
    // and rely on the new formatEntry to construct this.
    // If the original format was just for timestamp, then we must be careful.
    // The original behavior of this function was to format the timestamp and then append
    // the level and message. Let's replicate that.

    std::string defaultFormat = "{timestamp} {level}: {message}";
    if (format.empty() || format == "%Y-%m-%d %H:%M:%S") { // Default format used if not specified
        options.dateTimeFormat = "%Y-%m-%d %H:%M:%S";
    } else {
        options.dateTimeFormat = std::string(format); // Use provided format for datetime
    }

    return formatEntry(entry, defaultFormat, options);
}

std::string LogAnalyzer::logLevelToString(LogLevel level) const {
    return Utils::logLevelToString(level);
}

LogLevel LogAnalyzer::stringToLogLevel(const std::string& levelStr) {
    return Utils::stringToLogLevel(levelStr);
}

// New: Print filtered entries with advanced formatting options.
void LogAnalyzer::printFilteredEntries(std::ostream& out, const FilterCriteria& criteria, const FormattingOptions& options) const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
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

// Deprecated: Original printFilteredEntries
void LogAnalyzer::printFilteredEntries(std::ostream& out, const FilterCriteria& criteria, std::string_view formatString) const {
    FormattingOptions options;
    options.dateTimeFormat = std::string(formatString); // Assume formatString primarily affects datetime
    // For backward compatibility, the deprecated version did not include structured fields by default
    // or provide explicit control over other FormattingOptions.
    // The default format passed to formatEntry will handle the basic structure.
    printFilteredEntries(out, criteria, options);
}
