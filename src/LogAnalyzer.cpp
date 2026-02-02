#include "LogAnalyzer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm> // For std::transform, std::sort, std::min_element, std::max_element
#include <iomanip>   // For std::get_time, std::put_time
#include <ctime>     // For std::tm
#include <format>    // For std::format (C++20)
#include <numeric>   // For std::iota, etc.
#include <chrono>    // For std::chrono utilities
#include <future>    // For std::async

// Helper function to convert string to LogLevel enum
LogLevel LogAnalyzer::stringToLogLevel(const std::string& levelStr) {
    std::string upperLevelStr = levelStr;
    std::transform(upperLevelStr.begin(), upperLevelStr.end(), upperLevelStr.begin(), ::toupper);
    if (upperLevelStr == "INFO") return LogLevel::INFO;
    if (upperLevelStr == "WARNING") return LogLevel::WARNING;
    if (upperLevelStr == "ERROR") return LogLevel::ERROR;
    if (upperLevelStr == "DEBUG") return LogLevel::DEBUG;
    return LogLevel::UNKNOWN;
}

// Helper function to convert LogLevel enum to string
std::string LogAnalyzer::logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN"; // Should not be reached
}

LogAnalyzer::LogAnalyzer() {
    clear();
}

void LogAnalyzer::clear() {
    entries.clear();
    levelCounts.clear();
    levelCounts[LogLevel::INFO] = 0;
    levelCounts[LogLevel::WARNING] = 0;
    levelCounts[LogLevel::ERROR] = 0;
    levelCounts[LogLevel::DEBUG] = 0;
    levelCounts[LogLevel::UNKNOWN] = 0;
    lastReport = AnalysisReport(); // Reset report
}

const std::vector<LogEntry>& LogAnalyzer::getEntries() const {
    return entries;
}

// View-based access (Iteration 1 Feature)
std::span<const LogEntry> LogAnalyzer::entries_view() const {
    return std::span<const LogEntry>(entries);
}

void LogAnalyzer::setCustomLogLevelMapping(std::string_view levelString, LogLevel mappedLevel) {
    std::string key(levelString);
    std::transform(key.begin(), key.end(), key.begin(), ::toupper);
    customLevelMappings[key] = mappedLevel;
}

LogLevel LogAnalyzer::resolveLogLevel(const std::string& levelStr) const {
    std::string upperLevelStr = levelStr;
    std::transform(upperLevelStr.begin(), upperLevelStr.end(), upperLevelStr.begin(), ::toupper);
    
    // Check custom mappings first
    if (auto it = customLevelMappings.find(upperLevelStr); it != customLevelMappings.end()) {
        return it->second;
    }
    
    // Fallback to static default mapping
    return stringToLogLevel(levelStr);
}

void LogAnalyzer::exportAsCsv(std::ostream& out, const FilterCriteria& filter) const {
    std::vector<LogEntry> entriesToExport = getFilteredEntries(filter);
    
    // Write Header
    out << "Timestamp,Level,Message\n";
    
    for (const auto& entry : entriesToExport) {
        out << formatTimestamp(entry.timestamp) << ",";
        out << logLevelToString(entry.level) << ",";
        
        // Escape message for CSV
        std::string msg = entry.message;
        bool needsQuotes = false;
        if (msg.find(',') != std::string::npos || msg.find('"') != std::string::npos || msg.find('\n') != std::string::npos) {
            needsQuotes = true;
        }
        
        if (needsQuotes) {
            out << "\"";
            for (char c : msg) {
                if (c == '"') {
                    out << "\"\""; // Double quotes
                } else {
                    out << c;
                }
            }
            out << "\"";
        } else {
            out << msg;
        }
        out << "\n";
    }
}


// Helper for timestamp formatting (Iteration 1 Feature)
std::string LogAnalyzer::formatTimestamp(std::chrono::system_clock::time_point tp, std::string_view format) const {
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::localtime(&time);
    std::stringstream ss;
    // Use the format string provided. std::put_time expects a C-style format string.
    ss << std::put_time(&tm, std::string(format).c_str());
    return ss.str();
}

