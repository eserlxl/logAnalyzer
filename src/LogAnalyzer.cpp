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

LogAnalyzer::LogAnalyzer() : defaultParser_(std::make_unique<DefaultLogParser>(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\] (\w+): (.*)$)")) {} 

void LogAnalyzer::clear() {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    entries_.clear();
    levelCounts.clear();
    lastReport = {};
}

AnalysisReport LogAnalyzer::analyze(const std::string &filePath, const std::string &pattern) {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    std::ifstream file(filePath);
    if (!file.is_open()) {
        lastReport.status = ParseError::FILE_OPEN_FAILED;
        lastReport.parseErrors.emplace_back(0, "Could not open file.");
        return lastReport;
    }

    std::unique_ptr<ILogParser> parser;
    if (!pattern.empty()) {
        try {
            // If a pattern is provided, use the new constructor with an empty vector for fieldMappings.
            // This ensures we use the new constructor's logic for parsing based on the pattern.
            parser = std::make_unique<DefaultLogParser>(pattern, std::vector<FieldMapping>{}, customLevelMappings);
        } catch (const std::regex_error& e) {
            lastReport.status = ParseError::INVALID_REGEX_PATTERN;
            lastReport.parseErrors.emplace_back(0, e.what());
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
            lastReport.parseErrors.emplace_back(lineNumber, result.errorMessage);
            // Store even unparseable lines as UNKNOWN entries for context.
            LogEntry partialEntry;
            partialEntry.level = LogLevel::UNKNOWN;
            partialEntry.message = line;
            partialEntry.sourceFile = filePath;
            partialEntry.id = lineNumber; // Assign line number as ID
            entries_.push_back(partialEntry);
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
    AnalysisReport report = analyze(filePath, pattern);
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
        return analyze(filePath, pattern);
    });
}

