// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "export/core.h"
#include "utils/string.h"
#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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

    // Coerce an arbitrary key into a well-formed XML element name. XML Names start
    // with a letter, '_' or ':' and may otherwise contain digits, '-' and '.'. Any
    // other character (e.g. '&', '<', a space) is mapped to '_', and a '_' is
    // prepended when the first character is not a valid start char (e.g. a leading
    // digit), so an arbitrary log key never produces malformed XML.
    std::string sanitizeXmlName(const std::string& key) {
        const auto isNameStart = [](unsigned char c) {
            return std::isalpha(c) != 0 || c == '_' || c == ':';
        };
        const auto isNameChar = [&](unsigned char c) {
            return isNameStart(c) || std::isdigit(c) != 0 || c == '-' || c == '.';
        };
        std::string result;
        result.reserve(key.size() + 1);
        for (char ch : key) {
            const auto uc = static_cast<unsigned char>(ch);
            result += isNameChar(uc) ? ch : '_';
        }
        if (result.empty() || !isNameStart(static_cast<unsigned char>(result.front()))) {
            result.insert(result.begin(), '_');
        }
        return result;
    }

    void jsonToXml(const json& j, std::ostream& os, int indentLevel) {
        std::string indent(indentLevel * 2, ' ');
        if (j.is_object()) {
            for (auto it = j.begin(); it != j.end(); ++it) {
                const std::string tag = sanitizeXmlName(it.key());
                os << indent << "<" << tag << ">";
                if (it.value().is_primitive() || it.value().is_null()) {
                    os << xmlEscape(it.value().dump());
                } else {
                    os << '\n';
                    jsonToXml(it.value(), os, indentLevel + 1);
                    os << indent;
                }
                os << "</" << tag << ">" << '\n';
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
                    if (arg == LogEntryField::STRUCTURED_FIELD) {
                        // XML expands structured data into nested elements rather than
                        // emitting the raw string, so keep this case local.
                        isStructured = true;
                        value = entry.structuredData.value_or("");
                    } else {
                        value = standardFieldValue(entry, arg, fieldMapping.datetimeFormat);
                    }
                }
            }, fieldMapping.field);

            if (!tagName.empty()) {
                const std::string xmlTag = sanitizeXmlName(tagName);
                os << "    <" << xmlTag << ">";
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
                os << "</" << xmlTag << ">" << '\n';
            }
        }
        os << "  </entry>" << '\n';
    }
    os << "</log>" << '\n';
}
