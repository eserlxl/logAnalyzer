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
        efm.field = Utils::stringToLogEntryField(j.at("field").get<std::string>());
        // Note: stringToLogEntryField assumes standard fields. Custom fields would need special handling here.
        if (efm.field == LogEntryField::UNKNOWN && j.at("field").get<std::string>() != "UNKNOWN") {
             errors.push_back("ExportFieldMapping has an unrecognized field: " + j.at("field").get<std::string>());
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
    // Add more options as needed, e.g., compression, encoding

    // Constructor to provide sane defaults for common use cases.
    ExportSettings() {
        // Default fields for plaintext/csv export if none specified
        fieldsToExport.emplace_back(LogEntryField::TIMESTAMP, "Timestamp");
        fieldsToExport.emplace_back(LogEntryField::LEVEL, "Level");
        fieldsToExport.emplace_back(LogEntryField::MESSAGE, "Message");
    }
};

// --- JSON Conversion for ExportSettings ---
inline void to_json(nlohmann::json& j, const ExportSettings& es) {
    j = nlohmann::json{
        {"outputPath", es.outputPath},
        {"format", Utils::exportFormatToString(es.format)},
        {"fieldsToExport", es.fieldsToExport}, // Uses ExportFieldMapping to_json
        {"includeHeader", es.includeHeader}
    };
}

inline void from_json(const nlohmann::json& j, ExportSettings& es) {
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
            // As per existing logic, if present but invalid type, default to true
            es.includeHeader = true; 
        }
    }
}

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