std::vector<LogEntry> LogAnalyzer::getFilteredEntries(const FilterCriteria& criteria) const {
    std::vector<LogEntry> filteredEntries;
    std::regex regexFilter;
    if (!criteria.regexPattern.empty()) {
        try {
            regexFilter = std::regex(criteria.regexPattern);
        } catch (const std::regex_error& e) {
            std::cerr << "Invalid regex pattern in filter: " << e.what() << std::endl;
            return filteredEntries;
        }
    }

    // Define the level order for minLogLevel comparison
    auto getLogLevelValue = [](LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG:   return 0;
            case LogLevel::INFO:    return 1;
            case LogLevel::WARNING: return 2;
            case LogLevel::ERROR:   return 3;
            case LogLevel::UNKNOWN: return 4;
            default:                return 5; // Should not happen
        }
    };

    for (const auto& entry : entries) {
        // Level filter
        bool levelMatch = criteria.levels.empty();
        if (!criteria.levels.empty()) {
            for (LogLevel level : criteria.levels) {
                if (entry.level == level) {
                    levelMatch = true;
                    break;
                }
            }
        }
        
        // minLogLevel filter (Iteration 1 Feature)
        if (criteria.minLogLevel.has_value()) {
            if (getLogLevelValue(entry.level) < getLogLevelValue(criteria.minLogLevel.value())) {
                levelMatch = false; // Overrides if minLogLevel not met
            }
        }

        if (!levelMatch) continue;

        // Time filter
        if (criteria.startTime.has_value() && entry.timestamp < criteria.startTime.value()) {
            continue;
        }
        if (criteria.endTime.has_value() && entry.timestamp > criteria.endTime.value()) {
            continue;
        }

        // Message filter (Regex or Keyword)
        bool messageMatch = true;
        if (!criteria.regexPattern.empty()) {
             messageMatch = std::regex_search(entry.message, regexFilter);
        } else if (!criteria.keyword.empty()) {
            std::string entryMessage = entry.message;
            std::string keyword = criteria.keyword;
            
            if (!criteria.keywordCaseSensitive) {
                std::transform(entryMessage.begin(), entryMessage.end(), entryMessage.begin(), ::tolower);
                std::transform(keyword.begin(), keyword.end(), keyword.begin(), ::tolower);
            }
            if (entryMessage.find(keyword) == std::string::npos) {
                messageMatch = false;
            }
        }

        if (messageMatch) {
            filteredEntries.push_back(entry);
        }
    }
    return filteredEntries;
}

std::vector<LogEntry> LogAnalyzer::getSortedFilteredEntries(
    const FilterCriteria& criteria,
    SortBy sortBy,
    SortOrder sortOrder
) const {
    std::vector<LogEntry> filtered = getFilteredEntries(criteria);
    
    auto levelToInt = [](LogLevel l) {
        switch(l) {
            case LogLevel::DEBUG: return 1;
            case LogLevel::INFO: return 2;
            case LogLevel::WARNING: return 3;
            case LogLevel::ERROR: return 4;
            case LogLevel::UNKNOWN: return 5;
            default: return 0;
        }
    };

    std::sort(filtered.begin(), filtered.end(), [=](const LogEntry& a, const LogEntry& b) {
        if (sortOrder == SortOrder::ASCENDING) {
             switch (sortBy) {
                case SortBy::TIMESTAMP: return a.timestamp < b.timestamp;
                case SortBy::LEVEL: return levelToInt(a.level) < levelToInt(b.level);
                case SortBy::MESSAGE: return a.message < b.message;
            }
        } else {
            switch (sortBy) {
                case SortBy::TIMESTAMP: return a.timestamp > b.timestamp;
                case SortBy::LEVEL: return levelToInt(a.level) > levelToInt(b.level);
                case SortBy::MESSAGE: return a.message > b.message;
            }
        }
        return false;
    });
    
    return filtered;
}

std::map<std::string, int> LogAnalyzer::getUniqueMessageCounts() const {
    std::map<std::string, int> counts;
    for (const auto& entry : entries) {
        counts[entry.message]++;
    }
    return counts;
}

std::vector<std::pair<std::string, int>> LogAnalyzer::getTopMessages(int n) const {
    std::map<std::string, int> counts = getUniqueMessageCounts();
    std::vector<std::pair<std::string, int>> sortedCounts(counts.begin(), counts.end());
    
    std::sort(sortedCounts.begin(), sortedCounts.end(), [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
        return a.second > b.second; // Descending order
    });
    
    if (n >= 0 && n < static_cast<int>(sortedCounts.size())) {
        sortedCounts.resize(n);
    }
    return sortedCounts;
}

