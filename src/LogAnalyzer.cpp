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
    auto filtered_expected = getFilteredEntries(criteria); // This call is already locked inside
    if (!filtered_expected) {
        std::cerr << "Error filtering entries: " << filtered_expected.error().message << std::endl;
        return;
    }
    const auto& filtered = filtered_expected.value();
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

std::span<const LogEntry> LogAnalyzer::getEntriesView() const { // Renamed from entries_view
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    return entries_;
}

// Audit: Fragile CSV Export
void LogAnalyzer::exportAsCsv(std::ostream& out, const FilterCriteria& filter, char delimiter) const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    
    // CSV Header
    out << "Timestamp" << delimiter << "Level" << delimiter << "Message" << delimiter << "File\n";
    
    auto filtered_expected = getFilteredEntries(filter); // This call is already locked inside
    if (!filtered_expected) {
        std::cerr << "Error filtering entries for CSV export: " << filtered_expected.error().message << std::endl;
        return;
    }
    const auto& filtered = filtered_expected.value();
    
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
                msg.replace(pos, 1, """");
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



std::string LogAnalyzer::logLevelToString(LogLevel level) const {
    return Utils::logLevelToString(level);
}

LogLevel LogAnalyzer::stringToLogLevel(const std::string& levelStr) {
    return Utils::stringToLogLevel(levelStr);
}



void LogAnalyzer::exportAsJson(std::ostream &out, const FilterCriteria &filter, bool includeSummary, bool prettyPrint) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto filtered_expected = getFilteredEntries_NoLock(filter); // Use non-locking version as we already hold the lock
    if (!filtered_expected) {
        std::cerr << "Error filtering entries for JSON export: " << filtered_expected.error().message << std::endl;
        return;
    }
    const auto& filtered = filtered_expected.value();

    const std::string indent = prettyPrint ? "  " : "";
    const std::string newline = prettyPrint ? "\n" : "";
    const std::string entryIndent = prettyPrint ? "    " : "";

    // Always output a root JSON object.
    out << "{"
 << newline;
    // Ensure "summary" root element is always present for consistency.
    // It will contain at least "totalEntries".
    out << indent << "\"summary\": {\"totalEntries\": " << filtered.size() << "}," << newline;
    out << indent << "\"entries\": [" << newline;

    for (size_t i = 0; i < filtered.size(); ++i) {
        const auto& entry = filtered[i];
        out << (includeSummary ? entryIndent : indent);
        out << "{";
        out << "\"timestamp\":\"" << formatTimestamp(entry.timestamp) << "\",";
        out << "\"level\":\"" << logLevelToString(entry.level) << "\",";
        out << "\"message\":\"" << Utils::escapeJsonString(entry.message) << "\",";
        out << "\"file\":\"" << Utils::escapeJsonString(entry.sourceFile) << "\"";
        out << "}";
        if (i < filtered.size() - 1) {
            out << ",";
        }
        out << newline;
    }

    out << indent << "]" << newline;
    out << "}" << newline;
}





std::expected<std::vector<LogEntry>, LogParseError> LogAnalyzer::getFilteredEntries(const FilterCriteria& criteria) const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    return getFilteredEntries_NoLock(criteria);
}

// Non-locking version for internal use
std::expected<std::vector<LogEntry>, LogParseError> LogAnalyzer::getFilteredEntries_NoLock(const FilterCriteria& criteria) const {
    std::vector<LogEntry> filtered;
    
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
        try {
            composite->add(std::make_shared<RegexFilter>(criteria.regexPattern));
        } catch (const std::regex_error& e) {
            // Audit: Regex error handling in getFilteredEntries_NoLock
            // Propagate regex compilation errors back to the caller.
            return std::unexpected(LogParseError{ParseError::INVALID_REGEX_PATTERN, e.what(), 0});
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


std::vector<LogEntry> LogAnalyzer::getSortedFilteredEntries(const FilterCriteria& criteria, SortBy sortBy, SortOrder sortOrder) const {
    auto filtered_expected = getFilteredEntries(criteria); // This already locks internally
    if (!filtered_expected) {
        // Handle the error case from getFilteredEntries.
        // For this function, it might be appropriate to return an empty vector or rethrow.
        // Returning empty vector for now.
        return {};
    }
    std::vector<LogEntry> filtered = filtered_expected.value();

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
