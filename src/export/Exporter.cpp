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

// Merge implementation for ExportSettings
void ExportSettings::merge(const ExportSettings& other) {
    if (other.outputPath.has_value()) outputPath = other.outputPath;
    if (other.format.has_value()) format = other.format;
    if (!other.fieldsToExport.empty()) fieldsToExport = other.fieldsToExport; // Replace for now, or append? Usually fields list is a complete set.
    if (other.includeHeader.has_value()) includeHeader = other.includeHeader;
    if (other.jsonIndent.has_value()) jsonIndent = other.jsonIndent;
    if (other.separator.has_value()) separator = other.separator;
    if (other.textFormatString.has_value()) textFormatString = other.textFormatString;
    if (other.useAnsiColors.has_value()) useAnsiColors = other.useAnsiColors;
    if (other.sortBy.has_value()) sortBy = other.sortBy;
    if (other.sortOrder.has_value()) sortOrder = other.sortOrder;
    if (other.outputNoColor.has_value()) outputNoColor = other.outputNoColor;
    if (other.textOutputFormat.has_value()) textOutputFormat = other.textOutputFormat;
    if (other.includeSummary.has_value()) includeSummary = other.includeSummary;
    if (other.prettyPrint.has_value()) prettyPrint = other.prettyPrint;
    if (other.csvSeparator.has_value()) csvSeparator = other.csvSeparator;
    
    if (!other.csvFields.empty()) csvFields = other.csvFields;
    if (!other.jsonFields.empty()) jsonFields = other.jsonFields;
    
    if (other.topMessagesCount.has_value()) topMessagesCount = other.topMessagesCount;
    if (other.streamMode.has_value()) streamMode = other.streamMode;
    if (other.tailMode.has_value()) tailMode = other.tailMode;
    if (other.tailInterval.has_value()) tailInterval = other.tailInterval;
}

// JSON conversion for ExportFieldMapping
void to_json(nlohmann::json& j, const ExportFieldMapping& efm) {
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, LogEntryField>) {
            j["field"] = Utils::logEntryFieldToString(arg);
        } else if constexpr (std::is_same_v<T, std::string>) {
            j["field"] = arg;
        }
    }, efm.field);
    j["customHeader"] = efm.customHeader;
    if (efm.datetimeFormat) {
        j["datetimeFormat"] = *efm.datetimeFormat;
    }
}

void from_json(const nlohmann::json& j, ExportFieldMapping& efm) {
    if (!j.is_object()) {
        throw ExportException("ExportFieldMapping must be a JSON object.");
    }

    if (j.contains("field") && j.at("field").is_string()) {
        std::string fieldStr = j.at("field").get<std::string>();
        LogEntryField fieldEnum = Utils::stringToLogEntryField(fieldStr);
        if (fieldEnum != LogEntryField::UNKNOWN) {
            efm.field = fieldEnum;
        } else {
            efm.field = fieldStr; // Store as string for custom fields
        }
    } else {
        throw ExportException("ExportFieldMapping is missing or has invalid 'field'.");
    }

    if (j.contains("customHeader") && j.at("customHeader").is_string()) {
        efm.customHeader = j.at("customHeader").get<std::string>();
    }

    if (j.contains("datetimeFormat") && j.at("datetimeFormat").is_string()) {
        efm.datetimeFormat = j.at("datetimeFormat").get<std::string>();
    }
}

void Exporter::exportLogEntries(
    std::ostream& os,
    const std::vector<LogEntry>& entries,
    const ExportSettings& settings) {

    // Default to PLAINTEXT if not set
    ExportFormat effectiveFormat = settings.format.value_or(ExportFormat::PLAINTEXT);

    switch (effectiveFormat) {
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
            exportAsXml(os, entries, settings);
            break;
        case ExportFormat::UNKNOWN:
        default:
            throw ExportException("Unknown or unsupported export format specified.");
    }
}

