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

LogAnalyzer::LogAnalyzer() : defaultParser_(std::make_unique<DefaultLogParser>(std::string(LogAnalyzer::DEFAULT_LOG_REGEX_PATTERN))) {}

void LogAnalyzer::clear() {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    entries_.clear();
    levelCounts.clear();
    lastReport = {};
}

AnalysisReport LogAnalyzer::loadAndReplace(const std::string &filePath, const std::string &pattern) {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    std::ifstream file(filePath);
    if (!file.is_open()) {
        lastReport.status = ParseError::FILE_OPEN_FAILED;
        lastReport.parseErrors.emplace_back(LogParseError{ParseError::FILE_OPEN_FAILED, "Could not open file.", 0});
        return lastReport;
    }

    std::unique_ptr<ILogParser> parser;
    if (!pattern.empty()) {
        try {
            std::vector<FieldMapping> currentFieldMappings;
            if (pattern == LogAnalyzer::DEFAULT_LOG_REGEX_PATTERN) {
                // If using the default regex pattern, infer the field mappings
                currentFieldMappings.emplace_back(LogEntryField::TIMESTAMP, 1, std::string("%Y-%m-%d %H:%M:%S"));
                currentFieldMappings.emplace_back(LogEntryField::LEVEL, 2);
                currentFieldMappings.emplace_back(LogEntryField::MESSAGE, 3);
            }
            // For custom patterns, the original code passed an empty vector,
            // implying the custom regex itself should handle parsing or mapping.
            // We maintain this behavior for custom patterns.
            parser = std::make_unique<DefaultLogParser>(pattern, currentFieldMappings, customLevelMappings);
        } catch (const std::regex_error& e) {
            lastReport.status = ParseError::INVALID_REGEX_PATTERN;
            lastReport.parseErrors.emplace_back(LogParseError{ParseError::INVALID_REGEX_PATTERN, e.what(), 0});
            return lastReport;
        }
    } else {
        // If no pattern is provided, use the default parser's regex, but apply custom level mappings.
        // Ensure we are using the new constructor correctly here too.
        parser = std::make_unique<DefaultLogParser>(defaultParser_->getLineFilterRegex(), std::vector<FieldMapping>{}, customLevelMappings);
    }

    std::string line;
    size_t lineNumber = 0;
    entries_.clear(); // Clear existing entries when analyzing a new file
    lastReport = {};  // Reset report
    while (std::getline(file, line)) {
        lineNumber++;
        lastReport.linesProcessed++;
        auto result = parser->parseLine(line, lineNumber);
        if (result.success) {
            result.entry->sourceFile = filePath;
            lastReport.successfulParses++;
            entries_.push_back(*result.entry);
        } else {
            lastReport.parseErrors.emplace_back(LogParseError{ParseError::PARTIAL_FAILURE, result.errorMessage, lineNumber});
            // Store even unparseable lines as UNKNOWN entries for context.
            LogEntry partialEntry;
            partialEntry.level = LogLevel::UNKNOWN;
            partialEntry.message = line;
            partialEntry.sourceFile = filePath;
            partialEntry.id = lineNumber; // Assign line number as ID
            entries_.push_back(partialEntry);

            // Debugging output for parse failures
            std::cerr << "Parse Error on line " << lineNumber << ": " << result.errorMessage << std::endl;
            std::cerr << "Failing part: " << result.failingPart << std::endl;
        }
    }

    if (!lastReport.parseErrors.empty()) {
        lastReport.status = ParseError::PARTIAL_FAILURE;
    } else {
        lastReport.status = ParseError::SUCCESS;
    }
    
    // Sort entries by timestamp. This is currently O(N log N).
    // Audit: Inefficient sorting in analyze(). Needs optimization.
    // For now, ensure thread safety. Optimization for sorting and appending is a separate step.
    std::sort(entries_.begin(), entries_.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    return lastReport;
}

const std::vector<LogEntry>& LogAnalyzer::getEntries() const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    return entries_;
}

void LogAnalyzer::setCustomLogLevelMapping(std::string_view levelString, LogLevel mappedLevel) {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    customLevelMappings[std::string(levelString)] = mappedLevel;
}

std::expected<void, LogParseError> LogAnalyzer::load(const std::string& filePath, const std::string& pattern) {
    // This calls analyze which is already locked.
    // No additional lock needed here unless we were directly manipulating entries_ or lastReport.
    AnalysisReport report = loadAndReplace(filePath, pattern); // Changed from analyze
    if (report.status == ParseError::FILE_OPEN_FAILED) {
        return std::unexpected(LogParseError{report.status, "Could not open file.", 0});
    }
    return {};
}

std::future<AnalysisReport> LogAnalyzer::loadAsync(const std::string& filePath, const std::string& pattern) {
    // The analyze method already acquires a lock on mutex_. This might cause a deadlock
    // if the main thread also tries to acquire the lock while processing the future.
    // A safer approach would be to have a non-locking version of the core parsing logic,
    // and then apply the lock in the public methods like analyze() and load().
    // For now, we rely on the fact that analyze() will be executed in a separate thread.
    return std::async(std::launch::async, [this, filePath, pattern]() {
        // Note: analyze() already acquires a lock on mutex_. This might cause a deadlock
        // if the main thread also tries to acquire the lock while processing the future.
        // A safer approach would be to have a non-locking version of the core parsing logic,
        // and then apply the lock in the public methods like analyze() and load().
        // For now, we rely on the fact that analyze() will be executed in a separate thread.
        return loadAndReplace(filePath, pattern); // Changed from analyze
    });
}

