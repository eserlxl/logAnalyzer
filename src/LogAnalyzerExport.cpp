#include "LogAnalyzer.h"
#include "Utils.h"
#include <iostream>
#include <vector>
#include <string>

std::string LogAnalyzer::formatTimestamp(std::chrono::system_clock::time_point tp, std::string_view format) const {
    return Utils::formatTimestamp(tp, format);
}

void LogAnalyzer::exportAsCsv(std::ostream& out, const FilterCriteria& filter, char delimiter) const {
    // std::lock_guard<std::mutex> lock(mutex_); // Temporarily removed for diagnostic purposes
    
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
