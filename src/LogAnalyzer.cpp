#include "LogAnalyzer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm> // For std::transform, std::sort
#include <iomanip>   // For std::get_time, std::put_time
#include <ctime>     // For std::tm

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

std::string LogAnalyzer::formatTimestamp(std::chrono::system_clock::time_point tp) const {
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::localtime(&time);
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
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
    const std::string& formatString
) const {
    std::vector<LogEntry> filtered = getFilteredEntries(criteria);
    for (const auto& entry : filtered) {
        std::string output = formatString;
        
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

    clear();

    std::string line;
    // Default regex for log lines like: [2023-10-27 10:00:00] INFO: My message
    std::string finalPattern = pattern.empty() ? R"(\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\]\s+([A-Z]+):\s+(.*))" : pattern;
    
    try {
        std::regex logRegex(finalPattern);
        std::smatch match;

        while (std::getline(file, line)) {
            if (std::regex_search(line, match, logRegex) && match.size() == 4) {
                LogEntry entry;
                
                // Parse timestamp
                std::string timestampStr = match[1].str();
                std::tm tm = {};
                std::istringstream ss(timestampStr);
                ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                if (ss.fail()) {
                    // Fallback: use current time if parsing fails
                    entry.timestamp = std::chrono::system_clock::now();
                } else {
                    entry.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
                }

                entry.level = stringToLogLevel(match[2].str());
                entry.message = match[3].str();
                
                entries.push_back(entry);
                levelCounts[entry.level]++;
            } else {
                // If a line doesn't match the pattern, it's an UNKNOWN entry
                LogEntry entry;
                entry.timestamp = std::chrono::system_clock::now(); 
                entry.level = LogLevel::UNKNOWN;
                entry.message = line;
                entries.push_back(entry);
                levelCounts[LogLevel::UNKNOWN]++;
            }
        }
    } catch (const std::regex_error& e) {
        std::cerr << "Invalid regex pattern provided: " << e.what() << std::endl;
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
