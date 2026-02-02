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
}

const std::vector<LogEntry>& LogAnalyzer::getEntries() const {
    return entries;
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

// Modified analyze (Breaking change from void)
std::expected<size_t, ParseError> LogAnalyzer::analyze(const std::string& filePath, const std::string& pattern) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filePath << std::endl;
        return std::unexpected(ParseError::FILE_OPEN_FAILED);
    }

    clear();

    std::string line;
    // Default regex for log lines like: [2023-10-27 10:00:00] INFO: My message
    // Also consider optional milliseconds: [2023-10-27 10:00:00.123] INFO: My message
    std::string finalPattern = pattern.empty() ? R"(\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}(?:\.\d{3})?)\]\s+([A-Z]+):\s+(.*))" : pattern;
    
    std::regex logRegex;
    try {
        logRegex = std::regex(finalPattern);
    } catch (const std::regex_error& e) {
        std::cerr << "Invalid regex pattern provided: " << e.what() << std::endl;
        return std::unexpected(ParseError::INVALID_REGEX_PATTERN);
    }
    
    std::smatch match;
    size_t successfulParses = 0;
    bool partialFailure = false;

    while (std::getline(file, line)) {
        if (std::regex_search(line, match, logRegex) && match.size() == 4) {
            LogEntry entry;
            
            // Parse timestamp
            std::string timestampStr = match[1].str();
            std::tm tm = {};
            std::istringstream ss(timestampStr);
            
            // Handle optional milliseconds
            if (timestampStr.length() > 19 && timestampStr[19] == '.') { // Check for milliseconds
                // Parse up to seconds, then handle milliseconds
                ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                if (!ss.fail()) {
                    long long milliseconds = 0;
                    ss.ignore(1); // Skip the dot
                    std::string ms_str;
                    ss >> ms_str;
                    if (!ms_str.empty()) {
                        try {
                            milliseconds = std::stoll(ms_str);
                        } catch (...) {
                            // Ignore malformed milliseconds for now, treat as no milliseconds
                        }
                    }
                    entry.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm)) + std::chrono::milliseconds(milliseconds);
                } else {
                    entry.timestamp = std::chrono::system_clock::now(); // Fallback
                    partialFailure = true;
                }
            } else {
                ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                if (!ss.fail()) {
                    entry.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
                } else {
                    entry.timestamp = std::chrono::system_clock::now(); // Fallback
                    partialFailure = true;
                }
            }

            entry.level = stringToLogLevel(match[2].str());
            entry.message = match[3].str();
            
            entries.push_back(entry);
            levelCounts[entry.level]++;
            successfulParses++;
        } else {
            // If a line doesn't match the pattern, it's an UNKNOWN entry, and counts as a partial failure
            LogEntry entry;
            entry.timestamp = std::chrono::system_clock::now(); 
            entry.level = LogLevel::UNKNOWN;
            entry.message = line;
            entries.push_back(entry);
            levelCounts[LogLevel::UNKNOWN]++;
            partialFailure = true;
        }
    }
    file.close();

    if (successfulParses > 0 && partialFailure) {
        // If some lines parsed and some failed, we return the successful count,
        // but the ParseError::PARTIAL_FAILURE context is lost in the return type.
        // The design specified "returns the number of successfully parsed entries on success, or a ParseError code on failure".
        // This implies if there's an error code, it's a failure.
        // To accurately reflect partial failure when some lines were parsed,
        // we should probably return a success with the count, and the user can check entries for UNKNOWN.
        // For now, sticking to the error design: if there's any partial failure, it's an error.
        // This makes `analyze` return either a count (pure success) or an error (any failure).
        return std::unexpected(ParseError::PARTIAL_FAILURE);
    } else if (successfulParses == 0 && partialFailure) {
        // All lines failed to parse or file was empty, but no other error like regex or file open
        return std::unexpected(ParseError::PARTIAL_FAILURE); // Consider this a total failure if nothing parsed
    }
    return successfulParses;
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
                entry.level = stringToLogLevel(match[2].str());
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
        
        int count = 0;
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
    auto maxTime = minmax_ts.second->timestamp;

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
