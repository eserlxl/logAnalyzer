#include "analyzer/Core.h"
#include "utils/Core.h"
#include <iostream>
#include <vector>
#include <string>

void LogAnalyzer::exportAsCsv(std::ostream& out, const FilterCriteria& filter, char delimiter, std::string_view timestampFormat) const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    
    // CSV Header
    out << "Timestamp" << delimiter << "Level" << delimiter << "Message" << delimiter << "File\n";
    
    auto filtered_expected = getFilteredEntries_NoLock(filter); // Use non-locking version as we already hold the lock
    if (!filtered_expected) {
        std::cerr << "Error filtering entries for CSV export: " << filtered_expected.error().message << std::endl;
        return;
    }
    const auto& filtered = filtered_expected.value();
    
    for (const auto& entry : filtered) {
        std::string timestampStr;
        if (entry.timestamp.has_value()) {
            timestampStr = Utils::formatTimestamp(entry.timestamp.value(), timestampFormat);
        } else {
            timestampStr = "";
        }
        out << timestampStr << delimiter;
        out << Utils::logLevelToString(entry.level) << delimiter;
        
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

void LogAnalyzer::exportAsJson(std::ostream &out, const FilterCriteria &filter, bool prettyPrint, std::string_view timestampFormat) const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
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
    // It will contain at least "count".
    out << indent << "\"summary\":{\"count\": " << filtered.size() << "}," << newline;
    out << indent << "\"entries\": [" << newline;

    for (size_t i = 0; i < filtered.size(); ++i) {
        const auto& entry = filtered[i];
        std::string timestampStr;
        if (entry.timestamp.has_value()) {
            timestampStr = Utils::formatTimestamp(entry.timestamp.value(), timestampFormat);
        } // else: timestampStr remains empty

        out << entryIndent; // Always use entryIndent for log entries
        out << "{";
        out << "\"ID\":\"" << entry.id << "\","; // Export ID
        if (entry.timestamp.has_value()) {
            out << "\"TIMESTAMP\":\"" << timestampStr << "\",";
        } else {
            out << "\"TIMESTAMP\":\"" << "\","; // Export empty string if no timestamp
        }
        out << "\"LEVEL\":\"" << Utils::logLevelToString(entry.level) << "\",";
        out << "\"MESSAGE\":\"" << Utils::escapeJsonString(entry.message) << "\",";
        out << "\"SOURCE_FILE\":\"" << Utils::escapeJsonString(entry.sourceFile) << "\","; // Changed key to SOURCE_FILE
        out << "\"LINE_NUMBER\":\"" << entry.sourceLineNumber << "\""; // Export LINE_NUMBER

        // Export custom fields
        if (!entry.customFields.empty()) {
            out << ", \"CUSTOM_FIELDS\": {";
            bool firstCustomField = true;
            for (const auto& pair : entry.customFields) {
                if (!firstCustomField) {
                    out << ",";
                }
                out << "\"" << Utils::escapeJsonString(pair.first) << "\":\"" << Utils::escapeJsonString(pair.second) << "\"";
                firstCustomField = false;
            }
            out << "}";
        }
        out << "}";
        if (i < filtered.size() - 1) {
            out << ",";
        }
        out << newline;
    }

    out << indent << "]" << newline;
    out << "}" << newline;
}
