// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 eserlxl

#include "export/Exporter.h"
#include "analyzer/Core.h"
#include "utils/UtilsCore.h"
#include <nlohmann/json.hpp>
#include <iomanip>
// No longer need <iostream> for std::cerr (it was removed in the header fix)
#include <set> // Required for std::set in field discovery

using json = nlohmann::json;


ExportResult Exporter::exportLogEntries( // Changed return type
    std::ostream& os,
    const std::vector<LogEntry>& entries,
    const ExportSettings& settings) {

    switch (settings.format) {
        case ExportFormat::JSON:
            exportAsJson(os, entries, settings);
            break;
        case ExportFormat::CSV:
            exportAsCsv(os, entries, settings);
            break;
        case ExportFormat::PLAINTEXT:
            exportAsText(os, entries, settings);
            break;
        case ExportFormat::XML:
            // TODO: Implement XML export
            // Throw an exception for unimplemented XML export
            throw ExportException("XML export not yet implemented.");
        case ExportFormat::UNKNOWN:
        default:
            // Throw an exception for unknown export format
            throw ExportException("Unknown export format.");
    }
    return ExportResult::SUCCESS;
}

void Exporter::exportAsJson(
    std::ostream& os,
    const std::vector<LogEntry>& entries,
    const ExportSettings& settings) {
    
    json j;
    j["entries"] = json::array();
    
    // Determine the fields to export based on settings.fieldsToExport
    std::vector<ExportFieldMapping> fieldsToExport = settings.fieldsToExport;

    if (fieldsToExport.empty()) {
        // Default to standard fields + discover custom fields if settings.fieldsToExport is empty
        fieldsToExport.emplace_back(LogEntryField::ID, "ID");
        // Ensure datetime format can be applied to timestamp if specified in settings.fieldsToExport later.
        // For default, just use the field name.
        fieldsToExport.emplace_back(LogEntryField::TIMESTAMP, "Timestamp"); 
        fieldsToExport.emplace_back(LogEntryField::LEVEL, "Level");
        fieldsToExport.emplace_back(LogEntryField::MESSAGE, "Message");

        // Discover unique custom fields across all log entries
        std::set<std::string> uniqueCustomFieldNames;
        for (const auto& entry : entries) {
            for (const auto& customFieldPair : entry.customFields) {
                uniqueCustomFieldNames.insert(customFieldPair.first);
            }
        }
        for (const auto& fieldName : uniqueCustomFieldNames) {
            // For custom fields, the 'customHeader' will be the actual field name used as key.
            fieldsToExport.emplace_back(LogEntryField::CUSTOM, fieldName);
        }
    }

    for (const auto& entry : entries) {
        json entryJson; // Create an empty JSON object for the entry
        
        for (const auto& fieldMapping : fieldsToExport) {
            std::string value;
            std::string key;

            if (fieldMapping.field == LogEntryField::CUSTOM) {
                key = fieldMapping.customHeader; // Key is the custom field name
                // Only add custom field to JSON if the key is not empty AND it exists in the log entry
                if (!key.empty() && entry.customFields.count(key)) {
                    entryJson[key] = entry.customFields.at(key);
                } 
                // If key is empty or custom field not found, we don't add it to entryJson.
            } else {
                // For standard fields, use customHeader if provided, otherwise use string representation of field.
                key = fieldMapping.customHeader.empty() ? Utils::logEntryFieldToString(fieldMapping.field) : fieldMapping.customHeader;

                // Always assign standard field value to entryJson if a valid key was determined.
                // The key should always be valid here.
                switch (fieldMapping.field) {
                    case LogEntryField::ID:
                        if (entry.id.has_value()) {
                            entryJson[key] = entry.id.value();
                        } else {
                            entryJson[key] = json::value_t::null;
                        }
                        break;
                    case LogEntryField::TIMESTAMP: {
                        if (entry.timestamp.has_value()) {
                            std::string timestampValue;
                            if (fieldMapping.datetimeFormat.has_value()) {
                                timestampValue = Utils::formatTimestamp(entry.timestamp.value(), fieldMapping.datetimeFormat.value());
                            } else {
                                timestampValue = Utils::formatTimestamp(entry.timestamp.value());
                            }
                            entryJson[key] = timestampValue;
                        } else {
                            entryJson[key] = json::value_t::null; // Explicitly null if timestamp is absent
                        }
                        break;
                    }
                    case LogEntryField::LEVEL:
                        entryJson[key] = Utils::logLevelToString(entry.level);
                        break;
                    case LogEntryField::MESSAGE:
                        entryJson[key] = entry.message; // Always assign, even if empty
                        break;
                    case LogEntryField::SOURCE_FILE:
                        entryJson[key] = entry.sourceFile; // Always assign, even if empty
                        break;
                    case LogEntryField::LINE_NUMBER:
                        if (entry.sourceLineNumber.has_value()) {
                            entryJson[key] = entry.sourceLineNumber.value();
                        } else {
                            entryJson[key] = json::value_t::null;
                        }
                        break;
                    case LogEntryField::THREAD_ID:
                        if (entry.threadId.has_value()) {
                            entryJson[key] = entry.threadId.value();
                        } else {
                            entryJson[key] = json::value_t::null;
                        }
                        break;
                    case LogEntryField::MODULE:
                        if (entry.module.has_value()) {
                            entryJson[key] = entry.module.value();
                        } else {
                            entryJson[key] = json::value_t::null;
                        }
                        break;
                    case LogEntryField::HOST:
                        if (entry.host.has_value()) {
                            entryJson[key] = entry.host.value();
                        } else {
                            entryJson[key] = json::value_t::null;
                        }
                        break;
                    case LogEntryField::STRUCTURED_FIELD:
                        // Structured fields need special handling for parsing.
                        // For export, if explicitly requested and LogEntry has no structured data, export null or empty object.
                        entryJson[key] = json::value_t::null; // Or json::object()
                        break;
                    case LogEntryField::UNKNOWN:
                    default:
                        // If UNKNOWN field type is somehow requested, or an unhandled enum,
                        // it's an error in fieldMapping definition or program logic.
                        // We will add a null entry to ensure the key exists but with no value.
                        entryJson[key] = json::value_t::null;
                        break;
                }
            }
        }
        // Add the populated entryJson to the main entries array
        j["entries"].push_back(entryJson);
    }
    
    // Ensure "summary" root element is always present for consistency
    j["summary"] = {
        {"count", entries.size()}
    };

    // Temporary debug print to inspect the JSON object before dumping
    // std::cerr << "DEBUG: JSON object before dump: " << j.dump(2) << std::endl;

    if (settings.jsonIndent.has_value() && settings.jsonIndent.value() >= 0) {
        os << j.dump(settings.jsonIndent.value()) << std::endl;
    } else {
        os << j.dump() << std::endl;
    }
}

