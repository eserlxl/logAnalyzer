// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer/Core.h"
#include "utils/String.h"
#include "utils/UtilsCore.h"
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
    out << "{" << newline;
    // Ensure "summary" root element is always present for consistency.
    // It will contain at least "count".
    out << indent << "\"summary\":{\"count\": " << filtered.size() << "}," << newline;
    out << indent << "\"entries\": [" << newline;

    for (size_t i = 0; i < filtered.size(); ++i) {
        const auto& entry = filtered[i];
        
        // Use nlohmann::json to construct the entry object for robustness
        nlohmann::json entryJson;

        if (entry.id.has_value()) {
            entryJson["ID"] = entry.id.value();
        } else {
            entryJson["ID"] = nullptr;
        }

        std::string timestampStr;
        if (entry.timestamp.has_value()) {
            timestampStr = Utils::formatTimestamp(entry.timestamp.value(), timestampFormat);
            entryJson["TIMESTAMP"] = timestampStr;
        } else {
            entryJson["TIMESTAMP"] = nullptr;
        }
        
        entryJson["LEVEL"] = Utils::logLevelToString(entry.level);
        entryJson["MESSAGE"] = entry.message;
        entryJson["SOURCE_FILE"] = entry.sourceFile;
        
        if (entry.sourceLineNumber.has_value()) {
            entryJson["LINE_NUMBER"] = entry.sourceLineNumber.value();
        } else {
            entryJson["LINE_NUMBER"] = nullptr;
        }

        if (entry.threadId.has_value()) {
            entryJson["THREAD_ID"] = entry.threadId.value();
        } else {
            entryJson["THREAD_ID"] = nullptr;
        }

        if (entry.module.has_value()) {
            entryJson["MODULE"] = entry.module.value();
        } else {
            entryJson["MODULE"] = nullptr;
        }

        if (entry.host.has_value()) {
            entryJson["HOST"] = entry.host.value();
        } else {
            entryJson["HOST"] = nullptr;
        }

        // Export custom fields
        if (!entry.customFields.empty()) {
            nlohmann::json customFieldsJson;
            for (const auto& pair : entry.customFields) {
                customFieldsJson[Utils::escapeJsonString(pair.first)] = Utils::escapeJsonString(pair.second);
            }
            entryJson["CUSTOM_FIELDS"] = customFieldsJson;
        }

        // Output the constructed JSON object
        if (prettyPrint) {
            out << entryJson.dump(4, ' ', true, nlohmann::json::error_handler_t::replace) << (i < filtered.size() - 1 ? "," : "") << newline;
        } else {
            out << entryJson.dump(-1, ' ', true, nlohmann::json::error_handler_t::replace) << (i < filtered.size() - 1 ? "," : "") << newline;
        }
    }

    out << indent << "]" << newline;
    out << "}" << newline;
}
