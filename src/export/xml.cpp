// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "export/core.h"
#include "utils/string.h"
#include <iostream>
#include <string>
#include <vector>

namespace {
// Function to escape characters for XML
std::string escapeXml(const std::string &value) {
    std::string escaped;
    escaped.reserve(value.length());
    for (char c : value) {
        switch (c) {
            case '&':  escaped += "&amp;";       break;
            case '<':  escaped += "&lt;";        break;
            case '>':  escaped += "&gt;";        break;
            case '"':  escaped += "&quot;";      break;
            case '\'': escaped += "&apos;";      break;
            default:   escaped += c;             break;
        }
    }
    return escaped;
}

void writeXmlField(std::ostream& os, const std::string& name, const std::string& value, int indent) {
    if (value.empty()) return;
    os << std::string(indent, ' ') << "<" << name << ">" << escapeXml(value) << "</" << name << ">" << '\n';
}

} // namespace

void Exporter::exportAsXml(
    std::ostream& os,
    const std::vector<LogEntry>& entries,
    const ExportSettings& settings) {

    int indent = settings.jsonIndent.value_or(0);
    bool pretty = indent > 0;

    os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << (pretty ? "\n" : "");
    os << "<logs>" << (pretty ? "\n" : "");

    for (const auto& entry : entries) {
        os << std::string(pretty ? indent : 0, ' ') << "<entry>" << (pretty ? "\n" : "");

        int fieldIndent = pretty ? indent * 2 : 0;
        if (entry.id) {
            writeXmlField(os, "id", std::to_string(*entry.id), fieldIndent);
        }
        if (entry.timestamp) {
            writeXmlField(os, "timestamp", Utils::formatTimestamp(*entry.timestamp), fieldIndent);
        }
        writeXmlField(os, "level", Utils::logLevelToString(entry.level), fieldIndent);
        writeXmlField(os, "message", entry.message, fieldIndent);
        if (!entry.sourceFile.empty()) {
            writeXmlField(os, "sourceFile", entry.sourceFile, fieldIndent);
        }
        if (entry.sourceLineNumber) {
            writeXmlField(os, "lineNumber", std::to_string(*entry.sourceLineNumber), fieldIndent);
        }
        if (entry.threadId) {
            writeXmlField(os, "threadId", *entry.threadId, fieldIndent);
        }
        if (entry.module) {
            writeXmlField(os, "module", *entry.module, fieldIndent);
        }
        if (entry.host) {
            writeXmlField(os, "host", *entry.host, fieldIndent);
        }

        if (!entry.customFields.empty()) {
            os << std::string(fieldIndent, ' ') << "<customFields>" << (pretty ? "\n" : "");
            for (const auto& [key, value] : entry.customFields) {
                // Basic XML sanitization for key
                std::string safe_key = key;
                Utils::replaceAll(safe_key, " ", "_");
                writeXmlField(os, safe_key, value, pretty ? indent * 3 : 0);
            }
            os << std::string(fieldIndent, ' ') << "</customFields>" << (pretty ? "\n" : "");
        }

        os << std::string(pretty ? indent : 0, ' ') << "</entry>" << (pretty ? "\n" : "");
    }

    os << "</logs>" << (pretty ? "\n" : "");
}