// Helper function for CSV escaping (only escapes internal quotes, does NOT add outer quotes)
namespace {
    std::string csvEscapeInternal(const std::string& value) {
        std::string escapedValue = value;
        Utils::replaceAll(escapedValue, "\"", "\"\""); // Double internal quotes
        return escapedValue;
    }
} // anonymous namespace


void Exporter::exportAsCsv(
    std::ostream& os,
    const std::vector<LogEntry>& entries,
    const ExportSettings& settings) {

    std::vector<ExportFieldMapping> fieldsToConsider = settings.fieldsToExport;

    // If fieldsToExport is empty, discover default fields and all available custom fields
    if (fieldsToConsider.empty()) {
        fieldsToConsider.emplace_back(LogEntryField::ID, "ID");
        fieldsToConsider.emplace_back(LogEntryField::TIMESTAMP, "Timestamp");
        fieldsToConsider.emplace_back(LogEntryField::LEVEL, "Level");
        fieldsToConsider.emplace_back(LogEntryField::MESSAGE, "Message");

        // Discover unique custom fields across all log entries
        std::set<std::string> uniqueCustomFieldNames;
        for (const auto& entry : entries) {
            for (const auto& customFieldPair : entry.customFields) {
                uniqueCustomFieldNames.insert(customFieldPair.first);
            }
        }
        for (const auto& fieldName : uniqueCustomFieldNames) {
            // For custom fields, the 'customHeader' will be the actual field name.
            // LogEntryField::CUSTOM acts as a placeholder.
            fieldsToConsider.emplace_back(LogEntryField::CUSTOM, fieldName);
        }
    }

    if (settings.includeHeader && !fieldsToConsider.empty()) {
        for (size_t i = 0; i < fieldsToConsider.size(); ++i) {
            const auto& fieldMapping = fieldsToConsider[i];
            std::string headerName;

            if (fieldMapping.field == LogEntryField::CUSTOM) {
                // For CUSTOM fields, the customHeader is the actual field name.
                // If customHeader is empty, it's an invalid mapping for a custom field.
                if (!fieldMapping.customHeader.empty()) {
                    headerName = fieldMapping.customHeader;
                } else {
                    // This case should ideally not happen if custom fields are discovered correctly,
                    // but as a fallback, we could log a warning or use a placeholder.
                    // For now, let's treat it as an empty header for this specific field.
                    headerName = ""; 
                }
            } else {
                // For standard fields, use customHeader if provided, otherwise use string representation.
                if (!fieldMapping.customHeader.empty()) {
                    headerName = fieldMapping.customHeader;
                } else {
                    headerName = Utils::logEntryFieldToString(fieldMapping.field);
                }
            }
            // Headers are always treated as strings and should be quoted if they contain special chars
            bool needsHeaderQuotes = false;
            if (headerName.empty() ||
                headerName.find(settings.separator) != std::string::npos ||
                headerName.find('"') != std::string::npos ||
                headerName.find('\n') != std::string::npos ||
                headerName.find('\r') != std::string::npos) {
                needsHeaderQuotes = true;
            }
            std::string escapedHeader = csvEscapeInternal(headerName);
            if (needsHeaderQuotes) {
                os << "\"" << escapedHeader << "\"";
            } else {
                os << escapedHeader;
            }
            
            if (i < fieldsToConsider.size() - 1) {
                os << settings.separator;
            }
        }
        os << std::endl;
    }

    for (const auto& entry : entries) {
        for (size_t i = 0; i < fieldsToConsider.size(); ++i) {
            const auto& fieldMapping = fieldsToConsider[i];
            std::string value_str; 
            bool is_numeric_field = false;

            switch (fieldMapping.field) {
                case LogEntryField::ID:
                    if (entry.id.has_value()) {
                        value_str = std::to_string(entry.id.value());
                    } else {
                        value_str = "";
                    }
                    is_numeric_field = true;
                    break;
                case LogEntryField::TIMESTAMP:
                    if (entry.timestamp.has_value()) {
                        if (fieldMapping.datetimeFormat.has_value()) {
                            value_str = Utils::formatTimestamp(entry.timestamp.value(), fieldMapping.datetimeFormat.value());
                        } else {
                            value_str = Utils::formatTimestamp(entry.timestamp.value());
                        }
                    } else {
                        value_str = ""; // Empty string if no timestamp
                    }
                    break;
                case LogEntryField::LEVEL:
                    value_str = Utils::logLevelToString(entry.level);
                    break;
                case LogEntryField::MESSAGE:
                    value_str = entry.message;
                    break;
                case LogEntryField::CUSTOM:
                    // For CUSTOM fields, use customHeader to find the value in entry.customFields.
                    if (!fieldMapping.customHeader.empty() && entry.customFields.count(fieldMapping.customHeader)) {
                        value_str = entry.customFields.at(fieldMapping.customHeader);
                    } else {
                        value_str = ""; // If customHeader is empty or the field doesn't exist, output an empty string.
                    }
                    break;
                case LogEntryField::SOURCE_FILE:
                    value_str = entry.sourceFile;
                    break;
                case LogEntryField::LINE_NUMBER:
                    if (entry.sourceLineNumber.has_value()) {
                        value_str = std::to_string(entry.sourceLineNumber.value());
                    } else {
                        value_str = "";
                    }
                    is_numeric_field = true;
                    break;
                case LogEntryField::THREAD_ID:
                    if (entry.threadId.has_value()) {
                        value_str = entry.threadId.value();
                    } else {
                        value_str = "";
                    }
                    break;
                case LogEntryField::MODULE:
                    if (entry.module.has_value()) {
                        value_str = entry.module.value();
                    } else {
                        value_str = "";
                    }
                    break;
                case LogEntryField::HOST:
                    if (entry.host.has_value()) {
                        value_str = entry.host.value();
                    } else {
                        value_str = "";
                    }
                    break;
                case LogEntryField::STRUCTURED_FIELD:
                case LogEntryField::UNKNOWN:
                default:
                    value_str = ""; // For unhandled or unknown fields, export an empty string
                    break;
            }

            if (is_numeric_field) {
                os << value_str; // Output numeric directly
            } else {
                bool needsOuterQuotes = false;
                // Quote if empty, or contains separator, double-quote, newline, or carriage return
                if (value_str.empty() ||
                    value_str.find(settings.separator) != std::string::npos ||
                    value_str.find('"') != std::string::npos ||
                    value_str.find('\n') != std::string::npos ||
                    value_str.find('\r') != std::string::npos) {
                    needsOuterQuotes = true;
                }
                
                std::string escapedValue = csvEscapeInternal(value_str); // Apply internal escaping
                if (needsOuterQuotes) {
                    os << "\"" << escapedValue << "\"";
                } else {
                    os << escapedValue;
                }
            }
            if (i < fieldsToConsider.size() - 1) {
                os << settings.separator;
            }
        }
        os << std::endl;
    }
}