void LogAnalyzer::printFilteredEntries(
    std::ostream& out,
    const FilterCriteria& criteria,
    std::string_view formatString
) const {
    std::vector<LogEntry> filtered = getFilteredEntries(criteria);
    for (const auto& entry : filtered) {
        // We'll use std::format with positional arguments.
        // The formatString will need to be adapted to use positional arguments.
        // For simplicity and given the named placeholder design, we'll continue with string replacement for now,
        // but adapt to std::string_view for the format string itself.
        std::string output = std::string(formatString); // Convert to std::string for modification
        
        // Simple placeholder replacement
        auto replaceAll = [&](std::string& str, const std::string& from, const std::string& to) {
            size_t start_pos = 0;
            while((start_pos = str.find(from, start_pos)) != std::string::npos) {
                str.replace(start_pos, from.length(), to);
                start_pos += to.length();
            }
        };

        replaceAll(output, "{timestamp}", formatTimestamp(entry.timestamp));
        replaceAll(output, "{level}", logLevelToString(entry.level));
        replaceAll(output, "{message}", entry.message);
        
        out << output << std::endl;
    }
}



// New API Extensions for Iteration 1 - Search functionality
std::optional<LogEntry> LogAnalyzer::findFirst(const FilterCriteria& criteria) const {
    const std::vector<LogEntry>& filtered = getFilteredEntries(criteria);
    if (!filtered.empty()) {
        return filtered.front();
    }
    return std::nullopt;
}

std::optional<LogEntry> LogAnalyzer::findLast(const FilterCriteria& criteria) const {
    const std::vector<LogEntry>& filtered = getFilteredEntries(criteria);
    if (!filtered.empty()) {
        return filtered.back();
    }
    return std::nullopt;
}

std::string LogAnalyzer::getSummaryString() const {
    std::stringstream ss;
    ss << "--- Log Analysis Summary ---" << std::endl;
    ss << "Total entries: " << entries.size() << std::endl;
    for (auto const& [level, count] : levelCounts) {
        ss << logLevelToString(level) << ": " << count << std::endl;
    }
    return ss.str();
}

// New private parser to be used by load, append, and analyze
std::expected<void, LogParseError> LogAnalyzer::parseFile(const std::string& filePath, const std::string& pattern, bool appendMode) {
    AnalysisReport report;
    report.linesProcessed = 0;
    report.successfulParses = 0;
    report.status = ParseError::SUCCESS;

    std::ifstream file(filePath);
    if (!file.is_open()) {
        LogParseError err{ParseError::FILE_OPEN_FAILED, "Could not open file: " + filePath, 0};
        lastReport.status = err.code;
        lastReport.parseErrors.push_back({err.lineNumber, err.message});
        return std::unexpected(err);
    }

    if (!appendMode) {
        clear();
    }

    std::string line;
    std::string finalPattern = pattern.empty() ? R"(\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}(?:\.\d{3})?)\]\s+([A-Z]+):\s+(.*))" : pattern;
    
    std::regex logRegex;
    try {
        logRegex = std::regex(finalPattern);
    } catch (const std::regex_error& e) {
        LogParseError err{ParseError::INVALID_REGEX_PATTERN, "Invalid regex pattern provided: " + std::string(e.what()), 0};
        lastReport.status = err.code;
        lastReport.parseErrors.push_back({err.lineNumber, err.message});
        return std::unexpected(err);
    }
    
    std::smatch match;
    size_t currentLineNumber = 0;

    while (std::getline(file, line)) {
        currentLineNumber++;
        report.linesProcessed++;

        if (std::regex_search(line, match, logRegex) && match.size() == 4) {
            LogEntry entry;
            bool timestampParseFailed = false;
            
            std::string timestampStr = match[1].str();
            std::tm tm = {};
            std::istringstream ss(timestampStr);
            long long milliseconds = 0;

            if (timestampStr.length() > 19 && timestampStr[19] == '.') { // Contains milliseconds
                ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                if (!ss.fail()) {
                    ss.ignore(1); // Skip the dot
                    std::string ms_str;
                    ss >> ms_str;
                    if (!ms_str.empty()) {
                        try {
                            milliseconds = std::stoll(ms_str);
                        } catch (const std::out_of_range& oor) {
                            milliseconds = 0;
                            report.parseErrors.push_back({currentLineNumber, "Malformed milliseconds in timestamp for line: " + line});
                        } catch (const std::invalid_argument& ia) {
                            milliseconds = 0;
                            report.parseErrors.push_back({currentLineNumber, "Non-numeric milliseconds in timestamp for line: " + line});
                        }
                    }
                    entry.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm)) + std::chrono::milliseconds(milliseconds);
                } else {
                    timestampParseFailed = true;
                }
            } else { // No milliseconds
                ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                if (!ss.fail()) {
                    entry.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
                } else {
                    timestampParseFailed = true;
                }
            }

            if (timestampParseFailed) {
                report.parseErrors.push_back({currentLineNumber, "Timestamp parsing failed for line: " + line});
                entry.timestamp = std::chrono::system_clock::now();
                entry.level = LogLevel::UNKNOWN;
                entry.message = line;
                entries.push_back(entry);
                levelCounts[entry.level]++;
            } else {
                entry.level = resolveLogLevel(match[2].str());
                entry.message = match[3].str();
                entries.push_back(entry);
                levelCounts[entry.level]++;
                report.successfulParses++;
            }
        } else {
            report.parseErrors.push_back({currentLineNumber, "Line does not match log pattern: " + line});
            LogEntry entry;
            entry.timestamp = std::chrono::system_clock::now(); 
            entry.level = LogLevel::UNKNOWN;
            entry.message = line;
            entries.push_back(entry);
            levelCounts[LogLevel::UNKNOWN]++;
        }
    }
    file.close();

    if (report.successfulParses < report.linesProcessed) {
        report.status = ParseError::PARTIAL_FAILURE;
    }

    lastReport = report;

    if (appendMode) {
        // Re-sort the combined entries by timestamp
        std::ranges::sort(entries, [](const LogEntry& a, const LogEntry& b) {
            return a.timestamp < b.timestamp;
        });
    }

    return {}; // Success
}

