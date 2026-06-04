// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "export/core.h"
#include "utils/string.h"
#include <sstream>
#include <optional>
#include <string_view>

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

    // Resolve a placeholder token (with braces) to its value, or std::nullopt when
    // the token is not a recognized placeholder so it is emitted verbatim.
    const auto resolve = [&](std::string_view token) -> std::optional<std::string> {
        if (token == PLACEHOLDER_LEVEL) return finalLevelStr;
        if (token == PLACEHOLDER_ID) return entry.id ? std::to_string(*entry.id) : std::string();
        if (token == PLACEHOLDER_TIMESTAMP) return entry.timestamp ? Utils::formatTimestamp(*entry.timestamp) : std::string();
        if (token == PLACEHOLDER_MESSAGE) return entry.message;
        if (token == PLACEHOLDER_SOURCE_FILE) return entry.sourceFile;
        if (token == PLACEHOLDER_LINE_NUMBER) return entry.sourceLineNumber ? std::to_string(*entry.sourceLineNumber) : std::string();
        if (token == PLACEHOLDER_THREAD_ID) return entry.threadId.value_or("");
        if (token == PLACEHOLDER_MODULE) return entry.module.value_or("");
        if (token == PLACEHOLDER_HOST) return entry.host.value_or("");
        if (token == PLACEHOLDER_CUSTOM_FIELDS) return customFieldsStr;
        if (token.size() > PLACEHOLDER_CUSTOM_PREFIX.size() &&
            token.starts_with(PLACEHOLDER_CUSTOM_PREFIX) &&
            token.back() == '}') {
            const std::string key(token.substr(PLACEHOLDER_CUSTOM_PREFIX.size(),
                                               token.size() - PLACEHOLDER_CUSTOM_PREFIX.size() - 1));
            if (auto it = entry.customFields.find(key); it != entry.customFields.end()) {
                return it->second;
            }
        }
        return std::nullopt;
    };

    // Substitute in a single left-to-right pass. Only recognized placeholders are
    // consumed; substituted values are appended directly and never re-scanned, so
    // a field value that itself contains a placeholder token is emitted literally.
    std::string result;
    result.reserve(formatString.size());
    size_t i = 0;
    while (i < formatString.size()) {
        if (formatString[i] == '{') {
            const size_t close = formatString.find('}', i);
            if (close != std::string::npos) {
                const std::string_view token(formatString.data() + i, close - i + 1);
                if (auto value = resolve(token)) {
                    result += *value;
                    i = close + 1;
                    continue;
                }
            }
        }
        result += formatString[i];
        ++i;
    }
    return result;
}
