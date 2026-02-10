// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer/core.h"
#include "utils/string.h"
#include "utils/core.h"
#include <iostream>
#include <vector>
#include <string>

void LogAnalyzer::exportAsCsv(std::ostream& out, const filter::FilterCriteria& filter, char delimiter, std::string_view timestampFormat) const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    
    // CSV Header
    out << "ID" << delimiter << "Timestamp" << delimiter << "Level" << delimiter << "Message" << delimiter << "SourceFile\n";
    
    auto filtered_expected = getFilteredEntries_NoLock(filter); // Use non-locking version as we already hold the lock
    if (!filtered_expected) {
        std::cerr << "Error filtering entries for CSV export: " << filtered_expected.error().message << '\n';
        return;
    }
    const auto& filtered = filtered_expected.value();
    
    for (const auto& entry : filtered) {
        // Output ID
        if (entry.id.has_value()) {
            out << *entry.id << delimiter;
        } else {
            out << "" << delimiter;
        }

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

void LogAnalyzer::exportAsJson(std::ostream &out, const filter::FilterCriteria &filter, bool prettyPrint, std::string_view timestampFormat) const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    auto filtered_expected = getFilteredEntries_NoLock(filter); // Use non-locking version as we already hold the lock
    if (!filtered_expected) {
        std::cerr << "Error filtering entries for JSON export: " << filtered_expected.error().message << '\n';
        return;
    }
    const auto& filtered = filtered_expected.value();

    nlohmann::json rootJson;
    rootJson["summary"]["count"] = filtered.size();
    nlohmann::json entriesArray = nlohmann::json::array();

    for (const auto& entry : filtered) {
        nlohmann::json entryJson;

        if (entry.id.has_value()) {
            entryJson["Id"] = entry.id.value();
        } else {
            entryJson["Id"] = nullptr;
        }

        std::string timestampStr;
        if (entry.timestamp.has_value()) {
            timestampStr = Utils::formatTimestamp(entry.timestamp.value(), timestampFormat);
            entryJson["Timestamp"] = timestampStr;
        } else {
            entryJson["Timestamp"] = nullptr;
        }
        
        entryJson["Level"] = Utils::logLevelToString(entry.level);
        entryJson["Message"] = entry.message;
        entryJson["SourceFile"] = entry.sourceFile;
        
        if (entry.sourceLineNumber.has_value()) {
            entryJson["LineNumber"] = entry.sourceLineNumber.value();
        } else {
            entryJson["LineNumber"] = nullptr;
        }

        if (entry.threadId.has_value()) {
            entryJson["ThreadId"] = entry.threadId.value();
        } else {
            entryJson["ThreadId"] = nullptr;
        }

        if (entry.module.has_value()) {
            entryJson["Module"] = entry.module.value();
        } else {
            entryJson["Module"] = nullptr;
        }

        if (entry.host.has_value()) {
            entryJson["Host"] = entry.host.value();
        } else {
            entryJson["Host"] = nullptr;
        }

        // Export custom fields
        if (!entry.customFields.empty()) {
            nlohmann::json customFieldsJson;
            for (const auto& pair : entry.customFields) {
                // No need to escape here, nlohmann::json::dump handles escaping automatically
                customFieldsJson[pair.first] = pair.second;
            }
            entryJson["CustomFields"] = customFieldsJson;
        }
        entriesArray.push_back(entryJson);
    }
    rootJson["entries"] = entriesArray;

    if (prettyPrint) {
        out << rootJson.dump(4, ' ', true, nlohmann::json::error_handler_t::replace) << '\n';
    } else {
        out << rootJson.dump(-1, ' ', true, nlohmann::json::error_handler_t::replace) << '\n';
    }
}