std::expected<void, LogParseError> LogAnalyzer::analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, const std::string& pattern) {
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
                 // Return structured error information instead of printing to cerr.
                 return std::unexpected(LogParseError{ParseError::FILE_OPEN_FAILED, "Could not open file: " + filePath, 0});
            }
            input = &file;
        }

        std::unique_ptr<ILogParser> parser;
        if (!pattern.empty()) {
            try {
                 // Use the new constructor with an empty vector for fieldMappings.
                 parser = std::make_unique<DefaultLogParser>(pattern, std::vector<FieldMapping>{}, customLevelMappings);
            } catch (const std::regex_error& e) {
                 // Cannot report this error via `lastReport` as it's not locked/owned here.
                 // Return structured error information instead of printing to cerr.
                 return std::unexpected(LogParseError{ParseError::INVALID_REGEX_PATTERN, "Error creating parser for pattern: " + pattern + " - " + e.what(), 0});
            }
        } else {
            // If no pattern is provided, use the default parser's regex.
            // Ensure we are using the new constructor correctly here too.
            parser = std::make_unique<DefaultLogParser>(defaultParser_->getLineFilterRegex(), std::vector<FieldMapping>{}, customLevelMappings);
        }

        std::string line;
        size_t lineNumber = 0;
        bool shouldContinue = true;
        while (std::getline(*input, line)) {
            lineNumber++;
            auto result = parser->parseLine(line, lineNumber);
            if (result.success) {
                result.entry->sourceFile = (filePath == "-" ? "stdin" : filePath);
                if (!entryCallback(*result.entry)) {
                    shouldContinue = false;
                    break; // Callback returned false, stop processing this file
                }
            } else {
                LogEntry partialEntry;
                partialEntry.level = LogLevel::UNKNOWN;
                partialEntry.message = line;
                partialEntry.sourceFile = (filePath == "-" ? "stdin" : filePath);
                partialEntry.id = lineNumber; // Assign line number as ID
                if (!entryCallback(partialEntry)) {
                    shouldContinue = false;
                    break; // Callback returned false, stop processing this file
                }
            }
        }
        // If the file was opened, it will be closed automatically when 'file' goes out of scope.
        if (!shouldContinue) break; // Stop processing further files if callback requested it
    }
    return {}; // Return success if loop completed without errors
}

// Audit: Inefficient append() sorting. Change to use std::inplace_merge.
// The current implementation uses std::merge into a new vector, which is not ideal but functional.
// Let's try to optimize it to use inplace_merge.
std::expected<void, LogParseError> LogAnalyzer::append(const std::string& filePath, const std::string& pattern) {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    std::ifstream file(filePath);
    if (!file.is_open()) {
         return std::unexpected(LogParseError{ParseError::FILE_OPEN_FAILED, "Could not open file.", 0});
    }

    std::unique_ptr<ILogParser> parser;
    if (!pattern.empty()) {
        try {
            // Use the new constructor with an empty vector for fieldMappings.
            parser = std::make_unique<DefaultLogParser>(pattern, std::vector<FieldMapping>{}, customLevelMappings);
        } catch (const std::regex_error& e) {
            return std::unexpected(LogParseError{ParseError::INVALID_REGEX_PATTERN, e.what(), 0});
        }
    } else {
        // If no pattern is provided, use the default parser's regex.
        // Ensure we are using the new constructor correctly here too.
        parser = std::make_unique<DefaultLogParser>(defaultParser_->getLineFilterRegex(), std::vector<FieldMapping>{}, customLevelMappings);
    }

    std::vector<LogEntry> newEntries;
    std::string line;
    size_t lineNumber = 0;
    while (std::getline(file, line)) {
        lineNumber++;
        auto result = parser->parseLine(line, lineNumber);
        if (result.success) {
            result.entry->sourceFile = filePath;
            newEntries.push_back(*result.entry);
        } else {
            LogEntry partialEntry;
            partialEntry.level = LogLevel::UNKNOWN;
            partialEntry.message = line;
            partialEntry.sourceFile = filePath;
            partialEntry.id = lineNumber; // Assign line number as ID
            newEntries.push_back(partialEntry);
        }
    }

    // Sort new entries
    std::sort(newEntries.begin(), newEntries.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    // Merge new entries into existing sorted entries_ using std::inplace_merge
    // This requires that both 'entries_' and 'newEntries' are sorted.
    // 'entries_' is assumed to be sorted from previous operations (analyze, load, append).
    // 'newEntries' is sorted above.
    if (!newEntries.empty()) {
        // Insert all new elements at the end of `entries_`
        entries_.insert(entries_.end(), 
                        std::make_move_iterator(newEntries.begin()), 
                        std::make_move_iterator(newEntries.end()));
        
        // Now, `entries_` contains original sorted entries followed by new sorted entries.
        // `inplace_merge` merges the two sorted ranges.
        // The first range is [entries_.begin(), entries_.end() - newEntries.size())
        // The second range is [entries_.end() - newEntries.size(), entries_.end())
        std::inplace_merge(entries_.begin(), entries_.end() - newEntries.size(), entries_.end(),
                           [](const LogEntry& a, const LogEntry& b) {
                               return a.timestamp < b.timestamp;
                           });
    }
    
    // The previous implementation using std::merge into a new vector and then moving was also O(N) where N is total entries.
    // This inplace_merge approach might be slightly more memory efficient by avoiding a full copy if done in-place,
    // but it requires careful handling of iterators and sizes.
    // The current inplace_merge approach requires inserting all new elements first, then merging.
    // This is correct.

    return {};
}

std::span<const LogEntry> LogAnalyzer::getEntriesView() const { // Renamed from entries_view
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    return entries_;
}

std::string LogAnalyzer::logLevelToString(LogLevel level) const {
    return Utils::logLevelToString(level);
}

LogLevel LogAnalyzer::stringToLogLevel(const std::string& levelStr) {
    return Utils::stringToLogLevel(levelStr);
}
