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
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, LogEntryField>) {
            j["field"] = Utils::logEntryFieldToString(arg);
        } else if constexpr (std::is_same_v<T, std::string>) {
            j["field"] = arg;
        } else {
            // Should not happen with current variant types
            j["field"] = nullptr;
        }
    }, efm.field);
    j["customHeader"] = efm.customHeader;
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
            std::string key;
            json value_json; // Use json type for value to handle different types correctly

            // Use std::visit to handle the std::variant 'field' member
            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    // Custom string field
                    key = arg; // The string itself is the key
                    if (!key.empty() && entry.customFields.count(key)) {
                        value_json = entry.customFields.at(key);
                    } else {
                        value_json = json::value_t::null; // Null if custom field not found or key is empty
                    }
                } else if constexpr (std::is_same_v<T, LogEntryField>) {
                    // Standard LogEntryField
                    LogEntryField fieldEnum = arg;
                    key = fieldMapping.customHeader.empty() ? Utils::logEntryFieldToString(fieldEnum) : fieldMapping.customHeader;

                    switch (fieldEnum) {
                        case LogEntryField::ID:
                            value_json = entry.id.has_value() ? json(entry.id.value()) : json::value_t::null;
                            break;
                        case LogEntryField::TIMESTAMP: {
                            if (entry.timestamp.has_value()) {
                                std::string timestampValue;
                                if (fieldMapping.datetimeFormat.has_value()) {
                                    timestampValue = Utils::formatTimestamp(entry.timestamp.value(), fieldMapping.datetimeFormat.value());
                                } else {
                                    timestampValue = Utils::formatTimestamp(entry.timestamp.value());
                                }
                                value_json = timestampValue;
                            } else {
                                value_json = json::value_t::null;
                            }
                            break;
                        }
                        case LogEntryField::LEVEL:
                            value_json = Utils::logLevelToString(entry.level);
                            break;
                        case LogEntryField::MESSAGE:
                            value_json = entry.message;
                            break;
                        case LogEntryField::SOURCE_FILE:
                            value_str = entry.sourceFile; // Assign to value_str first to check for nullopt
                            value_json = !entry.sourceFile.empty() ? json(entry.sourceFile) : json::value_t::null;
                            break;
                        case LogEntryField::LINE_NUMBER:
                            value_json = entry.sourceLineNumber.has_value() ? json(entry.sourceLineNumber.value()) : json::value_t::null;
                            break;
                        case LogEntryField::THREAD_ID:
                            value_json = entry.threadId.has_value() ? json(entry.threadId.value()) : json::value_t::null;
                            break;
                        case LogEntryField::MODULE:
                            value_json = entry.module.has_value() ? json(entry.module.value()) : json::value_t::null;
                            break;
                        case LogEntryField::HOST:
                            value_json = entry.host.has_value() ? json(entry.host.value()) : json::value_t::null;
                            break;
                        case LogEntryField::STRUCTURED_FIELD:
                            if (entry.structuredData.has_value()) {
                                try {
                                    value_json = json::parse(entry.structuredData.value());
                                } catch (const json::parse_error& e) {
                                    value_json = entry.structuredData.value(); // Treat as string if parsing fails
                                }
                            } else {
                                value_json = json::value_t::null;
                            }
                            break;
                        case LogEntryField::UNKNOWN:
                        default:
                            value_json = json::value_t::null;
                            break;
                    }
                }
            } else {
                 // Fallback for unexpected variant types (should not happen)
                 key = "unknown_field"; // Or handle appropriately
                 value_json = json::value_t::null;
            }

            if (!key.empty()) {
                entryJson[key] = value_json;
            }
        }
        // Add the populated entryJson to the main entries array
        j["entries"].push_back(entryJson);
    }
    
    // Ensure "summary" root element is always present for consistency
    j["summary"] = {
        {"count", entries.size()}
    };

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

            // Determine the header name. This part needs to correctly interpret fieldMapping.field
            // whether it's a string (custom field name) or LogEntryField enum.
            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    headerName = arg; // Custom field name is the header name
                } else if constexpr (std::is_same_v<T, LogEntryField>) {
                    headerName = fieldMapping.customHeader.empty() ? Utils::logEntryFieldToString(arg) : fieldMapping.customHeader;
                }
            }, fieldMapping.field);

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

            // Use std::visit to handle the std::variant 'field' member and extract the value
            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                
                if constexpr (std::is_same_v<T, std::string>) {
                    // It's a custom string field name. Use it to look up in customFields.
                    const std::string& fieldName = arg;
                    if (!fieldName.empty() && entry.customFields.count(fieldName)) {
                        value_str = entry.customFields.at(fieldName);
                    } else {
                        value_str = ""; // Empty string if custom field not found or name is empty.
                    }
                } else if constexpr (std::is_same_v<T, LogEntryField>) {
                    // It's a standard LogEntryField. Use the switch statement for known fields.
                    LogEntryField fieldEnum = arg;
                    switch (fieldEnum) {
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
                }
            }, fieldMapping.field); // Pass the variant to std::visit

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
        effectiveFields.emplace_back(LogEntryField::ID, "ID");
        effectiveFields.emplace_back(LogEntryField::TIMESTAMP, "TIMESTAMP");
        effectiveFields.emplace_back(LogEntryField::LEVEL, "LEVEL");
        effectiveFields.emplace_back(LogEntryField::MESSAGE, "MESSAGE");

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