std::expected<void, LogParseError> LogAnalyzer::load(const std::string& filePath, const std::string& pattern) {
    return parseFile(filePath, pattern, false);
}

std::expected<void, LogParseError> LogAnalyzer::append(const std::string& filePath, const std::string& pattern) {
    return parseFile(filePath, pattern, true);
}

std::future<AnalysisReport> LogAnalyzer::load_async(const std::string& filePath, const std::string& pattern) {
    // This captures 'this' to call the member function. The user of the API
    // is responsible for ensuring thread safety, i.e., not calling other methods
    // on this LogAnalyzer instance until the future is ready.
    return std::async(std::launch::async, [this, filePath, pattern] {
        return this->analyze(filePath, pattern);
    });
}

// Modified analyze to use the new internal parser
AnalysisReport LogAnalyzer::analyze(const std::string& filePath, const std::string& pattern) {
    auto result = parseFile(filePath, pattern, false);
    if (!result.has_value()) {
        // analyze needs to return a full report even on total failure.
        // lastReport is already set inside parseFile for failure cases.
    }
    return lastReport;
}

// Function signature for streaming (Iteration 1 Feature)
void LogAnalyzer::analyzeStream(
    const std::string& filePath, 
    std::function<bool(const LogEntry&)> entryCallback,
    const std::string& pattern
) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Could not open file for streaming: " << filePath << std::endl;
        return;
    }

    std::string line;
    std::string finalPattern = pattern.empty() ? R"(\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}(?:\.\d{3})?)\]\s+([A-Z]+):\s+(.*))" : pattern;
    
    std::regex logRegex;
    try {
        logRegex = std::regex(finalPattern);
    } catch (const std::regex_error& e) {
        std::cerr << "Invalid regex pattern provided for streaming: " << e.what() << std::endl;
        return;
    }
    
    std::smatch match;

    while (std::getline(file, line)) {
        LogEntry entry;
        bool parsedSuccessfully = false;

        if (std::regex_search(line, match, logRegex) && match.size() == 4) {
            // Parse timestamp
            std::string timestampStr = match[1].str();
            std::tm tm = {};
            std::istringstream ss(timestampStr);
            
            // Handle optional milliseconds
            if (timestampStr.length() > 19 && timestampStr[19] == '.') {
                ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                if (!ss.fail()) {
                    long long milliseconds = 0;
                    ss.ignore(1); // Skip the dot
                    std::string ms_str;
                    ss >> ms_str;
                    if (!ms_str.empty()) {
                        try {
                            milliseconds = std::stoll(ms_str);
                        } catch (...) { } // Ignore malformed milliseconds
                    }
                    entry.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm)) + std::chrono::milliseconds(milliseconds);
                    parsedSuccessfully = true;
                }
            } else {
                ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                if (!ss.fail()) {
                    entry.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
                    parsedSuccessfully = true;
                }
            }

            if (parsedSuccessfully) {
                entry.level = resolveLogLevel(match[2].str());
                entry.message = match[3].str();
            }
        }
        
        if (!parsedSuccessfully) {
            // Fallback for unparseable lines in streaming mode
            entry.timestamp = std::chrono::system_clock::now(); 
            entry.level = LogLevel::UNKNOWN;
            entry.message = line;
        }

        if (!entryCallback(entry)) {
            break; // Stop processing if callback returns false
        }
    }
    file.close();
}