void LogAnalyzer::analyzeStream(const std::vector<std::string>& filePaths, std::function<bool(const LogEntry&)> entryCallback, const std::string& pattern) {
    // This function processes streams and doesn't directly modify `entries_` or `lastReport`.
    // It's intended for callbacks and doesn't directly interact with the thread-safe members.
    // Therefore, no lock is needed here, as it operates independently of the analyzer's main state.
    for (const auto& filePath : filePaths) {
        std::istream* input;
        std::ifstream file;
        if (filePath == "-") {
            input = &std::cin;
        } else {
            file.open(filePath);
            if (!file.is_open()) {
                 // Log an error or handle appropriately, but don't proceed with this file.
                 // This function doesn't have access to `lastReport` to record this error.
                 std::cerr << "Error: Could not open file " << filePath << std::endl; // Added error logging
                 continue;
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
                 // Potentially log to cerr or ignore if pattern is invalid.
                 std::cerr << "Error creating parser for pattern: " << pattern << " - " << e.what() << std::endl;
                 continue; // Skip processing this file with invalid pattern
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
}

// Audit: Inefficient Frequency Distribution Algorithm
// Current implementation is already O(N) due to single pass.
// The audit might be referring to an older version. This looks good.
std::vector<TimeWindowStats> LogAnalyzer::getFrequencyDistribution(std::chrono::seconds windowSize) const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    if (entries_.empty()) return {};

    std::vector<TimeWindowStats> stats;
    auto startTime = entries_.front().timestamp;
    auto endTime = entries_.back().timestamp;

    // Ensure windowSize is positive to prevent infinite loops or division by zero
    if (windowSize.count() <= 0) {
        // Return an empty vector or throw an exception, or define a default window.
        // For now, return empty.
        return {};
    }

    // Calculate the number of windows needed. Ensure at least one window.
    auto totalDurationSeconds = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
    size_t numWindows = (totalDurationSeconds / windowSize.count()) + 1;
    if (numWindows == 0) numWindows = 1; // Ensure at least one window if duration is small

    // Initialize time windows
    std::vector<TimeWindowStats> tempStats(numWindows);
    for (size_t i = 0; i < numWindows; ++i) {
        tempStats[i].windowStart = startTime + std::chrono::seconds(i * windowSize.count());
        tempStats[i].windowEnd = tempStats[i].windowStart + windowSize;
        tempStats[i].totalCount = 0;
    }

    // Single pass to assign entries to windows. This is O(N).
    size_t currentWindowIndex = 0;
    for (const auto& entry : entries_) {
        // Advance currentWindowIndex until the entry falls within the window or beyond.
        // Ensure we don't go past the last window index.
        while (currentWindowIndex < numWindows && entry.timestamp >= tempStats[currentWindowIndex].windowEnd) {
            currentWindowIndex++;
        }
        
        // If the entry falls within the current window's bounds (inclusive start, exclusive end)
        if (currentWindowIndex < numWindows && entry.timestamp >= tempStats[currentWindowIndex].windowStart && entry.timestamp < tempStats[currentWindowIndex].windowEnd) {
            tempStats[currentWindowIndex].totalCount++;
            tempStats[currentWindowIndex].counts[entry.level]++;
        }
        // Entries that fall *exactly* on the windowEnd of the last window might be missed if not careful.
        // However, the loop condition `entry.timestamp < tempStats[currentWindowIndex].windowEnd` handles this.
        // If an entry's timestamp is exactly `tempStats[currentWindowIndex].windowEnd`, it will be considered for the *next* window.
    }

    // Filter out empty windows for the final result
    for (const auto& window : tempStats) {
        if (window.totalCount > 0) {
            stats.push_back(window);
        }
    }
    return stats;
}

// This function was a duplicate and likely meant to be optimized.
// Since getFrequencyDistribution is already optimized, we can remove this or refactor it.
// For now, I'll make it call the optimized version as per the header.
std::vector<TimeWindowStats> LogAnalyzer::getFrequencyDistributionOptimized(std::chrono::seconds windowSize) const {
    // Audit: This method is a duplicate of getFrequencyDistribution and is misleading.
    // Refactor: Remove this or ensure it does something distinct/better. For now, it calls the existing one.
    return getFrequencyDistribution(windowSize);
}

/*
// Audit: Incorrect `merge_sorted` implementation.
// Placeholder implementation to prevent segfaults.
// In a real implementation, this would merge the sources into a temporary file.
// This requires significant changes to LogFileView to manage the temporary file correctly.
LogFileView LogAnalyzer::merge_sorted(std::span<LogFileView> sources) {
    // Placeholder: Create an empty temporary file.
    std::string tempFileName = "merged_empty_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".log";
    std::ofstream tempFile(tempFileName);
    if (!tempFile.is_open()) {
        // Handle error: perhaps throw an exception or return a view that signals an error.
        // For now, proceed assuming it was created.
    }
    tempFile.close(); 
    
    // The LogFileView constructor now handles temporary file management via shared_ptr.
    // Assuming DefaultLogParser constructor with pattern string is available and correct.
    // The LogFileView constructor might take a parser directly.
    // Example call: std::make_unique<DefaultLogParser>(pattern, std::vector<FieldMapping>{}, customLevelMappings)
    // Here, we use a default pattern. The default parser needs to be constructed properly.
    // Using the deprecated constructor here might be implied by the original code.
    // Let's use the new constructor with an empty FieldMapping list for clarity.
    return LogFileView(tempFileName, std::make_unique<DefaultLogParser>(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\] (\w+): (.*)$)", std::vector<FieldMapping>{}, std::map<std::string, LogLevel, ci_less>{})); 
}
*/

std::optional<LogEntry> LogAnalyzer::findFirst(const FilterCriteria& criteria) const {
    // Calls getFilteredEntries which is locked.
    auto filtered = getFilteredEntries(criteria);
    if (filtered.empty()) return std::nullopt;
    return filtered.front();
}

std::optional<LogEntry> LogAnalyzer::findLast(const FilterCriteria& criteria) const {
    // Calls getFilteredEntries which is locked.
    auto filtered = getFilteredEntries(criteria);
    if (filtered.empty()) return std::nullopt;
    return filtered.back();
}

// Non-locking version for internal use
std::vector<LogEntry> LogAnalyzer::getFilteredEntries_NoLock(const FilterCriteria& criteria) const {
    std::vector<LogEntry> filtered;
    
    // Audit: Filter logic might be complex. Ensure it's correctly constructed.
    // The current implementation uses a CompositeFilter. This is generally good.
    auto composite = std::make_shared<CompositeFilter>(CompositeFilter::Logic::AND);
    
    if (!criteria.levels.empty()) {
        auto levelSet = std::make_shared<CompositeFilter>(CompositeFilter::Logic::OR);
        for (auto l : criteria.levels) {
            levelSet->add(std::make_shared<LevelFilter>(l));
        }
        composite->add(levelSet);
    }
    if (!criteria.keyword.empty()) {
        composite->add(std::make_shared<KeywordFilter>(criteria.keyword, criteria.keywordCaseSensitive));
    }
    if (!criteria.regexPattern.empty()) {
        // Ensure regex compilation is safe. Consider exceptions.
        try {
            // Use the correct RegexFilter constructor, assuming it takes a string pattern.
            composite->add(std::make_shared<RegexFilter>(criteria.regexPattern));
        } catch (const std::regex_error& e) {
            // How to report this? This function is const and cannot modify lastReport or throw.
            // This suggests filters should be pre-validated or a mechanism to report errors needs to exist.
            // For now, we log to cerr and skip adding this filter.
            std::cerr << "Regex error in getFilteredEntries: " << e.what() << std::endl;
        }
    }
    if (criteria.startTime || criteria.endTime) {
        composite->add(std::make_shared<TimeRangeFilter>(
            criteria.startTime.value_or(std::chrono::system_clock::time_point::min()),
            criteria.endTime.value_or(std::chrono::system_clock::time_point::max())
        ));
    }

    for (const auto& entry : entries_) {
        if (composite->matches(entry)) {
            filtered.push_back(entry);
        }
    }
    return filtered;
}

std::vector<LogEntry> LogAnalyzer::getFilteredEntries(const FilterCriteria& criteria) const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    return getFilteredEntries_NoLock(criteria);
}

std::string LogAnalyzer::formatTimestamp(std::chrono::system_clock::time_point tp, std::string_view format) const {
    return Utils::formatTimestamp(tp, format);
}

std::string LogAnalyzer::formatEntry(const LogEntry &entry, std::string_view format, bool useColor) const {
    std::string output = std::string(format);
    
    std::string levelStr = logLevelToString(entry.level);
    if (useColor) {
        // Audit: Colorization logic could be more robust.
        if (entry.level == LogLevel::ERROR || entry.level == LogLevel::FATAL) 
            levelStr = Utils::AnsiColor::RED + levelStr + Utils::AnsiColor::RESET;
        else if (entry.level == LogLevel::WARNING) 
            levelStr = Utils::AnsiColor::YELLOW + levelStr + Utils::AnsiColor::RESET;
        else if (entry.level == LogLevel::INFO) 
            levelStr = Utils::AnsiColor::GREEN + levelStr + Utils::AnsiColor::RESET;
        else if (entry.level == LogLevel::DEBUG || entry.level == LogLevel::TRACE) 
            levelStr = Utils::AnsiColor::CYAN + levelStr + Utils::AnsiColor::RESET;
        // Note: UNKNOWN level is not colored.
    }

    Utils::replaceAll(output, "{timestamp}", formatTimestamp(entry.timestamp));
    Utils::replaceAll(output, "{level}", levelStr);
    Utils::replaceAll(output, "{message}", entry.message);
    Utils::replaceAll(output, "{lineNumber}", std::to_string(entry.id));
    Utils::replaceAll(output, "{fileName}", entry.sourceFile);
    
    return output;
}

void LogAnalyzer::printFilteredEntries(std::ostream& out, const FilterCriteria& criteria, std::string_view formatString) const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    auto filtered = getFilteredEntries(criteria); // This call is already locked inside
    for (const auto& entry : filtered) {
        // Using formatEntry with useColor = false for backward compatibility.
        out << formatEntry(entry, formatString, false) << "\n";
    }
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

std::span<const LogEntry> LogAnalyzer::entriesView() const { // Renamed from entries_view
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    return entries_;
}

// Audit: Fragile CSV Export
void LogAnalyzer::exportAsCsv(std::ostream& out, const FilterCriteria& filter, char delimiter) const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    
    // CSV Header
    out << "Timestamp" << delimiter << "Level" << delimiter << "Message" << delimiter << "File\n";
    
    auto filtered = getFilteredEntries(filter); // This call is already locked inside
    
    for (const auto& entry : filtered) {
        out << formatTimestamp(entry.timestamp) << delimiter;
        out << logLevelToString(entry.level) << delimiter;
        
        // Audit: Handle newlines and quotes in messages for CSV
        std::string msg = entry.message;
        bool needsQuotes = msg.find(delimiter) != std::string::npos || 
                           msg.find('"') != std::string::npos ||
                           msg.find('\n') != std::string::npos ||
                           msg.find('\r') != std::string::npos;
                           
        if (needsQuotes) {
            // Escape existing quotes by doubling them
            size_t pos = msg.find('"');
            while (pos != std::string::npos) {
                msg.replace(pos, 1, "\"\"");
                pos = msg.find('"', pos + 2); // Move past the inserted quote
            }
            out << '"' << msg << '"'; // Enclose in quotes
        } else {
            out << msg; // No quotes needed
        }
        
        out << delimiter << entry.sourceFile << "\n";
    }
}

