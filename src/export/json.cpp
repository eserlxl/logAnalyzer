// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "export/core.h"
#include "utils/String.h"
#include <iostream>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
    efm = ExportFieldMapping{};

    if (j.contains("field") && j.at("field").is_string()) {
        std::string fieldStr = Utils::trim(j.at("field").get<std::string>());
        if (fieldStr.empty()) {
            throw ExportException("ExportFieldMapping 'field' cannot be empty.");
        }
        LogEntryField fieldEnum = Utils::stringToLogEntryField(fieldStr);
        if (fieldEnum != LogEntryField::UNKNOWN) {
            efm.field = fieldEnum;
        } else {
            efm.field = fieldStr; // Store as string for custom fields
        }
    } else {
        throw ExportException("ExportFieldMapping is missing or has invalid 'field'.");
    }

    if (j.contains("customHeader")) {
        if (j.at("customHeader").is_null()) {
            efm.customHeader.clear();
        } else if (j.at("customHeader").is_string()) {
            efm.customHeader = j.at("customHeader").get<std::string>();
        } else {
            throw ExportException("ExportFieldMapping has invalid 'customHeader' type; expected string or null.");
        }
    }

    if (j.contains("datetimeFormat")) {
        if (j.at("datetimeFormat").is_null()) {
            efm.datetimeFormat.reset();
        } else if (j.at("datetimeFormat").is_string()) {
            const std::string trimmedFormat = Utils::trim(j.at("datetimeFormat").get<std::string>());
            if (trimmedFormat.empty()) {
                throw ExportException("ExportFieldMapping 'datetimeFormat' cannot be empty.");
            }
            efm.datetimeFormat = trimmedFormat;
        } else {
            throw ExportException("ExportFieldMapping has invalid 'datetimeFormat' type; expected string or null.");
        }
    }

    if (efm.datetimeFormat && std::holds_alternative<LogEntryField>(efm.field)) {
        if (std::get<LogEntryField>(efm.field) != LogEntryField::TIMESTAMP) {
            throw ExportException("ExportFieldMapping 'datetimeFormat' is only valid for TIMESTAMP field.");
        }
    }
    if (efm.datetimeFormat && std::holds_alternative<std::string>(efm.field)) {
        throw ExportException("ExportFieldMapping 'datetimeFormat' is not supported for custom fields.");
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
                            value_json = entry.id ? json(*entry.id) : nullptr;
                            break;
                        case LogEntryField::TIMESTAMP: {
                            if (entry.timestamp) {
                                if (fieldMapping.datetimeFormat) {
                                    value_json = Utils::formatTimestamp(*entry.timestamp, *fieldMapping.datetimeFormat);
                                } else {
                                    value_json = Utils::formatTimestamp(*entry.timestamp);
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
                            value_json = entry.sourceLineNumber ? json(*entry.sourceLineNumber) : nullptr;
                            break;
                        case LogEntryField::THREAD_ID:
                            value_json = entry.threadId ? json(*entry.threadId) : nullptr;
                            break;
                        case LogEntryField::MODULE:
                            value_json = entry.module ? json(*entry.module) : nullptr;
                            break;
                        case LogEntryField::HOST:
                            value_json = entry.host ? json(*entry.host) : nullptr;
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

    os << j.dump(settings.jsonIndent.value_or(-1)) << '\n';
}

// Helper namespace for XML utilities
namespace {
    std::string xmlEscape(const std::string& data) {
        std::string buffer;
        buffer.reserve(data.size());
        for (char c : data) {
            switch (c) {
                case '&':  buffer.append("&amp;");       break;
                case '"': buffer.append("&quot;");      break;
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
                    os << '\n';
                    jsonToXml(it.value(), os, indentLevel + 1);
                    os << indent;
                }
                os << "</" << xmlEscape(it.key()) << '\n';
            }
        } else if (j.is_array()) {
            for (const auto& item : j) {
                os << indent << "<item>";
                if (item.is_primitive() || item.is_null()) {
                    os << xmlEscape(item.dump());
                } else {
                    os << '\n';
                    jsonToXml(item, os, indentLevel + 1);
                    os << indent;
                }
                os << "</item>" << '\n';
            }
        }
    }
}

void Exporter::exportAsXml(
    std::ostream& os,
    const std::vector<LogEntry>& entries,
    const ExportSettings& settings) {
    
    os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << '\n';
    os << "<log>" << '\n';

    std::vector<ExportFieldMapping> fieldsToConsider = getEffectiveExportFieldMappings(entries, settings);

    for (const auto& entry : entries) {
        os << "  <entry>" << '\n';
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
                        os << '\n';
                        jsonToXml(structuredJson, os, 3);
                        os << "    ";
                    } catch (const json::parse_error&) {
                        os << "<![CDATA[" << value << "]]>";
                    }
                } else {
                    os << xmlEscape(value);
                }
                os << "</" << xmlEscape(tagName) << '\n';
            }
        }
        os << "  </entry>" << '\n';
    }
    os << "</log>" << '\n';
}