void LogAnalyzer::printSummary(std::ostream& out) const {
    out << getSummaryString();
}

void LogAnalyzer::exportAsJson(std::ostream& out, const FilterCriteria& filter, bool includeSummary, bool prettyPrint) const {
    std::vector<LogEntry> entriesToExport = getFilteredEntries(filter);
    
    std::string indent = prettyPrint ? "  " : "";
    std::string newline = prettyPrint ? "\n" : "";
    
    if (includeSummary) {
        out << "{" << newline;
        if (prettyPrint) out << "  ";
        out << "\"summary\": {" << newline;
        
        std::map<std::string, int> filteredCounts;
        for (const auto& entry : entriesToExport) {
            filteredCounts[logLevelToString(entry.level)]++;
        }
        
        size_t count = 0;
        for (const auto& [levelStr, num] : filteredCounts) {
            if (prettyPrint) out << "    ";
            out << "\"" << levelStr << "\": " << num;
            if (++count < filteredCounts.size()) out << "," << newline;
            else out << newline;
        }
        if (prettyPrint) out << "  ";
        out << "}," << newline;
        
        if (prettyPrint) out << "  ";
        out << "\"entries\": ";
    }
    
    out << "[" << newline;
    for (size_t i = 0; i < entriesToExport.size(); ++i) {
        const auto& entry = entriesToExport[i];
        if (prettyPrint && includeSummary) out << "    ";
        else if (prettyPrint) out << indent;
        
        out << "  {" << newline;
        
        std::string entryIndent = (prettyPrint && includeSummary) ? "      " : (prettyPrint ? "    " : "");
        
        out << entryIndent << "\"timestamp\": \"" << formatTimestamp(entry.timestamp) << "\"," << newline;
        out << entryIndent << "\"level\": \"" << logLevelToString(entry.level) << "\"," << newline;
        
        // Escape quotes in message for valid JSON
        std::string escapedMessage = entry.message;
        std::string buffer;
        buffer.reserve(escapedMessage.length());
        for (char c : escapedMessage) {
            switch (c) {
                case '"': buffer += "\\\""; break;
                case '\\': buffer += "\\\\"; break;
                case '/': buffer += "\\/"; break;
                case '\b': buffer += "\\b"; break;
                case '\f': buffer += "\\f"; break;
                case '\n': buffer += "\\n"; break;
                case '\r': buffer += "\\r"; break;
                case '\t': buffer += "\\t"; break;
                default: buffer += c; break;
            }
        }
        
        out << entryIndent << "\"message\": \"" << buffer << "\"" << newline;
        
        if (prettyPrint && includeSummary) out << "    ";
        else if (prettyPrint) out << indent;
        out << "  }";
        
        if (i < entriesToExport.size() - 1) {
            out << "," << newline;
        } else {
            out << newline;
        }
    }
    
    if (prettyPrint && includeSummary) out << "  ";
    out << "]";
    
    if (includeSummary) {
        out << newline << "}" << newline;
    } else {
        out << newline;
    }
}

// New API Extensions for Iteration 1 - Advanced Statistical Analysis

std::vector<TimeGap> LogAnalyzer::findTimeGaps(std::chrono::milliseconds minGapDuration) const {
    std::vector<TimeGap> gaps;
    if (entries.size() < 2) return gaps;

    // Entries are assumed to be sorted (by merge or load)
    for (size_t i = 0; i < entries.size() - 1; ++i) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(entries[i+1].timestamp - entries[i].timestamp);
        if (duration > minGapDuration) {
            gaps.push_back({entries[i].timestamp, entries[i+1].timestamp, duration});
        }
    }
    return gaps;
}

double LogAnalyzer::getAverageEntryRate() const {
    if (entries.size() < 2) return 0.0;

    auto minmax = std::minmax_element(entries.begin(), entries.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });

    auto duration = std::chrono::duration_cast<std::chrono::seconds>(minmax.second->timestamp - minmax.first->timestamp);
    if (duration.count() == 0) return static_cast<double>(entries.size());

    return static_cast<double>(entries.size()) / duration.count();
}

