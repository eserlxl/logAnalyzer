// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "export/core.h"
#include "utils/string.h"
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
    if (other.outputPath) outputPath = other.outputPath;
    if (other.format) format = other.format;
    if (!other.fieldsToExport.empty()) fieldsToExport = other.fieldsToExport; // Replace for now, or append? Usually fields list is a complete set.
    if (other.includeHeader) includeHeader = other.includeHeader;
    if (other.jsonIndent) jsonIndent = other.jsonIndent;
    if (other.separator) separator = other.separator;
    if (other.textFormatString) textFormatString = other.textFormatString;
    if (other.useAnsiColors) useAnsiColors = other.useAnsiColors;
    if (other.sortBy) sortBy = other.sortBy;
    if (other.sortOrder) sortOrder = other.sortOrder;
    if (other.outputNoColor) outputNoColor = other.outputNoColor;
    if (other.textOutputFormat) textOutputFormat = other.textOutputFormat;
    if (other.includeSummary) includeSummary = other.includeSummary;
    if (other.prettyPrint) prettyPrint = other.prettyPrint;
    if (other.csvSeparator) csvSeparator = other.csvSeparator;
    
    if (!other.csvFields.empty()) csvFields = other.csvFields;
    if (!other.jsonFields.empty()) jsonFields = other.jsonFields;
    
    if (other.topMessagesCount) topMessagesCount = other.topMessagesCount;
    if (other.streamMode) streamMode = other.streamMode;
    if (other.tailMode) tailMode = other.tailMode;
    if (other.tailInterval) tailInterval = other.tailInterval;
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
