// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "export/core.h"
#include "utils/string.h"
#include <sstream>

// Placeholder for text export (can be enhanced later)
void Exporter::exportAsText(
    std::ostream& os, 
    const std::vector<LogEntry>& entries, 
    const ExportSettings& settings) {
    
    for (const auto& entry : entries) {
        os << formatEntryForText(entry, settings.textFormatString.value_or("{timestamp} [{level}] {message}"), settings.useAnsiColors.value_or(false)) << '\n';
    }
}

// Helper to format a single log entry for text output
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
    Utils::replaceAll(result, std::string(PLACEHOLDER_ID), entry.id ? std::to_string(*entry.id) : "");
    Utils::replaceAll(result, std::string(PLACEHOLDER_TIMESTAMP), entry.timestamp ? Utils::formatTimestamp(*entry.timestamp) : "");
    Utils::replaceAll(result, std::string(PLACEHOLDER_MESSAGE), entry.message);
    Utils::replaceAll(result, std::string(PLACEHOLDER_SOURCE_FILE), entry.sourceFile);
    Utils::replaceAll(result, std::string(PLACEHOLDER_LINE_NUMBER), entry.sourceLineNumber ? std::to_string(*entry.sourceLineNumber) : "");
    Utils::replaceAll(result, std::string(PLACEHOLDER_THREAD_ID), entry.threadId.value_or(""));
    Utils::replaceAll(result, std::string(PLACEHOLDER_MODULE), entry.module.value_or(""));
    Utils::replaceAll(result, std::string(PLACEHOLDER_HOST), entry.host.value_or(""));
    std::string customFieldsStr;
    if (!entry.customFields.empty()) {
        std::ostringstream customFields;
        bool first = true;
        for (const auto& [key, value] : entry.customFields) {
            if (!first) {
                customFields << ";";
            }
            customFields << key << ":" << value;
            first = false;
        }
        customFieldsStr = customFields.str();
    }
    Utils::replaceAll(result, std::string(PLACEHOLDER_CUSTOM_FIELDS), customFieldsStr);

    for (const auto& [key, val] : entry.customFields) {
        std::string placeholder = std::string(PLACEHOLDER_CUSTOM_PREFIX) + key + "}";
        Utils::replaceAll(result, placeholder, val);
    }
    return result;
}