std::vector<TimeWindowStats> LogAnalyzer::getFrequencyDistribution(
    std::chrono::seconds windowSize
) const {
    std::vector<TimeWindowStats> distribution;
    if (entries.empty() || windowSize <= std::chrono::seconds(0)) {
        return distribution;
    }

    // Find the overall time range of the log entries
    auto minmax_ts = std::minmax_element(entries.begin(), entries.end(), 
        [](const LogEntry& a, const LogEntry& b) {
            return a.timestamp < b.timestamp;
        });
    
    auto minTime = minmax_ts.first->timestamp;
    auto maxTime = minmax_ts.second->timestamp; // Restored maxTime

    // Iterate through time windows
    auto currentWindowStart = minTime;
    while (currentWindowStart <= maxTime) {
        auto windowEnd = currentWindowStart + windowSize;
        
        TimeWindowStats stats;
        stats.windowStart = currentWindowStart;
        stats.totalCount = 0;
        stats.counts[LogLevel::INFO] = 0;
        stats.counts[LogLevel::WARNING] = 0;
        stats.counts[LogLevel::ERROR] = 0;
        stats.counts[LogLevel::DEBUG] = 0;
        stats.counts[LogLevel::UNKNOWN] = 0;

        // Collect statistics for the current window
        for (const auto& entry : entries) {
            if (entry.timestamp >= currentWindowStart && entry.timestamp < windowEnd) {
                stats.counts[entry.level]++;
                stats.totalCount++;
            }
        }
        
        // Only add the window if it contains any log entries, or if it's the very first window to ensure coverage.
        // This logic might need refinement based on desired behavior for empty windows.
        // For now, we add all windows that start within the log range.
        if (stats.totalCount > 0 || currentWindowStart == minTime) {
            distribution.push_back(stats);
        }

        // Move to the next window
        currentWindowStart = windowEnd;
    }

    return distribution;
}

// Optimized O(N) implementation of distribution (Iteration 1 Feature)
std::vector<TimeWindowStats> LogAnalyzer::getFrequencyDistributionOptimized(
    std::chrono::seconds windowSize
) const {
    std::vector<TimeWindowStats> distribution;
    if (entries.empty() || windowSize <= std::chrono::seconds(0)) {
        return distribution;
    }

    // Initialize the first window's statistics, starting from the first log entry's timestamp
    TimeWindowStats currentStats;
    currentStats.windowStart = entries.front().timestamp; // Assumes entries are sorted by timestamp
    currentStats.totalCount = 0;
    currentStats.counts[LogLevel::INFO] = 0; 
    currentStats.counts[LogLevel::WARNING] = 0;
    currentStats.counts[LogLevel::ERROR] = 0;
    currentStats.counts[LogLevel::DEBUG] = 0;
    currentStats.counts[LogLevel::UNKNOWN] = 0;

    for (const auto& entry : entries) {
        // While the current entry's timestamp is past the current window's end boundary
        while (entry.timestamp >= (currentStats.windowStart + windowSize)) {
            // Add the completed window's statistics to the distribution
            distribution.push_back(currentStats);

            // Prepare for the next window
            currentStats.windowStart += windowSize;
            currentStats.totalCount = 0;
            currentStats.counts.clear(); // Clear map to reset all levels to 0
            currentStats.counts[LogLevel::INFO] = 0; 
            currentStats.counts[LogLevel::WARNING] = 0;
            currentStats.counts[LogLevel::ERROR] = 0;
            currentStats.counts[LogLevel::DEBUG] = 0;
            currentStats.counts[LogLevel::UNKNOWN] = 0;
        }
        
        // Add the entry to the current window's statistics
        currentStats.counts[entry.level]++;
        currentStats.totalCount++;
    }

    // Add the last accumulated window's statistics if it contains entries
    // or if it's the only window and there were entries processed.
    if (currentStats.totalCount > 0 || distribution.empty()) {
        distribution.push_back(currentStats);
    }
    
    return distribution;
}

// New API Extensions for Iteration 1 - Multi-file Merge

// New API Extensions for Iteration 1 - Multi-file Merge
void LogAnalyzer::merge(const LogAnalyzer& other) {
    // Append entries from the other analyzer
    entries.insert(entries.end(), other.entries.begin(), other.entries.end());
    
    // Update level counts
    for (const auto& pair : other.levelCounts) {
        levelCounts[pair.first] += pair.second;
    }

    // Re-sort the combined entries by timestamp
    std::ranges::sort(entries, [](const LogEntry& a, const LogEntry& b) {
        return a.timestamp < b.timestamp;
    });
}
