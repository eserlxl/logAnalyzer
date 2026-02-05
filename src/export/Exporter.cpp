// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "export/Exporter.h"
#include "utils/String.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <set>

using json = nlohmann::json;

// JSON conversion for ExportFieldMapping
void to_json(nlohmann::json& j, const ExportFieldMapping& efm) {
    j = nlohmann::json{
        {"field", Utils::logEntryFieldToString(efm.field)},
        {"customHeader", efm.customHeader}
    };
    if (efm.datetimeFormat) {
        j["datetimeFormat"] = *efm.datetimeFormat;
    }
}

void from_json(const nlohmann::json& j, ExportFieldMapping& efm) {
    std::vector<std::string> errors;

    if (j.contains("field") && j.at("field").is_string()) {
        std::string fieldStr = j.at("field").get<std::string>();
        efm.field = Utils::stringToLogEntryField(fieldStr);
        if (efm.field == LogEntryField::UNKNOWN && fieldStr != "UNKNOWN") {
             errors.push_back("ExportFieldMapping has an unrecognized field: " + fieldStr);
        }
    } else {
        errors.push_back("ExportFieldMapping is missing or has invalid 'field'.");
    }

    if (j.contains("customHeader") && j.at("customHeader").is_string()) {
        efm.customHeader = j.at("customHeader").get<std::string>();
    } // customHeader is optional

    if (j.contains("datetimeFormat") && j.at("datetimeFormat").is_string()) {
        efm.datetimeFormat = j.at("datetimeFormat").get<std::string>();
    } // datetimeFormat is optional

    if (!errors.empty()) {
        throw ExportException(errors[0]);
    }
}

void Exporter::exportLogEntries(
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
            exportAsXml(os, entries, settings); // Call exportAsXml directly
            break;
        case ExportFormat::UNKNOWN:
        default:
            // Throw an exception for unknown or unsupported export formats
            throw ExportException("Unknown or unsupported export format specified.");
    }
}