void Exporter::exportAsJson(
    std::ostream& os,
    const std::vector<LogEntry>& entries,
    const ExportSettings& settings) {
    
    json j;
    j["entries"] = json::array();
    
    std::vector<ExportFieldMapping> fieldsToExport = getEffectiveExportFieldMappings(entries, settings);

    for (const auto& entry : entries) {
        json entryJson;
        
        for (const auto& fieldMapping : fieldsToExport) {
            std::string key;
            json value_json;

            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    key = arg;
                    if (entry.customFields.count(key)) {
                        value_json = entry.customFields.at(key);
                    } else {
                        value_json = nullptr;
                    }
                } else if constexpr (std::is_same_v<T, LogEntryField>) {
                    LogEntryField fieldEnum = arg;
                    key = fieldMapping.customHeader.empty() ? Utils::logEntryFieldToString(fieldEnum) : fieldMapping.customHeader;

                    switch (fieldEnum) {
                        case LogEntryField::ID:
                            value_json = entry.id.has_value() ? json(entry.id.value()) : nullptr;
                            break;
                        case LogEntryField::TIMESTAMP: {
                            if (entry.timestamp.has_value()) {
                                if (fieldMapping.datetimeFormat.has_value()) {
                                    value_json = Utils::formatTimestamp(entry.timestamp.value(), *fieldMapping.datetimeFormat);
                                } else {
                                    value_json = Utils::formatTimestamp(entry.timestamp.value());
                                }
                            } else {
                                value_json = nullptr;
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
                            value_json = entry.sourceFile;
                            break;
                        case LogEntryField::LINE_NUMBER:
                            value_json = entry.sourceLineNumber.has_value() ? json(entry.sourceLineNumber.value()) : nullptr;
                            break;
                        case LogEntryField::THREAD_ID:
                            value_json = entry.threadId.has_value() ? json(entry.threadId.value()) : nullptr;
                            break;
                        case LogEntryField::MODULE:
                            value_json = entry.module.has_value() ? json(entry.module.value()) : nullptr;
                            break;
                        case LogEntryField::HOST:
                            value_json = entry.host.has_value() ? json(entry.host.value()) : nullptr;
                            break;
                        case LogEntryField::STRUCTURED_FIELD:
                            if (entry.structuredData.has_value()) {
                                try {
                                    value_json = json::parse(entry.structuredData.value());
                                } catch (const json::parse_error&) {
                                    value_json = entry.structuredData.value();
                                }
                            } else {
                                value_json = nullptr;
                            }
                            break;
                        case LogEntryField::UNKNOWN:
                        default:
                            value_json = nullptr;
                            break;
                    }
                }
            }, fieldMapping.field);

            if (!key.empty()) {
                bool isStandardField = std::holds_alternative<LogEntryField>(fieldMapping.field);

                if (isStandardField || !value_json.is_null()) {
                    entryJson[key] = value_json;
                }
            }
        }
        j["entries"].push_back(entryJson);
    }
    
    j["summary"] = {
        {"count", entries.size()}
    };

    os << j.dump(settings.jsonIndent.value_or(-1)) << std::endl;
}

// Helper namespace for XML utilities
namespace {
    std::string xmlEscape(const std::string& data) {
        std::string buffer;
        buffer.reserve(data.size());
        for (char c : data) {
            switch (c) {
                case '&':  buffer.append("&amp;");       break;
                case '\"': buffer.append("&quot;");      break;
                case '\'': buffer.append("&apos;");      break;
                case '<':  buffer.append("&lt;");        break;
                case '>':  buffer.append("&gt;");        break;
                default:   buffer.push_back(c);         break;
            }
        }
        return buffer;
    }

    void jsonToXml(const json& j, std::ostream& os, int indentLevel) {
        std::string indent(indentLevel * 2, ' ');
        if (j.is_object()) {
            for (auto it = j.begin(); it != j.end(); ++it) {
                os << indent << "<" << xmlEscape(it.key()) << ">";
                if (it.value().is_primitive() || it.value().is_null()) {
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
                os << indent << "<item>";
                if (item.is_primitive() || item.is_null()) {
                    os << xmlEscape(item.dump());
                } else {
                    os << std::endl;
                    jsonToXml(item, os, indentLevel + 1);
                    os << indent;
                }
                os << "</item>" << std::endl;
            }
        }
    }
}

void Exporter::exportAsCsv(
    std::ostream& os,
    const std::vector<LogEntry>& entries,
    const ExportSettings& settings) {

    std::vector<ExportFieldMapping> fieldsToConsider = getEffectiveExportFieldMappings(entries, settings);

    if (settings.includeHeader.value_or(true) && !fieldsToConsider.empty()) {
        for (size_t i = 0; i < fieldsToConsider.size(); ++i) {
            const auto& fieldMapping = fieldsToConsider[i];
            std::string headerName;
            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    headerName = fieldMapping.customHeader.empty() ? arg : fieldMapping.customHeader;
                } else if constexpr (std::is_same_v<T, LogEntryField>) {
                    headerName = fieldMapping.customHeader.empty() ? Utils::logEntryFieldToString(arg) : fieldMapping.customHeader;
                }
            }, fieldMapping.field);

