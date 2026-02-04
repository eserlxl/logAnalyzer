#ifndef EXPORTER_H
#define EXPORTER_H

#include "LogTypes.h"
#include "Filter.h"
#include <iostream>
#include <vector>
#include <string>
#include <utility> // For std::move

enum class ExportFormat {
    PLAINTEXT,  // Raw log lines, possibly formatted
    CSV,        // Comma Separated Values
    JSON,       // JSON array of objects
    XML         // XML structure
    // Potentially more, e.g., HTML, custom templates
};

// Specifies which fields to include in the export and their order.
struct ExportField {
    LogEntryField field;
    std::string customHeader; // Optional custom header for CSV/table output

    ExportField(LogEntryField f, std::string header = "") : field(f), customHeader(std::move(header)) {}
};

struct ExportSettings {
    std::string outputPath = "output.log"; // Default output file
    ExportFormat format = ExportFormat::PLAINTEXT;
    std::vector<ExportField> fieldsToExport; // If empty, export all available fields
    bool includeHeader = true; // For CSV/table formats
    // Add more options as needed, e.g., compression, encoding

    // Constructor to provide sane defaults for common use cases.
    ExportSettings() {
        // Default fields for plaintext/csv export if none specified
        fieldsToExport.emplace_back(LogEntryField::TIMESTAMP, "Timestamp");
        fieldsToExport.emplace_back(LogEntryField::LEVEL, "Level");
        fieldsToExport.emplace_back(LogEntryField::MESSAGE, "Message");
    }
};

// Forward declaration of LogAnalyzer to avoid circular dependency if needed for utility methods
class LogAnalyzer; 

class Exporter {
public:
    // Exports filtered log entries as JSON
    void exportAsJson(
        std::ostream& os, 
        const std::vector<LogEntry>& entries, 
        bool prettyPrint);

    // Exports filtered log entries as CSV
    void exportAsCsv(
        std::ostream& os, 
        const std::vector<LogEntry>& entries, 
        char separator);

    // Placeholder for text export (can be enhanced later)
    void exportAsText(
        std::ostream& os, 
        const std::vector<LogEntry>& entries, 
        const std::string& formatString,
        bool useColors);

private:
    // Helper to format a single log entry for text output
    std::string formatEntryForText(
        const LogEntry& entry, 
        const std::string& formatString, 
        bool useColors);
};

#endif // EXPORTER_H
