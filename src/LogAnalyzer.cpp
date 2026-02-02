#include "LogAnalyzer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>

LogAnalyzer::LogAnalyzer() {
    levelCounts[LogLevel::INFO] = 0;
    levelCounts[LogLevel::WARNING] = 0;
    levelCounts[LogLevel::ERROR] = 0;
    levelCounts[LogLevel::DEBUG] = 0;
    levelCounts[LogLevel::UNKNOWN] = 0;
}

LogLevel LogAnalyzer::stringToLogLevel(const std::string& levelStr) {
    if (levelStr == "INFO") return LogLevel::INFO;
    if (levelStr == "WARNING") return LogLevel::WARNING;
    if (levelStr == "ERROR") return LogLevel::ERROR;
    if (levelStr == "DEBUG") return LogLevel::DEBUG;
    return LogLevel::UNKNOWN;
}

std::string LogAnalyzer::logLevelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::DEBUG: return "DEBUG";
        default: return "UNKNOWN";
    }
}

void LogAnalyzer::analyze(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filePath << std::endl;
        return;
    }

    std::string line;
    // Simple regex for log lines like: [2023-10-27 10:00:00] INFO: My message
    std::regex logRegex(R"(\[([^\]]+)\]\s+(\w+):\s+(.*))");
    std::smatch match;

    while (std::getline(file, line)) {
        if (std::regex_search(line, match, logRegex) && match.size() == 4) {
            LogEntry entry;
            entry.timestamp = match[1].str();
            entry.level = stringToLogLevel(match[2].str());
            entry.message = match[3].str();
            
            entries.push_back(entry);
            levelCounts[entry.level]++;
        }
    }
    file.close();
}

void LogAnalyzer::printSummary() const {
    std::cout << "--- Log Analysis Summary ---" << std::endl;
    std::cout << "Total entries: " << entries.size() << std::endl;
    for (auto const& [level, count] : levelCounts) {
        std::cout << logLevelToString(level) << ": " << count << std::endl;
    }
}

void LogAnalyzer::filterByLevel(LogLevel level) const {
    std::cout << "--- Filtering by level: " << logLevelToString(level) << " ---" << std::endl;
    for (const auto& entry : entries) {
        if (entry.level == level) {
            std::cout << "[" << entry.timestamp << "] " << entry.message << std::endl;
        }
    }
}