            os << formatCsvField(headerName, settings.separator.value_or(','));
            if (i < fieldsToConsider.size() - 1) os << settings.separator.value_or(',');
        }
        os << std::endl;
    }

    for (const auto& entry : entries) {
        for (size_t i = 0; i < fieldsToConsider.size(); ++i) {
            const auto& fieldMapping = fieldsToConsider[i];
            std::string value_str;
            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    if (entry.customFields.count(arg)) {
                        value_str = entry.customFields.at(arg);
                    }
                } else if constexpr (std::is_same_v<T, LogEntryField>) {
                    switch (arg) {
                        case LogEntryField::ID:
                            value_str = entry.id.has_value() ? std::to_string(entry.id.value()) : "";
                            break;
                        case LogEntryField::TIMESTAMP:
                            if (entry.timestamp.has_value()) {
                                value_str = fieldMapping.datetimeFormat.has_value() ?
                                    Utils::formatTimestamp(entry.timestamp.value(), *fieldMapping.datetimeFormat) :
                                    Utils::formatTimestamp(entry.timestamp.value());
                            }
                            break;
                        case LogEntryField::LEVEL: value_str = Utils::logLevelToString(entry.level); break;
                        case LogEntryField::MESSAGE: value_str = entry.message; break;
                        case LogEntryField::SOURCE_FILE: value_str = entry.sourceFile; break;
                        case LogEntryField::LINE_NUMBER: value_str = entry.sourceLineNumber.has_value() ? std::to_string(entry.sourceLineNumber.value()) : ""; break;
                        case LogEntryField::THREAD_ID: value_str = entry.threadId.value_or(""); break;
                        case LogEntryField::MODULE: value_str = entry.module.value_or(""); break;
                        case LogEntryField::HOST: value_str = entry.host.value_or(""); break;
                        case LogEntryField::STRUCTURED_FIELD: value_str = entry.structuredData.value_or(""); break;
                        default: break;
                    }
                }
            }, fieldMapping.field);

            os << formatCsvField(value_str, settings.separator.value_or(','));
            if (i < fieldsToConsider.size() - 1) os << settings.separator.value_or(',');
        }
        os << std::endl;
    }
}

void Exporter::exportAsText(
    std::ostream& os, 
    const std::vector<LogEntry>& entries, 
    const ExportSettings& settings) {
    
    for (const auto& entry : entries) {
        os << formatEntryForText(entry, settings.textFormatString.value_or("{timestamp} [{level}] {message}"), settings.useAnsiColors.value_or(false)) << std::endl;
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
            bool isStructured = false;

            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    tagName = fieldMapping.customHeader.empty() ? arg : fieldMapping.customHeader;
                    if (entry.customFields.count(arg)) {
                        value = entry.customFields.at(arg);
                    }
                } else if constexpr (std::is_same_v<T, LogEntryField>) {
                    tagName = fieldMapping.customHeader.empty() ? Utils::logEntryFieldToString(arg) : fieldMapping.customHeader;
                    switch (arg) {
                        case LogEntryField::ID: value = entry.id.has_value() ? std::to_string(entry.id.value()) : ""; break;
                        case LogEntryField::TIMESTAMP:
                            if (entry.timestamp.has_value()) {
                                value = fieldMapping.datetimeFormat.has_value() ? Utils::formatTimestamp(entry.timestamp.value(), *fieldMapping.datetimeFormat) : Utils::formatTimestamp(entry.timestamp.value());
                            }
                            break;
                        case LogEntryField::LEVEL: value = Utils::logLevelToString(entry.level); break;
                        case LogEntryField::MESSAGE: value = entry.message; break;
                        case LogEntryField::SOURCE_FILE: value = entry.sourceFile; break;
                        case LogEntryField::LINE_NUMBER: value = entry.sourceLineNumber.has_value() ? std::to_string(entry.sourceLineNumber.value()) : ""; break;
                        case LogEntryField::THREAD_ID: value = entry.threadId.value_or(""); break;
                        case LogEntryField::MODULE: value = entry.module.value_or(""); break;
                        case LogEntryField::HOST: value = entry.host.value_or(""); break;
                        case LogEntryField::STRUCTURED_FIELD:
                            isStructured = true;
                            if (entry.structuredData.has_value()) {
                                value = *entry.structuredData;
                            }
                            break;
                        default: break;
                    }
                }
            }, fieldMapping.field);

            if (!tagName.empty()) {
                os << "    <" << xmlEscape(tagName) << ">";
                if (isStructured) {
                    try {
                        json structuredJson = json::parse(value);
                        os << std::endl;
                        jsonToXml(structuredJson, os, 3);
                        os << "    ";
                    } catch (const json::parse_error&) {
                        os << "<![CDATA[" << value << "]]>";
                    }
                } else {
                    os << xmlEscape(value);
                }
                os << "</" << xmlEscape(tagName) << ">" << std::endl;
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

    std::string result = formatString;
    std::string levelStr = Utils::logLevelToString(entry.level);
    std::string finalLevelStr = levelStr;

    if (useColors) {
        std::string colorCode;
        if (entry.level == LogLevel::ERROR || entry.level == LogLevel::FATAL) colorCode = Utils::AnsiColor::RED;
        else if (entry.level == LogLevel::WARNING) colorCode = Utils::AnsiColor::YELLOW;
        else if (entry.level == LogLevel::INFO) colorCode = Utils::AnsiColor::CYAN;
        else if (entry.level == LogLevel::DEBUG || entry.level == LogLevel::TRACE) colorCode = Utils::AnsiColor::GREEN;
        if (!colorCode.empty()) {
            finalLevelStr = colorCode + levelStr + std::string(Utils::AnsiColor::RESET);
        }
    }

    Utils::replaceAll(result, std::string(PLACEHOLDER_LEVEL), finalLevelStr);
    Utils::replaceAll(result, std::string(PLACEHOLDER_ID), entry.id.has_value() ? std::to_string(entry.id.value()) : "");
    Utils::replaceAll(result, std::string(PLACEHOLDER_TIMESTAMP), entry.timestamp.has_value() ? Utils::formatTimestamp(entry.timestamp.value()) : "");
    Utils::replaceAll(result, std::string(PLACEHOLDER_MESSAGE), entry.message);

    for (const auto& [key, val] : entry.customFields) {
        std::string placeholder = std::string(PLACEHOLDER_CUSTOM_PREFIX) + key + "}";
        Utils::replaceAll(result, placeholder, val);
    }
    return result;
}

