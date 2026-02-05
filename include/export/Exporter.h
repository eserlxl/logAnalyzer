#ifndef EXPORTER_H
#define EXPORTER_H

#include <nlohmann/json.hpp>
#include "core/LogTypes.h"
#include "utils/UtilsCore.h"
#include <iostream>
#include <vector>
#include <string>
#include <utility>
#include <optional>
#include <stdexcept>

// New: Enum for different export formats
enum class ExportFormat {
    PLAINTEXT,
    JSON,
    CSV,
    XML,
    UNKNOWN // Default for unrecognized formats
};



// New: Custom exception for export errors
class ExportException : public std::runtime_error {
public:
    explicit ExportException(const std::string& message) : std::runtime_error(message) {}
};

// New: Struct to define a mapping from a LogEntryField to an exported column header
struct ExportFieldMapping {
    LogEntryField field = LogEntryField::UNKNOWN; // The field from LogEntry to export
    std::string customHeader; // Optional: custom header name for the exported field
    std::optional<std::string> datetimeFormat; // Optional: format string for datetime fields

    ExportFieldMapping() = default;
    ExportFieldMapping(LogEntryField f, std::string header = "", std::optional<std::string> dtFormat = std::nullopt)
        : field(f), customHeader(std::move(header)), datetimeFormat(std::move(dtFormat)) {}
};

// JSON conversion for ExportFieldMapping
void to_json(nlohmann::json& j, const ExportFieldMapping& efm);
void from_json(const nlohmann::json& j, ExportFieldMapping& efm);

struct ExportSettings {
    std::string outputPath = "output.log"; // Default output file
    ExportFormat format = ExportFormat::PLAINTEXT;
    std::vector<ExportFieldMapping> fieldsToExport; // If empty, export all available fields
    bool includeHeader = true; // For CSV/table formats
    std::optional<int> jsonIndent; // For JSON pretty printing (e.g., 4 for 4 spaces)
    char separator = ','; // For CSV files
    std::string textFormatString = "{timestamp} [{level}] {message}"; // For PLAINTEXT format
    bool useAnsiColors = false; // For PLAINTEXT format
    
    // Missing fields identified from tests
    std::optional<SortBy> sortBy;
    std::optional<SortOrder> sortOrder;
    bool outputNoColor = false;
    std::string textOutputFormat = "{timestamp} {level}: {message}";
    bool includeSummary = false;
    bool prettyPrint = false;
    char csvSeparator = ',';
    std::vector<std::pair<std::string, std::string>> csvFields;
    std::vector<std::pair<std::string, std::string>> jsonFields;
    int topMessagesCount = 10;
    bool streamMode = false;
    bool tailMode = false;
    std::chrono::milliseconds tailInterval = std::chrono::milliseconds(1000);

    // Constructor to provide sane defaults for common use cases.
    ExportSettings() = default; // Leave fieldsToExport empty to signal "export all standard fields"
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
    
        // Static constexpr string_views for text format placeholders
        static constexpr std::string_view PLACEHOLDER_LEVEL = "{level}";
        static constexpr std::string_view PLACEHOLDER_ID = "{id}";
        static constexpr std::string_view PLACEHOLDER_TIMESTAMP = "{timestamp}";
        static constexpr std::string_view PLACEHOLDER_MESSAGE = "{message}";
        static constexpr std::string_view PLACEHOLDER_CUSTOM_PREFIX = "{custom.";
    };
#endif // EXPORTER_H