void Exporter::exportAsJson(
    std::ostream& os,
    const std::vector<LogEntry>& entries,
    const ExportSettings& settings) {
    
    json j;
    j["entries"] = json::array();
    
    // Determine the fields to export based on settings.fieldsToExport or discover them
    std::vector<ExportFieldMapping> fieldsToExport = getEffectiveExportFieldMappings(entries, settings);

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
                        if (entry.structuredData.has_value()) {
                            try {
                                json structuredJson = json::parse(entry.structuredData.value());
                                entryJson[key] = structuredJson;
                            } catch (const json::parse_error& e) {
                                // If parsing fails, treat it as a plain string
                                entryJson[key] = entry.structuredData.value();
                            }
                        } else {
                            entryJson[key] = json::value_t::null;
                        }
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

// Helper function for XML escaping
namespace {
    std::string xmlEscape(const std::string& data) {
        std::string buffer;
        buffer.reserve(data.size());
        for (size_t pos = 0; pos != data.size(); ++pos) {
            switch (data[pos]) {
                case '&':  buffer.append("&amp;");       break;
                case '\"': buffer.append("&quot;");      break;
                case '\'': buffer.append("&apos;");      break;
                case '<':  buffer.append("&lt;");        break;
                case '>':  buffer.append("&gt;");        break;
                default:   buffer.append(1, data[pos]); break;
            }
        }
        return buffer;
    }

    // Helper function to convert JSON to XML
    void jsonToXml(const nlohmann::json& j, std::ostream& os, int indentLevel) {
        std::string indent(indentLevel * 2, ' '); // 2 spaces per indent level

        if (j.is_object()) {
            for (auto it = j.begin(); it != j.end(); ++it) {
                os << indent << "<" << xmlEscape(it.key()) << ">";
                if (it.value().is_primitive()) {
                    os << xmlEscape(it.value().dump());
                } else {
                    os << std::endl;
                    jsonToXml(it.value(), os, indentLevel + 1);
                    os << indent;
                }
                os << "</" << xmlEscape(it.key()) << ">" << std::endl;
            }
        } else if (j.is_array()) {
            for (const auto& item : j) {
                os << indent << "<item>"; // Generic item tag for array elements
                if (item.is_primitive()) {
                    os << xmlEscape(item.dump());
                } else {
                    os << std::endl;
                    jsonToXml(item, os, indentLevel + 1);
                    os << indent;
                }
                os << "</item>" << std::endl;
            }
        } else if (j.is_primitive()) {
            os << xmlEscape(j.dump());
        }
    }
} // anonymous namespace


void Exporter::exportAsCsv(
    std::ostream& os,
    const std::vector<LogEntry>& entries,
    const ExportSettings& settings) {

    std::vector<ExportFieldMapping> fieldsToConsider = getEffectiveExportFieldMappings(entries, settings);

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
            os << formatCsvField(headerName, settings.separator);
            
            if (i < fieldsToConsider.size() - 1) {
                os << settings.separator;
            }
        }
        os << std::endl;
    }

    for (const auto& entry : entries) {
        for (size_t i = 0; i < fieldsToConsider.size(); ++i) {
            const auto& fieldMapping = fieldsToConsider[i];
            std::string value_str; // Declare here
            switch (fieldMapping.field) {
                case LogEntryField::ID:
                    if (entry.id.has_value()) {
                        value_str = std::to_string(entry.id.value());
                    } else {
                        value_str = "";
                    }
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
                    if (entry.structuredData.has_value()) {
                        value_str = entry.structuredData.value(); // Export raw structured data as string for CSV
                    } else {
                        value_str = "";
                    }
                    break;
                case LogEntryField::UNKNOWN:
                default:
                    value_str = ""; // For unhandled or unknown fields, export an empty string
                    break;
            }

            os << formatCsvField(value_str, settings.separator);
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

void Exporter::exportAsXml(
    std::ostream& os,
    const std::vector<LogEntry>& entries,
    const ExportSettings& settings) {
    
    os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
    os << "<log>" << std::endl;

    std::vector<ExportFieldMapping> fieldsToConsider = getEffectiveExportFieldMappings(entries, settings);

    for (const auto& entry : entries) {
        os << "  <entry>" << std::endl;
        for (const auto& fieldMapping : fieldsToConsider) {
            std::string tagName;
            std::string value;

            if (fieldMapping.field == LogEntryField::CUSTOM) {
                tagName = fieldMapping.customHeader;
                if (!tagName.empty() && entry.customFields.count(tagName)) {
                    value = entry.customFields.at(tagName);
                }
            } else {
                tagName = fieldMapping.customHeader.empty() ? Utils::logEntryFieldToString(fieldMapping.field) : fieldMapping.customHeader;
                switch (fieldMapping.field) {
                    case LogEntryField::ID:
                        if (entry.id.has_value()) {
                            value = std::to_string(entry.id.value());
                        }
                        break;
                    case LogEntryField::TIMESTAMP:
                        if (entry.timestamp.has_value()) {
                            if (fieldMapping.datetimeFormat.has_value()) {
                                value = Utils::formatTimestamp(entry.timestamp.value(), fieldMapping.datetimeFormat.value());
                            } else {
                                value = Utils::formatTimestamp(entry.timestamp.value());
                            }
                        }
                        break;
                    case LogEntryField::LEVEL:
                        value = Utils::logLevelToString(entry.level);
                        break;
                    case LogEntryField::MESSAGE:
                        value = entry.message;
                        break;
                    case LogEntryField::SOURCE_FILE:
                        value = entry.sourceFile;
                        break;
                    case LogEntryField::LINE_NUMBER:
                        if (entry.sourceLineNumber.has_value()) {
                            value = std::to_string(entry.sourceLineNumber.value());
                        }
                        break;
                    case LogEntryField::THREAD_ID:
                        if (entry.threadId.has_value()) {
                            value = entry.threadId.value();
                        }
                        break;
                    case LogEntryField::MODULE:
                        if (entry.module.has_value()) {
                            value = entry.module.value();
                        }
                        break;
                    case LogEntryField::HOST:
                        if (entry.host.has_value()) {
                            value = entry.host.value();
                        }
                        break;
                    case LogEntryField::STRUCTURED_FIELD:
                        if (!tagName.empty()) {
                            os << "    <" << tagName << ">";
                            if (entry.structuredData.has_value()) {
                                try {
                                    json structuredJson = json::parse(entry.structuredData.value());
                                    os << std::endl; // Newline for pretty printing nested XML
                                    jsonToXml(structuredJson, os, 3); // Indent level 3 for nested content
                                    os << "    "; // Indent before closing tag
                                } catch (const json::parse_error& e) {
                                    // If parsing fails, treat it as plain text and wrap in CDATA
                                    os << "<![CDATA[" << entry.structuredData.value() << "]]>";
                                }
                            }
                            os << "</" << tagName << ">" << std::endl;
                        }
                        break;
                }
            }
            if (!tagName.empty()) {
                os << "    <" << tagName << ">" << xmlEscape(value) << "</" << tagName << ">" << std::endl;
            }
        }
        os << "  </entry>" << std::endl;
    }
    os << "</log>" << std::endl;
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
    Utils::replaceAll(result, std::string(PLACEHOLDER_LEVEL), finalLevelStr);
    // Substitute other placeholders
    Utils::replaceAll(result, std::string(PLACEHOLDER_ID), entry.id.has_value() ? std::to_string(entry.id.value()) : "");
    Utils::replaceAll(result, std::string(PLACEHOLDER_TIMESTAMP), entry.timestamp.has_value() ? Utils::formatTimestamp(entry.timestamp.value()) : "");
    Utils::replaceAll(result, std::string(PLACEHOLDER_MESSAGE), entry.message);

    // Handle custom field placeholders like {custom.fieldName}
    // Note: This approach allows for custom fields to be placed anywhere in the format string,
    // but does not handle custom field names with special characters that might conflict with placeholder syntax.
    for (const auto& customField : entry.customFields) {
        std::string placeholder = std::string(PLACEHOLDER_CUSTOM_PREFIX) + customField.first + "}";
        Utils::replaceAll(result, placeholder, customField.second);
    }
    return result;
}

std::vector<ExportFieldMapping> Exporter::getEffectiveExportFieldMappings(
    const std::vector<LogEntry>& entries,
    const ExportSettings& settings) {

    std::vector<ExportFieldMapping> effectiveFields = settings.fieldsToExport;

    if (effectiveFields.empty()) {
        // Default to standard fields
        effectiveFields.emplace_back(LogEntryField::ID, "id");
        effectiveFields.emplace_back(LogEntryField::TIMESTAMP, "timestamp");
        effectiveFields.emplace_back(LogEntryField::LEVEL, "level");
        effectiveFields.emplace_back(LogEntryField::MESSAGE, "message");
        effectiveFields.emplace_back(LogEntryField::SOURCE_FILE, "sourceFile");
        effectiveFields.emplace_back(LogEntryField::LINE_NUMBER, "lineNumber");
        effectiveFields.emplace_back(LogEntryField::THREAD_ID, "threadId");
        effectiveFields.emplace_back(LogEntryField::MODULE, "module");
        effectiveFields.emplace_back(LogEntryField::HOST, "host");
        effectiveFields.emplace_back(LogEntryField::STRUCTURED_FIELD, "structuredField");

        // Discover unique custom fields across all log entries
        std::set<std::string> uniqueCustomFieldNames;
        for (const auto& entry : entries) {
            for (const auto& customFieldPair : entry.customFields) {
                uniqueCustomFieldNames.insert(customFieldPair.first);
            }
        }
        for (const auto& fieldName : uniqueCustomFieldNames) {
            effectiveFields.emplace_back(LogEntryField::CUSTOM, fieldName);
        }
    }
    return effectiveFields;
}

// --- JSON Conversion for ExportSettings ---
void to_json(nlohmann::json& j, const ExportSettings& es) {
    j = nlohmann::json{
        {"outputPath", es.outputPath},
        {"format", Utils::exportFormatToString(es.format)},
        {"includeHeader", es.includeHeader},
        {"separator", std::string(1, es.separator)},
        {"textFormatString", es.textFormatString},
        {"useAnsiColors", es.useAnsiColors},
        {"fieldsToExport", es.fieldsToExport}
    };
    if (es.jsonIndent) {
        j["jsonIndent"] = *es.jsonIndent;
    }
}

void from_json(const nlohmann::json& j, ExportSettings& es) {
    // Default construct ensures fieldsToExport is empty by default, indicating "all standard fields"
    // if no specific fieldsToExport are provided in JSON.
    es = ExportSettings(); 

    if (j.contains("outputPath")) {
        if (j.at("outputPath").is_string()) {
            es.outputPath = j.at("outputPath").get<std::string>();
        } else {
             throw ExportException("ExportSettings: 'outputPath' has invalid type. Expected string.");
        }
    }

    if (j.contains("fieldsToExport")) {
        if (j.at("fieldsToExport").is_array()) {
            es.fieldsToExport = j.at("fieldsToExport").get<std::vector<ExportFieldMapping>>();
        } else {
            throw ExportException("ExportSettings: 'fieldsToExport' has invalid type. Expected array.");
        }
    }

    if (j.contains("format")) {
        if (j.at("format").is_string()) {
            std::string formatStr = j.at("format").get<std::string>();
            auto formatOpt = Utils::stringToExportFormat(formatStr);
            if (formatOpt) {
                es.format = *formatOpt;
            } else {
                 // If the string is not recognized, throw an error, as format is a critical setting.
                 throw ExportException("ExportSettings: 'format' has invalid value: " + formatStr);
            }
        } else {
            throw ExportException("ExportSettings: 'format' has invalid type. Expected string.");
        }
    }

    if (j.contains("includeHeader")) {
        if (j.at("includeHeader").is_boolean()) {
            es.includeHeader = j.at("includeHeader").get<bool>();
        } else {
            throw ExportException("ExportSettings: 'includeHeader' has invalid type. Expected boolean."); 
        }
    }

    if (j.contains("jsonIndent")) {
        if (j.at("jsonIndent").is_number_integer()) {
            es.jsonIndent = j.at("jsonIndent").get<int>();
        } else {
            throw ExportException("ExportSettings: 'jsonIndent' has invalid type. Expected integer.");
        }
    }

    if (j.contains("separator")) {
        if (j.at("separator").is_string() && j.at("separator").get<std::string>().length() == 1) {
            es.separator = j.at("separator").get<std::string>().at(0);
        } else {
            throw ExportException("ExportSettings: 'separator' has invalid type or length. Expected a single character string.");
        }
    }

    if (j.contains("textFormatString")) {
        if (j.at("textFormatString").is_string()) {
            es.textFormatString = j.at("textFormatString").get<std::string>();
        } else {
            throw ExportException("ExportSettings: 'textFormatString' has invalid type. Expected string.");
        }
    }

    if (j.contains("useAnsiColors")) {
        if (j.at("useAnsiColors").is_boolean()) {
            es.useAnsiColors = j.at("useAnsiColors").get<bool>();
        } else {
            throw ExportException("ExportSettings: 'useAnsiColors' has invalid type. Expected boolean.");
        }
    }
}

std::string Exporter::formatCsvField(const std::string& value, char separator) {
    bool needsOuterQuotes = false;
    // Quote if empty, or contains separator, double-quote, newline, or carriage return
    if (value.empty() ||
        value.find(separator) != std::string::npos ||
        value.find('"') != std::string::npos ||
        value.find('\n') != std::string::npos ||
        value.find('\r') != std::string::npos) {
        needsOuterQuotes = true;
    }

    std::string escapedValue = value;
    Utils::replaceAll(escapedValue, "\"", "\"\""); // Double internal quotes

    if (needsOuterQuotes) {
        return "\"" + escapedValue + "\"";
    } else {
        return escapedValue;
    }
}