void Exporter::exportAsText(
    std::ostream& os, 
    const std::vector<LogEntry>& entries, 
    const ExportSettings& settings) {
    
    for (const auto& entry : entries) {
        os << formatEntryForText(entry, settings.textFormatString, settings.useAnsiColors) << std::endl;
    }
}


std::string Exporter::formatEntryForText(
    const LogEntry& entry,
    const std::string& formatString,
    bool useColors) {

    std::string result = formatString; // Start with the user-defined format string
    std::string levelStr = Utils::logLevelToString(entry.level); // Get plain level string
    std::string finalLevelStr = levelStr; // Initialize final level string to plain

    if (useColors) {
        std::string colorCode;
        if (entry.level == LogLevel::ERROR || entry.level == LogLevel::FATAL) {
            colorCode = Utils::AnsiColor::RED;
        } else if (entry.level == LogLevel::WARNING) {
            colorCode = Utils::AnsiColor::YELLOW;
        } else if (entry.level == LogLevel::INFO) {
            colorCode = Utils::AnsiColor::CYAN;
        } else if (entry.level == LogLevel::DEBUG || entry.level == LogLevel::TRACE) {
            colorCode = Utils::AnsiColor::GREEN;
        }
        // If a color code was determined, prepend it and append the reset code
        if (!colorCode.empty()) {
            finalLevelStr = colorCode + levelStr + std::string(Utils::AnsiColor::RESET);
        }
    }

    // Perform all placeholder replacements in the result string
    // Substitute {level} with the potentially colored string
    Utils::replaceAll(result, "{level}", finalLevelStr);
    // Substitute other placeholders
    Utils::replaceAll(result, "{id}", entry.id.has_value() ? std::to_string(entry.id.value()) : "");
    Utils::replaceAll(result, "{timestamp}", entry.timestamp.has_value() ? Utils::formatTimestamp(entry.timestamp.value()) : "");
    Utils::replaceAll(result, "{message}", entry.message);

    return result;
}
