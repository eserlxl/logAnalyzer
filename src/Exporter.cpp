#include "Exporter.h"
#include "LogAnalyzer.h"
#include "Utils.h"
#include <nlohmann/json.hpp>
#include <iomanip>

using json = nlohmann::json;

void Exporter::exportAsJson(
    std::ostream& os, 
    const std::vector<LogEntry>& entries, 
    bool includeSummary, 
    bool prettyPrint) {
    
    json j;
    j["entries"] = json::array();
    
    for (const auto& entry : entries) {
        json entryJson = {
            {"id", entry.id},
            {"timestamp", Utils::formatTimestamp(entry.timestamp)},
            {"level", Utils::logLevelToString(entry.level)},
            {"message", entry.message}
        };
        
        if (!entry.structuredFields.empty()) {
            entryJson["fields"] = entry.structuredFields;
        }
        
        j["entries"].push_back(entryJson);
    }
    
    if (includeSummary) {
        j["summary"] = {
            {"count", entries.size()}
        };
    }
    
    if (prettyPrint) {
        os << j.dump(4) << std::endl;
    } else {
        os << j.dump() << std::endl;
    }
}

void Exporter::exportAsCsv(
    std::ostream& os, 
    const std::vector<LogEntry>& entries, 
    char separator) {
    
    os << "ID" << separator << "Timestamp" << separator << "Level" << separator << "Message" << std::endl;
    
    for (const auto& entry : entries) {
        os << entry.id << separator
           << "\"" << Utils::formatTimestamp(entry.timestamp) << "\"" << separator
           << "\"" << Utils::logLevelToString(entry.level) << "\"" << separator
           << "\"" << entry.message << "\"" << std::endl;
    }
}

void Exporter::exportAsText(
    std::ostream& os, 
    const std::vector<LogEntry>& entries, 
    const std::string& formatString,
    bool useColors) {
    
    for (const auto& entry : entries) {
        os << formatEntryForText(entry, formatString, useColors) << std::endl;
    }
}

std::string Exporter::formatEntryForText(
    const LogEntry& entry, 
    const std::string& formatString, 
    bool useColors) {
    
    std::string result = formatString;
    std::string levelStr = Utils::logLevelToString(entry.level);
    
    if (useColors) {
        if (entry.level == LogLevel::ERROR || entry.level == LogLevel::FATAL) {
            levelStr = Utils::AnsiColor::RED + levelStr + Utils::AnsiColor::RESET;
        } else if (entry.level == LogLevel::WARNING) {
            levelStr = Utils::AnsiColor::YELLOW + levelStr + Utils::AnsiColor::RESET;
        } else if (entry.level == LogLevel::INFO) {
            levelStr = Utils::AnsiColor::GREEN + levelStr + Utils::AnsiColor::RESET;
        } else if (entry.level == LogLevel::DEBUG || entry.level == LogLevel::TRACE) {
            levelStr = Utils::AnsiColor::CYAN + levelStr + Utils::AnsiColor::RESET;
        }
    }
    
    Utils::replaceAll(result, "{id}", std::to_string(entry.id));
    Utils::replaceAll(result, "{timestamp}", Utils::formatTimestamp(entry.timestamp));
    Utils::replaceAll(result, "{level}", levelStr);
    Utils::replaceAll(result, "{message}", entry.message);
    
    return result;
}