std::vector<ExportFieldMapping> Exporter::getEffectiveExportFieldMappings(
    const std::vector<LogEntry>& entries,
    const ExportSettings& settings) {

    if (!settings.fieldsToExport.empty()) {
        return settings.fieldsToExport;
    }

    std::vector<ExportFieldMapping> effectiveFields;
    effectiveFields.emplace_back(LogEntryField::ID, "ID");
    effectiveFields.emplace_back(LogEntryField::TIMESTAMP, "TIMESTAMP");
    effectiveFields.emplace_back(LogEntryField::LEVEL, "LEVEL");
    effectiveFields.emplace_back(LogEntryField::MESSAGE, "MESSAGE");

    std::set<std::string> uniqueCustomFieldNames;
    for (const auto& entry : entries) {
        for (const auto& [key, val] : entry.customFields) {
            uniqueCustomFieldNames.insert(key);
        }
    }
    for (const auto& fieldName : uniqueCustomFieldNames) {
        effectiveFields.emplace_back(fieldName, fieldName);
    }
    return effectiveFields;
}

void to_json(nlohmann::json& j, const ExportSettings& es) {
    j = json::object();
    if (es.outputPath.has_value()) j["outputPath"] = es.outputPath.value();
    if (es.format.has_value()) j["format"] = Utils::exportFormatToString(es.format.value());
    if (es.includeHeader.has_value()) j["includeHeader"] = es.includeHeader.value();
    if (es.separator.has_value()) j["separator"] = std::string(1, es.separator.value());
    if (es.textFormatString.has_value()) j["textFormatString"] = es.textFormatString.value();
    if (es.useAnsiColors.has_value()) j["useAnsiColors"] = es.useAnsiColors.value();
    if (!es.fieldsToExport.empty()) j["fieldsToExport"] = es.fieldsToExport;
    if (es.jsonIndent.has_value()) {
        j["jsonIndent"] = es.jsonIndent.value();
    }
}

void from_json(const nlohmann::json& j, ExportSettings& es) {
    es = ExportSettings(); 
    if (j.contains("outputPath")) es.outputPath = j.at("outputPath").get<std::string>();
    if (j.contains("fieldsToExport")) es.fieldsToExport = j.at("fieldsToExport").get<std::vector<ExportFieldMapping>>();
    if (j.contains("format")) {
        auto formatOpt = Utils::stringToExportFormat(j.at("format").get<std::string>());
        if(formatOpt) es.format = *formatOpt;
        else throw ExportException("Invalid format string provided.");
    }
    if (j.contains("includeHeader")) es.includeHeader = j.at("includeHeader").get<bool>();
    if (j.contains("jsonIndent")) es.jsonIndent = j.at("jsonIndent").get<int>();
    if (j.contains("separator")) {
        std::string sep_str = j.at("separator").get<std::string>();
        if (sep_str.length() != 1) {
            throw ExportException("Separator must be a single character string.");
        }
        es.separator = sep_str[0];
    }
    if (j.contains("textFormatString")) es.textFormatString = j.at("textFormatString").get<std::string>();
    if (j.contains("useAnsiColors")) es.useAnsiColors = j.at("useAnsiColors").get<bool>();
}

std::string Exporter::formatCsvField(const std::string& value, char separator) {
    bool needsQuotes = value.empty() || value.find(separator) != std::string::npos || value.find('"') != std::string::npos || value.find('\n') != std::string::npos || value.find('\r') != std::string::npos;
    if (!needsQuotes) {
        return value;
    }
    std::string escaped = "\"";
    for (char c : value) {
        if (c == '"') escaped += "\"\"";
        else escaped += c;
    }
    escaped += "\"";
    return escaped;
}
