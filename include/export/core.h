// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include <nlohmann/json.hpp>
#include "core/log/types.h"
#include "utils/core.h"
#include <iostream>
#include <vector>
#include <string>
#include <utility>
#include <optional>
#include <variant>
#include <stdexcept>

// New: Enum for different export formats
enum class ExportFormat {
    PLAINTEXT,
    JSON,
    NDJSON,
    CSV,
    XML,
    UNKNOWN // Default for unrecognized formats
};

// Forward declarations to break include cycles
// Full definitions are in Filter.h, Exporter.h, Statistics.h
enum class StatisticType;
namespace filter { enum class SortBy : uint8_t; }
namespace filter { enum class SortOrder : uint8_t; }


// New: Custom exception for export errors
class ExportException : public std::runtime_error {
public:
    explicit ExportException(const std::string& message) : std::runtime_error(message) {}
};

// New: Struct to define a mapping from a LogEntryField to an exported column header
struct ExportFieldMapping {
    std::variant<LogEntryField, std::string> field = LogEntryField::UNKNOWN; // The field from LogEntry to export
    std::string customHeader; // Optional: custom header name for the exported field
    std::optional<std::string> datetimeFormat; // Optional: format string for datetime fields

    ExportFieldMapping() = default;
    ExportFieldMapping(LogEntryField f, std::string header = "", std::optional<std::string> dtFormat = std::nullopt)
        : field(f), customHeader(std::move(header)), datetimeFormat(std::move(dtFormat)) {}
    ExportFieldMapping(std::string f, std::string header = "", std::optional<std::string> dtFormat = std::nullopt)
        : field(std::move(f)), customHeader(std::move(header)), datetimeFormat(std::move(dtFormat)) {}
};

// JSON conversion for ExportFieldMapping
void to_json(nlohmann::json& j, const ExportFieldMapping& efm);
void from_json(const nlohmann::json& j, ExportFieldMapping& efm);

struct ExportSettings {
    std::optional<std::string> outputPath; 
    std::optional<ExportFormat> format;
    std::vector<ExportFieldMapping> fieldsToExport; // If empty, export all available fields (or specific logic)
    std::optional<bool> includeHeader; 
    std::optional<int> jsonIndent; 
    std::optional<char> separator; 
    std::optional<std::string> textFormatString; 
    std::optional<bool> useAnsiColors; 
    
    // Missing fields identified from tests
    std::optional<filter::SortBy> sortBy;
    std::optional<filter::SortOrder> sortOrder;
    std::optional<bool> outputNoColor;
    std::optional<std::string> textOutputFormat;
    std::optional<bool> includeSummary;
    std::optional<bool> prettyPrint;
    std::optional<char> csvSeparator;
    std::vector<std::pair<std::string, std::string>> csvFields;
    std::vector<std::pair<std::string, std::string>> jsonFields;
    std::optional<int> topMessagesCount;
    std::optional<bool> streamMode;
    std::optional<bool> tailMode;
    std::optional<std::chrono::milliseconds> tailInterval;

    // Constructor to provide sane defaults for common use cases.
    ExportSettings() = default; 

    // Merge another settings object into this one
    void merge(const ExportSettings& other);
};

// --- JSON Conversion for ExportSettings ---
void to_json(nlohmann::json& j, const ExportSettings& es);
void from_json(const nlohmann::json& j, ExportSettings& es);

// Forward declaration of LogAnalyzer to avoid circular dependency if needed for utility methods
class LogAnalyzer; 

class Exporter {
public:
    // New unified export method that takes ExportSettings
    void exportLogEntries(
        std::ostream& os, 
        const std::vector<LogEntry>& entries, 
        const ExportSettings& settings);

private:
    // Exports filtered log entries as JSON
    void exportAsJson(
        std::ostream& os, 
        const std::vector<LogEntry>& entries, 
        const ExportSettings& settings);

    // Exports filtered log entries as CSV
    void exportAsCsv(
        std::ostream& os, 
        const std::vector<LogEntry>& entries, 
        const ExportSettings& settings);

    // Placeholder for text export (can be enhanced later)
    void exportAsText(
        std::ostream& os, 
        const std::vector<LogEntry>& entries, 
        const ExportSettings& settings);

    // New method for XML export
    void exportAsXml(
        std::ostream& os,
        const std::vector<LogEntry>& entries,
        const ExportSettings& settings);

    // New method for NDJSON export
    void exportAsNdjson(
        std::ostream& os,
        const std::vector<LogEntry>& entries,
        const ExportSettings& settings);

        // Helper to format a single log entry for text output
        std::string formatEntryForText(
            const LogEntry& entry, 
            const std::string& formatString, 
            bool useColors);
    
        // Helper to determine the effective fields to export, including discovery of custom fields
        std::vector<ExportFieldMapping> getEffectiveExportFieldMappings(
            const std::vector<LogEntry>& entries, 
            const ExportSettings& settings);
    
        // Helper to format a single CSV field, including quoting and escaping
        std::string formatCsvField(const std::string& value, char separator);

        // Single source of truth for the scalar standard-field -> string mapping shared
        // by the CSV and XML exporters. Returns the string value for the nine scalar
        // LogEntryField values (STRUCTURED_FIELD yields the raw structuredData string;
        // XML expands that itself and so does not route STRUCTURED_FIELD through here).
        static std::string standardFieldValue(
            const LogEntry& entry,
            LogEntryField field,
            const std::optional<std::string>& datetimeFormat);
    
        // Static constexpr string_views for text format placeholders
        static constexpr std::string_view PLACEHOLDER_LEVEL = "{level}";
        static constexpr std::string_view PLACEHOLDER_ID = "{id}";
        static constexpr std::string_view PLACEHOLDER_TIMESTAMP = "{timestamp}";
        static constexpr std::string_view PLACEHOLDER_MESSAGE = "{message}";
        static constexpr std::string_view PLACEHOLDER_SOURCE_FILE = "{sourceFile}";
        static constexpr std::string_view PLACEHOLDER_LINE_NUMBER = "{lineNumber}";
        static constexpr std::string_view PLACEHOLDER_THREAD_ID = "{threadId}";
        static constexpr std::string_view PLACEHOLDER_MODULE = "{module}";
        static constexpr std::string_view PLACEHOLDER_HOST = "{host}";
        static constexpr std::string_view PLACEHOLDER_CUSTOM_FIELDS = "{customFields}";
        static constexpr std::string_view PLACEHOLDER_CUSTOM_PREFIX = "{custom.";
    };
