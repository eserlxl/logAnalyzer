#include "LogAnalyzer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm> // For std::transform

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
    levelCounts[LogLevel::INFO] = 0;
    levelCounts[LogLevel::WARNING] = 0;
    levelCounts[LogLevel::ERROR] = 0;
    levelCounts[LogLevel::DEBUG] = 0;
    levelCounts[LogLevel::UNKNOWN] = 0;
}

const std::vector<LogEntry>& LogAnalyzer::getEntries() const {
    return entries;
}

std::vector<LogEntry> LogAnalyzer::getFilteredEntries(const FilterCriteria& criteria) const {
    std::vector<LogEntry> filteredEntries;
    for (const auto& entry : entries) {
        bool levelMatch = criteria.levels.empty();
        if (!criteria.levels.empty()) {
            for (LogLevel level : criteria.levels) {
                if (entry.level == level) {
                    levelMatch = true;
                    break;
                }
            }
        }

        bool keywordMatch = criteria.keyword.empty();
        if (!criteria.keyword.empty()) {
            std::string entryMessageLower = entry.message;
            std::string keywordLower = criteria.keyword;
            std::transform(entryMessageLower.begin(), entryMessageLower.end(), entryMessageLower.begin(), ::tolower);
            std::transform(keywordLower.begin(), keywordLower.end(), keywordLower.begin(), ::tolower);
            if (entryMessageLower.find(keywordLower) != std::string::npos) {
                keywordMatch = true;
            }
        }

        if (levelMatch && keywordMatch) {
            filteredEntries.push_back(entry);
        }
    }
    return filteredEntries;
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

void LogAnalyzer::analyze(const std::string& filePath, const std::string& pattern) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filePath << std::endl;
        return;
    }

    // Clear previous analysis results
    entries.clear();
    for (auto& pair : levelCounts) {
        pair.second = 0;
    }

    std::string line;
    // Default regex for log lines like: [2023-10-27 10:00:00] INFO: My message
    // Updated default regex to be more robust
    std::string finalPattern = pattern.empty() ? R"(\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\]\s+([A-Z]+):\s+(.*))" : pattern;
    
    try {
        std::regex logRegex(finalPattern);
        std::smatch match;

        while (std::getline(file, line)) {
            if (std::regex_search(line, match, logRegex) && match.size() == 4) {
                LogEntry entry;
                entry.timestamp = match[1].str();
                entry.level = stringToLogLevel(match[2].str());
                entry.message = match[3].str();
                
                entries.push_back(entry);
                levelCounts[entry.level]++;
            } else {
                // If a line doesn't match the pattern, it's an UNKNOWN entry
                LogEntry entry;
                entry.timestamp = "N/A"; // Or current time, or empty
                entry.level = LogLevel::UNKNOWN;
                entry.message = line; // Store the entire line as the message
                entries.push_back(entry);
                levelCounts[LogLevel::UNKNOWN]++;
            }
        }
    } catch (const std::regex_error& e) {
        std::cerr << "Invalid regex pattern provided: " << e.what() << std::endl;
        // Optionally, rethrow or set an error state
    }
    file.close();
}

void LogAnalyzer::printSummary(std::ostream& out) const {
    out << getSummaryString();
}

void LogAnalyzer::exportAsJson(std::ostream& out, const FilterCriteria& filter) const {
    std::vector<LogEntry> entriesToExport = getFilteredEntries(filter);
    
    out << "[" << std::endl;
    for (size_t i = 0; i < entriesToExport.size(); ++i) {
        const auto& entry = entriesToExport[i];
        out << "  {" << std::endl;
        out << "    \"timestamp\": \"" << entry.timestamp << "\"," << std::endl;
        out << "    \"level\": \"" << logLevelToString(entry.level) << "\"," << std::endl;
        // Escape quotes in message for valid JSON
        std::string escapedMessage = entry.message;
        size_t pos = escapedMessage.find('"');
        while (pos != std::string::npos) {
            escapedMessage.replace(pos, 1, "\\\"");
            pos = escapedMessage.find('"', pos + 2);
        }
        out << "    \"message\": \"" << escapedMessage << "\"" << std::endl;
        out << "  }";
        if (i < entriesToExport.size() - 1) {
            out << ",";
        }
        out << std::endl;
    }
    out << "]" << std::endl;
}
