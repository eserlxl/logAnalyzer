#ifndef EXPORTER_H
#define EXPORTER_H

#include <nlohmann/json.hpp>
#include "core/LogTypes.h"
#include "utils/Utils.h"
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
inline void to_json(nlohmann::json& j, const ExportFieldMapping& efm) {
    j = nlohmann::json{
        {"field", Utils::logEntryFieldToString(efm.field)},
        {"customHeader", efm.customHeader}
    };
    if (efm.datetimeFormat) {
        j["datetimeFormat"] = *efm.datetimeFormat;
    }
}

inline void from_json(const nlohmann::json& j, ExportFieldMapping& efm) {
    std::vector<std::string> errors;

    if (j.contains("field") && j.at("field").is_string()) {
        std::string fieldStr = j.at("field").get<std::string>();
        efm.field = Utils::stringToLogEntryField(fieldStr);
        if (efm.field == LogEntryField::UNKNOWN && fieldStr != "UNKNOWN") {
             errors.push_back("ExportFieldMapping has an unrecognized field: " + fieldStr);
        }
    } else {
        errors.push_back("ExportFieldMapping is missing or has invalid 'field'.");
    }

    if (j.contains("customHeader") && j.at("customHeader").is_string()) {
        efm.customHeader = j.at("customHeader").get<std::string>();
    } // customHeader is optional

    if (j.contains("datetimeFormat") && j.at("datetimeFormat").is_string()) {
        efm.datetimeFormat = j.at("datetimeFormat").get<std::string>();
    } // datetimeFormat is optional

    if (!errors.empty()) {
        throw std::runtime_error(errors[0]);
    }
}

struct ExportSettings {
    std::string outputPath = "output.log"; // Default output file
    ExportFormat format = ExportFormat::PLAINTEXT;
    std::vector<ExportFieldMapping> fieldsToExport; // If empty, export all available fields
    bool includeHeader = true; // For CSV/table formats
    std::optional<int> jsonIndent; // For JSON pretty printing (e.g., 4 for 4 spaces)
    char separator = ','; // For CSV files
    std::string textFormatString = "{timestamp} [{level}] {message}"; // For PLAINTEXT format
    bool useAnsiColors = false; // For PLAINTEXT format
    // Add more options as needed, e.g., compression, encoding

    // Constructor to provide sane defaults for common use cases.
    ExportSettings() = default; // Leave fieldsToExport empty to signal "export all standard fields"
};

// --- JSON Conversion for ExportSettings ---
inline void to_json(nlohmann::json& j, const ExportSettings& es) {
    j = nlohmann::json{
        {"outputPath", es.outputPath},
        {"format", Utils::exportFormatToString(es.format)},
        {"fieldsToExport", es.fieldsToExport}, // Uses ExportFieldMapping to_json
        {"includeHeader", es.includeHeader},
        {"separator", std::string(1, es.separator)},
        {"textFormatString", es.textFormatString},
        {"useAnsiColors", es.useAnsiColors}
    };
    if (es.jsonIndent) {
        j["jsonIndent"] = *es.jsonIndent;
    }
}

inline void from_json(const nlohmann::json& j, ExportSettings& es) {
    // Default construct ensures fieldsToExport is empty by default, indicating "all standard fields"
    // if no specific fieldsToExport are provided in JSON.
    es = ExportSettings(); 

    if (j.contains("outputPath")) {
        if (j.at("outputPath").is_string()) {
            es.outputPath = j.at("outputPath").get<std::string>();
        } else {
            throw std::runtime_error("ExportSettings: 'outputPath' has invalid type. Expected string.");
        }
    }

    if (j.contains("format")) {
        if (j.at("format").is_string()) {
            std::string formatStr = j.at("format").get<std::string>();
            auto formatOpt = Utils::stringToExportFormat(formatStr);
            if (formatOpt) {
                es.format = *formatOpt;
            } else {
                 // If the string is not recognized, throw an error, as format is a critical setting.
                 throw std::runtime_error("ExportSettings: 'format' has invalid value: " + formatStr);
            }
        } else {
            throw std::runtime_error("ExportSettings: 'format' has invalid type. Expected string.");
        }
    }

    if (j.contains("fieldsToExport")) {
        if (j.at("fieldsToExport").is_array()) {
            es.fieldsToExport = j.at("fieldsToExport").get<std::vector<ExportFieldMapping>>();
        } else {
            throw std::runtime_error("ExportSettings: 'fieldsToExport' has invalid type. Expected array.");
        }
    }

    if (j.contains("includeHeader")) {
        if (j.at("includeHeader").is_boolean()) {
            es.includeHeader = j.at("includeHeader").get<bool>();
        } else {
            throw std::runtime_error("ExportSettings: 'includeHeader' has invalid type. Expected boolean."); 
        }
    }

    if (j.contains("jsonIndent")) {
        if (j.at("jsonIndent").is_number_integer()) {
            es.jsonIndent = j.at("jsonIndent").get<int>();
        } else {
            throw std::runtime_error("ExportSettings: 'jsonIndent' has invalid type. Expected integer.");
        }
    }

    if (j.contains("separator")) {
        if (j.at("separator").is_string() && j.at("separator").get<std::string>().length() == 1) {
            es.separator = j.at("separator").get<std::string>().at(0);
        } else {
            throw std::runtime_error("ExportSettings: 'separator' has invalid type or length. Expected a single character string.");
        }
    }

    if (j.contains("textFormatString")) {
        if (j.at("textFormatString").is_string()) {
            es.textFormatString = j.at("textFormatString").get<std::string>();
        } else {
            throw std::runtime_error("ExportSettings: 'textFormatString' has invalid type. Expected string.");
        }
    }

    if (j.contains("useAnsiColors")) {
        if (j.at("useAnsiColors").is_boolean()) {
            es.useAnsiColors = j.at("useAnsiColors").get<bool>();
        } else {
            throw std::runtime_error("ExportSettings: 'useAnsiColors' has invalid type. Expected boolean.");
        }
    }
}

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

    // Helper to format a single log entry for text output
    std::string formatEntryForText(
        const LogEntry& entry, 
        const std::string& formatString, 
        bool useColors);
};

#endif // EXPORTER_H