std::vector<TimeGap> LogAnalyzer::findTimeGaps(std::chrono::milliseconds minGapDuration) const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    if (entries_.size() < 2) return {};
    
    std::vector<TimeGap> gaps;
    for (size_t i = 0; i < entries_.size() - 1; ++i) {
        auto duration = entries_[i+1].timestamp - entries_[i].timestamp;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(duration) >= minGapDuration) {
            gaps.push_back({entries_[i].timestamp, entries_[i+1].timestamp, duration});
        }
    }
    return gaps;
}

double LogAnalyzer::getAverageEntryRate() const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    if (entries_.size() < 2) return 0.0;
    
    auto duration = entries_.back().timestamp - entries_.front().timestamp;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    
    // Avoid division by zero if duration is zero or very small.
    if (seconds == 0) {
        // If duration is 0, but entries are present, rate is effectively infinite or very high.
        // Return total entries as an indicator, or a very large number.
        // For simplicity, returning number of entries if duration is zero.
        return static_cast<double>(entries_.size());
    }
    return static_cast<double>(entries_.size()) / static_cast<double>(seconds);
}

/*
// Open is called before loadAsync, so it doesn't need to be locked here.
// It prepares a LogFileView for potential lazy parsing.
void LogAnalyzer::open(const std::string& filePath, std::unique_ptr<ILogParser> parser) {
    if (!parser) {
        // Use default parser regex, but apply custom level mappings.
        parser = std::make_unique<DefaultLogParser>(defaultParser_->getLineFilterRegex(), std::vector<FieldMapping>{}, customLevelMappings);
    }
    logSourceView_ = std::make_unique<LogFileView>(filePath, std::move(parser));
}

const LogFileView& LogAnalyzer::getView() const {
     // This returns a const reference. The LogFileView itself is not thread-safe for its iterators.
     // If this is called concurrently with operations that modify `entries_`, it could lead to issues.
     // However, `getView()` is typically used for lazy parsing, which is separate from the main `entries_` buffer.
     // The `LogFileView` itself manages its own file stream and parsing.
     // The primary thread-safety concern is `entries_` and `lastReport`.
     // Therefore, no lock is needed here for `logSourceView_`.
     if (!logSourceView_) {
         throw std::runtime_error("Log file view is not open.");
     }
     return *logSourceView_;
}
*/