void LogAnalyzer::exportAsCsv(std::ostream& out, const filter::FilterExpression& expression, char delimiter, std::string_view timestampFormat) const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    
    // CSV Header
    out << "ID" << delimiter << "Timestamp" << delimiter << "Level" << delimiter << "Message" << delimiter << "SourceFile\n";
    
    auto filtered_expected = getFilteredEntries_NoLock(expression);
    if (!filtered_expected) {
        std::cerr << "Error filtering entries for CSV export: " << filtered_expected.error().message << '\n';
        return;
    }
    const auto& filtered = filtered_expected.value();
    
    for (const auto& entry : filtered) {
        // Output ID
        if (entry.id.has_value()) {
            out << *entry.id << delimiter;
        } else {
            out << "" << delimiter;
        }

        std::string timestampStr;
        if (entry.timestamp.has_value()) {
            timestampStr = Utils::formatTimestamp(entry.timestamp.value(), timestampFormat);
        } else {
            timestampStr = "";
        }
        out << timestampStr << delimiter;
        out << Utils::logLevelToString(entry.level) << delimiter;
        
        std::string msg = entry.message;
        bool needsQuotes = msg.find(delimiter) != std::string::npos || 
                           msg.find('"') != std::string::npos ||
                           msg.find('\n') != std::string::npos ||
                           msg.find('\r') != std::string::npos;
                           
        if (needsQuotes) {
            size_t pos = msg.find('"');
            while (pos != std::string::npos) {
                msg.replace(pos, 1, "\"\"");
                pos = msg.find('"', pos + 2);
            }
            out << '"' << msg << '"';
        } else {
            out << msg;
        }
        
        out << delimiter << entry.sourceFile << "\n";
    }
}

void LogAnalyzer::exportAsJson(std::ostream &out, const filter::FilterExpression &expression, bool prettyPrint, std::string_view timestampFormat) const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    auto filtered_expected = getFilteredEntries_NoLock(expression);
    if (!filtered_expected) {
        std::cerr << "Error filtering entries for JSON export: " << filtered_expected.error().message << '\n';
        return;
    }
    const auto& filtered = filtered_expected.value();

    nlohmann::json rootJson;
    rootJson["summary"]["count"] = filtered.size();
    nlohmann::json entriesArray = nlohmann::json::array();

    for (const auto& entry : filtered) {
        nlohmann::json entryJson;

        if (entry.id.has_value()) {
            entryJson["Id"] = entry.id.value();
        } else {
            entryJson["Id"] = nullptr;
        }

        std::string timestampStr;
        if (entry.timestamp.has_value()) {
            timestampStr = Utils::formatTimestamp(entry.timestamp.value(), timestampFormat);
            entryJson["Timestamp"] = timestampStr;
        } else {
            entryJson["Timestamp"] = nullptr;
        }
        
        entryJson["Level"] = Utils::logLevelToString(entry.level);
        entryJson["Message"] = entry.message;
        entryJson["SourceFile"] = entry.sourceFile;
        
        if (entry.sourceLineNumber.has_value()) {
            entryJson["LineNumber"] = entry.sourceLineNumber.value();
        } else {
            entryJson["LineNumber"] = nullptr;
        }

        if (entry.threadId.has_value()) {
            entryJson["ThreadId"] = entry.threadId.value();
        } else {
            entryJson["ThreadId"] = nullptr;
        }

        if (entry.module.has_value()) {
            entryJson["Module"] = entry.module.value();
        } else {
            entryJson["Module"] = nullptr;
        }

        if (entry.host.has_value()) {
            entryJson["Host"] = entry.host.value();
        } else {
            entryJson["Host"] = nullptr;
        }

        if (!entry.customFields.empty()) {
            nlohmann::json customFieldsJson;
            for (const auto& pair : entry.customFields) {
                customFieldsJson[pair.first] = pair.second;
            }
            entryJson["CustomFields"] = customFieldsJson;
        }
        entriesArray.push_back(entryJson);
    }
    rootJson["entries"] = entriesArray;

    if (prettyPrint) {
        out << rootJson.dump(4, ' ', true, nlohmann::json::error_handler_t::replace) << '\n';
    } else {
        out << rootJson.dump(-1, ' ', true, nlohmann::json::error_handler_t::replace) << '\n';
    }
}
