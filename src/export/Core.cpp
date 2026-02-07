// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "export/Core.h"
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