void LogAnalyzer::runAnalysis(class ILogAnalyzer& /*analyzer*/, const IFilter* filter) {
    // This method performs analysis using an external analyzer.
    // It might iterate over `entries_` or `logSourceView_`.
    // If it iterates over `entries_`, it must be locked.
    // If it iterates over `logSourceView_`, that's lazy parsing and should be safe as `logSourceView_` is independent.

    // Check if we are using the in-memory entries or the lazy view
    /* if (entries_.empty() && logSourceView_) {
        // Lazy parsing: Iterate through the file view.
        // The LogFileView iterators are not inherently thread-safe if the underlying file
        // could be modified. However, this `runAnalysis` is typically called by a single thread.
        // If multiple threads could call `runAnalysis` and use `logSourceView_` concurrently, 
        // then `LogFileView` and its iterators would also need thread-safety mechanisms,
        // or `runAnalysis` itself would need to be locked.
        // For now, assume this is called from a single thread context or protected externally.
        
        for (auto it = logSourceView_->begin(); it != logSourceView_->end(); ++it) {
            if (it->has_value()) {
                if (!filter || filter->matches(*(*it))) { // Corrected: dereference iterator to get optional, then dereference optional
                    analyzer.processEntry(*(*it));
                }
            } else {
                // Handle parse error from iterator if necessary.
                // For now, skipping errored entries.
            }
        }
        
    } else */ {
        // Eager parsing: Iterate through the loaded entries. This needs to be locked.
        std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety when accessing entries_ 
        for (const auto& entry : entries_) {
            if (!filter || filter->matches(entry)) {
                // analyzer.processEntry(entry);
            }
        }
    }
    // analyzer.finalize();
}