void to_json(nlohmann::json& j, const ExportSettings& es) {
    j = json::object();
    if (es.outputPath) j["outputPath"] = *es.outputPath;
    if (es.format) j["format"] = Utils::exportFormatToString(*es.format);
    if (es.includeHeader) j["includeHeader"] = *es.includeHeader;
    if (es.separator) j["separator"] = std::string(1, *es.separator);
    if (es.textFormatString) j["textFormatString"] = *es.textFormatString;
    if (es.useAnsiColors) j["useAnsiColors"] = *es.useAnsiColors;
    if (!es.fieldsToExport.empty()) j["fieldsToExport"] = es.fieldsToExport;
    if (es.jsonIndent) {
        j["jsonIndent"] = *es.jsonIndent;
    }
}

void from_json(const nlohmann::json& j, ExportSettings& es) {
    if (!j.is_object()) {
        throw ExportException("ExportSettings must be a JSON object.");
    }
    es = ExportSettings(); 
    if (j.contains("outputPath")) {
        if (j.at("outputPath").is_null()) {
            es.outputPath.reset();
        } else if (j.at("outputPath").is_string()) {
            es.outputPath = j.at("outputPath").get<std::string>();
        } else {
            throw ExportException("'outputPath' must be a string or null.");
        }
    }
    if (j.contains("fieldsToExport")) {
        if (j.at("fieldsToExport").is_null()) {
            es.fieldsToExport.clear();
        } else if (j.at("fieldsToExport").is_array()) {
            es.fieldsToExport = j.at("fieldsToExport").get<std::vector<ExportFieldMapping>>();
        } else {
            throw ExportException("'fieldsToExport' must be an array or null.");
        }
    }
    if (j.contains("format")) {
        if (j.at("format").is_null()) {
            es.format.reset();
        } else if (j.at("format").is_string()) {
            auto formatOpt = Utils::stringToExportFormat(j.at("format").get<std::string>());
            if (formatOpt) es.format = *formatOpt;
            else throw ExportException("Invalid format string provided.");
        } else {
            throw ExportException("'format' must be a string or null.");
        }
    }
    if (j.contains("includeHeader")) {
        if (j.at("includeHeader").is_null()) {
            es.includeHeader.reset();
        } else if (j.at("includeHeader").is_boolean()) {
            es.includeHeader = j.at("includeHeader").get<bool>();
        } else {
            throw ExportException("'includeHeader' must be a boolean or null.");
        }
    }
    if (j.contains("jsonIndent")) {
        if (j.at("jsonIndent").is_null()) {
            es.jsonIndent.reset();
        } else if (j.at("jsonIndent").is_number_integer()) {
            const int indent = j.at("jsonIndent").get<int>();
            if (indent < 0) {
                throw ExportException("'jsonIndent' must be non-negative.");
            }
            es.jsonIndent = indent;
        } else {
            throw ExportException("'jsonIndent' must be an integer or null.");
        }
    }
    if (j.contains("separator")) {
        if (j.at("separator").is_null()) {
            es.separator.reset();
        } else if (j.at("separator").is_string()) {
            std::string sep_str = j.at("separator").get<std::string>();
            if (sep_str.length() != 1) {
                throw ExportException("Separator must be a single character string.");
            }
            es.separator = sep_str[0];
        } else {
            throw ExportException("'separator' must be a single-character string or null.");
        }
    }
    if (j.contains("textFormatString")) {
        if (j.at("textFormatString").is_null()) {
            es.textFormatString.reset();
        } else if (j.at("textFormatString").is_string()) {
            es.textFormatString = j.at("textFormatString").get<std::string>();
        } else {
            throw ExportException("'textFormatString' must be a string or null.");
        }
    }
    if (j.contains("useAnsiColors")) {
        if (j.at("useAnsiColors").is_null()) {
            es.useAnsiColors.reset();
        } else if (j.at("useAnsiColors").is_boolean()) {
            es.useAnsiColors = j.at("useAnsiColors").get<bool>();
        } else {
            throw ExportException("'useAnsiColors' must be a boolean or null.");
        }
    }
}