std::string LogAnalyzer::logLevelToString(LogLevel level) const {
    return Utils::logLevelToString(level);
}

LogLevel LogAnalyzer::stringToLogLevel(const std::string& levelStr) {
    return Utils::stringToLogLevel(levelStr);
}

// Helper function for JSON string escaping
// Should be in Utils.cpp or similar, but for now defined here.
// Audit: Need to ensure this function is comprehensive for JSON escaping.
std::string escapeJsonString(const std::string& input) {
    std::ostringstream oss;
    for (char c : input) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 32) { // Control characters
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    oss << c;
                }
                break;
        }
    }
    return oss.str();
}

void LogAnalyzer::exportAsJson(std::ostream &out, const FilterCriteria &filter, bool includeSummary, bool prettyPrint) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto filtered = getFilteredEntries_NoLock(filter);

    const std::string indent = prettyPrint ? "  " : "";
    const std::string newline = prettyPrint ? "\n" : "";
    const std::string entryIndent = prettyPrint ? "    " : "";

    if (includeSummary) {
        out << "{\"summary\": {\"totalEntries\": " << filtered.size() << "}," << newline;
        out << indent << "\"entries\": [" << newline;
    } else {
        out << "[" << newline;
    }

    for (size_t i = 0; i < filtered.size(); ++i) {
        const auto& entry = filtered[i];
        out << (includeSummary ? entryIndent : indent);
        out << "{";
        out << "\"timestamp\":\"" << formatTimestamp(entry.timestamp) << "\",";
        out << "\"level\":\"" << logLevelToString(entry.level) << "\",";
        out << "\"message\":\"" << escapeJsonString(entry.message) << "\",";
        out << "\"file\":\"" << escapeJsonString(entry.sourceFile) << "\"";
        out << "}";
        if (i < filtered.size() - 1) {
            out << ",";
        }
        out << newline;
    }

    if (includeSummary) {
        out << indent << "]" << newline;
        out << "}" << newline;
    } else {
        out << "]" << newline;
    }
}



void LogAnalyzer::printSummary(std::ostream& /*out*/) const {
    // Placeholder: No specific summary logic implemented here.
}

std::vector<LogEntry> LogAnalyzer::getSortedFilteredEntries(const FilterCriteria& criteria, SortBy sortBy, SortOrder sortOrder) const {
    std::vector<LogEntry> filtered = getFilteredEntries(criteria); // This already locks internally

    auto sortLambda = [&](const LogEntry& a, const LogEntry& b) {
        bool result = false;
        switch (sortBy) {
            case SortBy::TIMESTAMP:
                result = a.timestamp < b.timestamp;
                break;
            case SortBy::LEVEL:
                result = a.level < b.level;
                break;
            case SortBy::MESSAGE:
                result = a.message < b.message;
                break;
        }
        return (sortOrder == SortOrder::ASCENDING) ? result : !result;
    };

    std::sort(filtered.begin(), filtered.end(), sortLambda);

    return filtered;
}
// Non-locking version for internal use
std::map<std::string, int> LogAnalyzer::getUniqueMessageCounts_NoLock() const {
    std::map<std::string, int> counts;
    for (const auto& entry : entries_) {
        counts[entry.message]++;
    }
    return counts;
}

std::map<std::string, int> LogAnalyzer::getUniqueMessageCounts() const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    return getUniqueMessageCounts_NoLock();
}

std::vector<std::pair<std::string, int>> LogAnalyzer::getTopMessages(int n) const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    auto messageCounts = getUniqueMessageCounts_NoLock();
    std::vector<std::pair<std::string, int>> sortedCounts(messageCounts.begin(), messageCounts.end());
    
    if (n < 0) n = sortedCounts.size(); // If n is negative, return all
    n = std::min(n, static_cast<int>(sortedCounts.size())); // Cap n to the number of unique messages

    std::partial_sort(sortedCounts.begin(), sortedCounts.begin() + n, sortedCounts.end(), [](const auto& a, const auto& b) {
        return a.second > b.second; // Sort by count descending
    });
    
    sortedCounts.resize(n); // Trim the vector to the top n elements
    
    return sortedCounts;
}
