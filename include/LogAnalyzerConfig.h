#ifndef LOG_ANALYZER_CONFIG_H
#define LOG_ANALYZER_CONFIG_H

#include "LogTypes.h" // Assuming LogTypes.h defines LogLevel, FieldMapping, and ci_less
#include "Filter.h"   // For FilterRule
#include "Exporter.h" // For ExportSettings
#include "Statistics.h" // For StatisticConfig

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <string_view>
#include <functional> // Required for std::function
#include <utility>    // Required for std::move
#include <expected>   // For std::expected (C++23)
#include <fstream>    // For file operations (fromFile)
// Assuming nlohmann/json library is available for JSON parsing/serialization
// If not, this will require a different JSON library or manual parsing.
#include <nlohmann/json.hpp> 

// Defined as in LogAnalyzer.h comment
static constexpr std::string_view DEFAULT_LOG_REGEX_PATTERN_SV = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) ([A-Z]+): (.*)$)";
static const std::string DEFAULT_LOG_REGEX_PATTERN = std::string(DEFAULT_LOG_REGEX_PATTERN_SV);

// Helper functions for JSON serialization/deserialization
namespace { // Anonymous namespace for internal linkage

// Helper to convert LogLevel enum to string
std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        case LogLevel::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

// Helper to convert string to LogLevel enum
LogLevel stringToLogLevel(const std::string& levelStr) {
    if (levelStr == "TRACE") return LogLevel::TRACE;
    if (levelStr == "DEBUG") return LogLevel::DEBUG;
    if (levelStr == "INFO") return LogLevel::INFO;
    if (levelStr == "WARNING") return LogLevel::WARNING;
    if (levelStr == "ERROR") return LogLevel::ERROR;
    if (levelStr == "CRITICAL") return LogLevel::CRITICAL;
    return LogLevel::UNKNOWN;
}

// Helper to convert FilterOperator enum to string
std::string filterOperatorToString(FilterOperator op) {
    switch (op) {
        case FilterOperator::EQUALS: return "EQUALS";
        case FilterOperator::NOT_EQUALS: return "NOT_EQUALS";
        case FilterOperator::CONTAINS: return "CONTAINS";
        case FilterOperator::DOES_NOT_CONTAIN: return "DOES_NOT_CONTAIN";
        case FilterOperator::STARTS_WITH: return "STARTS_WITH";
        case FilterOperator::ENDS_WITH: return "ENDS_WITH";
        case FilterOperator::GREATER_THAN: return "GREATER_THAN";
        case FilterOperator::LESS_THAN: return "LESS_THAN";
        case FilterOperator::GREATER_THAN_OR_EQUAL: return "GREATER_THAN_OR_EQUAL";
        case FilterOperator::LESS_THAN_OR_EQUAL: return "LESS_THAN_OR_EQUAL";
        default: return "UNKNOWN";
    }
}

// Helper to convert string to FilterOperator enum
FilterOperator stringToFilterOperator(const std::string& opStr) {
    if (opStr == "EQUALS") return FilterOperator::EQUALS;
    if (opStr == "NOT_EQUALS") return FilterOperator::NOT_EQUALS;
    if (opStr == "CONTAINS") return FilterOperator::CONTAINS;
    if (opStr == "NOT_CONTAINS") return FilterOperator::NOT_CONTAINS;
    if (opStr == "STARTS_WITH") return FilterOperator::STARTS_WITH;
    if (opStr == "ENDS_WITH") return FilterOperator::ENDS_WITH;
    if (opStr == "GREATER_THAN") return FilterOperator::GREATER_THAN;
    if (opStr == "LESS_THAN") return FilterOperator::LESS_THAN;
    if (opStr == "GREATER_THAN_OR_EQUAL") return FilterOperator::GREATER_THAN_OR_EQUAL;
    if (opStr == "LESS_THAN_OR_EQUAL") return FilterOperator::LESS_THAN_OR_EQUAL;
    return FilterOperator::UNKNOWN; // Or throw an exception if unknown operator is critical
}

// Helper to convert ExportFormat enum to string
std::string exportFormatToString(ExportFormat format) {
    switch (format) {
        case ExportFormat::PLAINTEXT: return "PLAINTEXT";
        case ExportFormat::JSON: return "JSON";
        case ExportFormat::CSV: return "CSV"; // New
        case ExportFormat::XML: return "XML";  // New
        default: return "UNKNOWN";
    }
}

// Helper to convert string to ExportFormat enum
ExportFormat stringToExportFormat(const std::string& formatStr) {
    if (formatStr == "PLAINTEXT") return ExportFormat::PLAINTEXT;
    if (formatStr == "JSON") return ExportFormat::JSON;
    if (formatStr == "CSV") return ExportFormat::CSV; // New
    if (formatStr == "XML") return ExportFormat::XML;  // New
    return ExportFormat::UNKNOWN; // Or throw
}

// Helper to convert StatisticType enum to string
std::string statisticTypeToString(StatisticType type) {
    switch (type) {
        case StatisticType::COUNT_BY_LEVEL: return "COUNT_BY_LEVEL";
        case StatisticType::TOP_N_OCCURRENCES: return "TOP_N_OCCURRENCES";
        case StatisticType::OCCURRENCE_COUNT: return "OCCURRENCE_COUNT";
        case StatisticType::SUM: return "SUM"; // New
        case StatisticType::AVERAGE: return "AVERAGE"; // New
        case StatisticType::MIN: return "MIN"; // New
        case StatisticType::MAX: return "MAX"; // New
        default: return "UNKNOWN";
    }
}

// Helper to convert string to StatisticType enum
StatisticType stringToStatisticType(const std::string& typeStr) {
    if (typeStr == "COUNT_BY_LEVEL") return StatisticType::COUNT_BY_LEVEL;
    if (typeStr == "TOP_N_OCCURRENCES") return StatisticType::TOP_N_OCCURRENCES;
    if (typeStr == "OCCURRENCE_COUNT") return StatisticType::OCCURRENCE_COUNT;
    if (typeStr == "SUM") return StatisticType::SUM; // New
    if (typeStr == "AVERAGE") return StatisticType::AVERAGE; // New
    if (typeStr == "MIN") return StatisticType::MIN; // New
    if (typeStr == "MAX") return StatisticType::MAX; // New
    return StatisticType::UNKNOWN; // Or throw
}

// Helper to convert StatisticOutputFormat enum to string
std::string statisticOutputFormatToString(StatisticOutputFormat format) {
    switch (format) {
        case StatisticOutputFormat::PLAINTEXT_TABLE: return "PLAINTEXT_TABLE";
        case StatisticOutputFormat::JSON: return "JSON";
        case StatisticOutputFormat::CSV: return "CSV";
        default: return "UNKNOWN";
    }
}

// Helper to convert string to StatisticOutputFormat enum
StatisticOutputFormat stringToStatisticOutputFormat(const std::string& formatStr) {
    if (formatStr == "PLAINTEXT_TABLE") return StatisticOutputFormat::PLAINTEXT_TABLE;
    if (formatStr == "JSON") return StatisticOutputFormat::JSON;
    if (formatStr == "CSV") return StatisticOutputFormat::CSV;
    return StatisticOutputFormat::UNKNOWN; // Or throw
}


// Helper to convert LogEntryField enum to string
std::string logEntryFieldToString(LogEntryField field) {
    switch (field) {
        case LogEntryField::TIMESTAMP: return "TIMESTAMP";
        case LogEntryField::LEVEL: return "LEVEL";
        case LogEntryField::MESSAGE: return "MESSAGE";
        case LogEntryField::SOURCE_FILE: return "SOURCE_FILE";
        case LogEntryField::LINE_NUMBER: return "LINE_NUMBER";
        case LogEntryField::THREAD_ID: return "THREAD_ID";
        case LogEntryField::MODULE: return "MODULE";
        case LogEntryField::HOST: return "HOST";
        case LogEntryField::CUSTOM: return "CUSTOM"; // If we had a way to identify custom fields directly by enum
        default: return "UNKNOWN";
    }
}

// Helper to convert string to LogEntryField enum. 
// NOTE: This function assumes standard LogEntryFields. Custom fields will be handled differently.
LogEntryField stringToLogEntryField(const std::string& fieldStr) {
    if (fieldStr == "TIMESTAMP") return LogEntryField::TIMESTAMP;
    if (fieldStr == "LEVEL") return LogEntryField::LEVEL;
    if (fieldStr == "MESSAGE") return LogEntryField::MESSAGE;
    if (fieldStr == "SOURCE_FILE") return LogEntryField::SOURCE_FILE;
    if (fieldStr == "LINE_NUMBER") return LogEntryField::LINE_NUMBER;
    if (fieldStr == "THREAD_ID") return LogEntryField::THREAD_ID;
    if (fieldStr == "MODULE") return LogEntryField::MODULE;
    if (fieldStr == "HOST") return LogEntryField::HOST;
    // For custom fields, the string itself is used as the identifier.
    // This function should return a placeholder or handle this case appropriately
    // when called within a context that knows about custom fields.
    // For now, returning UNKNOWN if it's not a standard field.
    return LogEntryField::UNKNOWN; 
}


    // --- JSON Conversion for FieldMapping ---
inline void to_json(nlohmann::json& j, const FieldMapping& fm) {
    j = nlohmann::json{
        {"groupIndex", fm.groupIndex},
        {"formats", fm.formats}
    };
    // Handle the variant for field_identifier
    if (std::holds_alternative<LogEntryField>(fm.field_identifier)) {
        j["field"] = Utils::logEntryFieldToString(std::get<LogEntryField>(fm.field_identifier));
    } else if (std::holds_alternative<std::string>(fm.field_identifier)) {
        j["field"] = std::get<std::string>(fm.field_identifier);
        if (fm.customFieldType) {
            j["customFieldType"] = *fm.customFieldType;
        }
    }
}

inline void from_json(const nlohmann::json& j, FieldMapping& fm) {
    std::vector<std::string> errors;
    
    // Required fields
    if (j.contains("groupIndex") && j.at("groupIndex").is_number_integer()) {
        fm.groupIndex = j.at("groupIndex").get<int>();
    } else {
        errors.push_back("FieldMapping is missing or has invalid 'groupIndex'.");
    }

    if (j.contains("formats") && j.at("formats").is_array()) {
        fm.formats = j.at("formats").get<std::vector<std::string>>();
    } // 'formats' is optional, so no error if missing

    // Field identifier (enum or string)
    if (j.contains("field")) {
        if (j.at("field").is_string()) {
            std::string fieldStr = j.at("field").get<std::string>();
            // Try to convert to standard LogEntryField first
            LogEntryField standardField = Utils::stringToLogEntryField(fieldStr);
            if (standardField != LogEntryField::UNKNOWN) {
                fm.field_identifier = standardField;
            } else {
                // It's not a standard field, assume it's a custom field name
                fm.field_identifier = fieldStr;
                // Check for customFieldType if it's a custom field
                if (j.contains("customFieldType") && j.at("customFieldType").is_string()) {
                    fm.customFieldType = j.at("customFieldType").get<std::string>();
                } else {
                    // If it's a custom field but no type is provided, it might be an issue depending on requirements.
                    // For now, we'll allow it but might add a validation check later.
                }
            }
        } else {
            errors.push_back("FieldMapping 'field' must be a string.");
        }
    } else {
        errors.push_back("FieldMapping is missing the required 'field' key.");
    }

    if (!errors.empty()) {
        throw nlohmann::json::exception(errors.size(), errors[0].c_str()); // Basic exception for now
    }
}

// --- JSON Conversion for FilterRule ---
inline void to_json(nlohmann::json& j, const FilterRule& fr) {
    j = nlohmann::json{
        {"field", Utils::logEntryFieldToString(fr.field)},
        {"op", Utils::filterOperatorToString(fr.op)},
        {"value", fr.value},
        {"caseSensitive", fr.caseSensitive}
    };
}

inline void from_json(const nlohmann::json& j, FilterRule& fr) {
    std::vector<std::string> errors;

    if (j.contains("field") && j.at("field").is_string()) {
        fr.field = Utils::stringToLogEntryField(j.at("field").get<std::string>());
        // Note: If the field is not a standard LogEntryField and not handled by stringToLogEntryField,
        // it might be intended as a custom field. This would require more sophisticated handling
        // if custom fields are meant to be directly specified in filterRules JSON.
        // For now, assuming filterRules only refer to standard LogEntryFields.
    } else {
        errors.push_back("FilterRule is missing or has invalid 'field'.");
    }

    if (j.contains("op") && j.at("op").is_string()) {
        fr.op = Utils::stringToFilterOperator(j.at("op").get<std::string>());
    } else {
        errors.push_back("FilterRule is missing or has invalid 'op'.");
    }

    if (j.contains("value") && j.at("value").is_string()) {
        fr.value = j.at("value").get<std::string>();
    } else {
        errors.push_back("FilterRule is missing or has invalid 'value'.");
    }

    if (j.contains("caseSensitive") && j.at("caseSensitive").is_boolean()) {
        fr.caseSensitive = j.at("caseSensitive").get<bool>();
    } else {
        // Default to false if not specified or invalid
        fr.caseSensitive = false; 
    }

    if (!errors.empty()) {
        throw nlohmann::json::exception(errors.size(), errors[0].c_str());
    }
}

// --- JSON Conversion for ExportFieldMapping ---
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
        // Similar note as FilterRule: assumes standard fields for now.
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
        throw nlohmann::json::exception(errors.size(), errors[0].c_str());
    }
}

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
    std::vector<std::string> errors;

    if (j.contains("outputPath") && j.at("outputPath").is_string()) {
        es.outputPath = j.at("outputPath").get<std::string>();
    } else {
        errors.push_back("ExportSettings is missing or has invalid 'outputPath'.");
    }

    if (j.contains("format") && j.at("format").is_string()) {
        es.format = Utils::stringToExportFormat(j.at("format").get<std::string>());
    }

    if (j.contains("fieldsToExport") && j.at("fieldsToExport").is_array()) {
        es.fieldsToExport = j.at("fieldsToExport").get<std::vector<ExportFieldMapping>>();
    } else {
        errors.push_back("ExportSettings is missing or has invalid 'fieldsToExport'.");
    }

    if (j.contains("includeHeader") && j.at("includeHeader").is_boolean()) {
        es.includeHeader = j.at("includeHeader").get<bool>();
    } else {
        // Default to true if not specified or invalid
        es.includeHeader = true;
    }

    if (!errors.empty()) {
        throw nlohmann::json::exception(errors.size(), errors[0].c_str());
    }
}

// --- JSON Conversion for StatisticConfig ---
inline void to_json(nlohmann::json& j, const StatisticConfig& sc) {
    j = nlohmann::json{
        {"type", Utils::statisticTypeToString(sc.type)}
    };
    if (sc.field) {
        j["field"] = Utils::logEntryFieldToString(*sc.field);
    }
    if (sc.pattern) {
        j["pattern"] = *sc.pattern;
    }
    if (sc.topN) {
        j["topN"] = *sc.topN;
    }
    if (sc.groupByField) {
        j["groupByField"] = Utils::logEntryFieldToString(*sc.groupByField);
    }
}

inline void from_json(const nlohmann::json& j, StatisticConfig& sc) {
    std::vector<std::string> errors;

    if (j.contains("type") && j.at("type").is_string()) {
        sc.type = Utils::stringToStatisticType(j.at("type").get<std::string>());
    } else {
        errors.push_back("StatisticConfig is missing or has invalid 'type'.");
    }

    if (j.contains("field") && j.at("field").is_string()) {
        std::string fieldStr = j.at("field").get<std::string>();
        // IMPORTANT: stringToLogEntryField only handles standard fields.
        // If custom fields can be used here, this needs adjustment.
        sc.field = Utils::stringToLogEntryField(fieldStr); 
        if (sc.field == LogEntryField::UNKNOWN && fieldStr != "UNKNOWN") {
             errors.push_back("StatisticConfig has an unrecognized 'field' string: " + fieldStr);
        }
    } // field is optional

    if (j.contains("pattern") && j.at("pattern").is_string()) {
        sc.pattern = j.at("pattern").get<std::string>();
    } // pattern is optional

    if (j.contains("topN") && j.at("topN").is_number_integer()) {
        sc.topN = j.at("topN").get<int>();
    } // topN is optional

    if (j.contains("groupByField") && j.at("groupByField").is_string()) {
        std::string groupByFieldStr = j.at("groupByField").get<std::string>();
        sc.groupByField = Utils::stringToLogEntryField(groupByFieldStr);
        if (sc.groupByField == LogEntryField::UNKNOWN && groupByFieldStr != "UNKNOWN") {
             errors.push_back("StatisticConfig has an unrecognized 'groupByField' string: " + groupByFieldStr);
        }
    } // groupByField is optional

    if (!errors.empty()) {
        throw nlohmann::json::exception(errors.size(), errors[0].c_str());
    }
}

// --- JSON Conversion for FilterLogicalOperator ---
// Note: FilterLogicalOperator is an enum, not a struct, so it doesn't need to_json/from_json helpers directly
// unless it were complex. We'll handle its string conversion within FilterExpression's JSON logic.

// Helper to convert FilterLogicalOperator to string
inline std::string filterLogicalOperatorToString(FilterLogicalOperator op) {
    switch (op) {
        case FilterLogicalOperator::AND: return "AND";
        case FilterLogicalOperator::OR: return "OR";
        case FilterLogicalOperator::NOT: return "NOT";
        default: return "UNKNOWN";
    }
}

// Helper to convert string to FilterLogicalOperator
inline FilterLogicalOperator stringToFilterLogicalOperator(const std::string& opStr) {
    if (opStr == "AND") return FilterLogicalOperator::AND;
    if (opStr == "OR") return FilterLogicalOperator::OR;
    if (opStr == "NOT") return FilterLogicalOperator::NOT;
    return FilterLogicalOperator::UNKNOWN;
}

// Forward declarations for recursive JSON conversion
inline void to_json(nlohmann::json& j, const FilterExpression& fe);
inline void from_json(const nlohmann::json& j, FilterExpression& fe);

// Helper to convert FilterCondition to JSON
inline void to_json(nlohmann::json& j, const FilterCondition& fc) {
    j = nlohmann::json{
        {"field", Utils::logEntryFieldToString(fc.field)},
        {"op", Utils::filterOperatorToString(fc.op)},
        {"value", fc.value},
        {"value_type", static_cast<int>(fc.valueType)}, // Cast to int for enum
        {"caseSensitive", fc.caseSensitive}
    };
    if (fc.datetimeFormat) {
        j["datetimeFormat"] = *fc.datetimeFormat;
    }
}

// Helper to convert JSON to FilterCondition
inline void from_json(const nlohmann::json& j, FilterCondition& fc) {
    std::vector<std::string> errors;

    if (j.contains("field") && j.at("field").is_string()) {
        fc.field = Utils::stringToLogEntryField(j.at("field").get<std::string>());
        if (fc.field == LogEntryField::UNKNOWN && j.at("field").get<std::string>() != "UNKNOWN") {
             errors.push_back("FilterCondition has an unrecognized 'field' string: " + j.at("field").get<std::string>());
        }
    } else {
        errors.push_back("FilterCondition is missing or has invalid 'field'.");
    }

    if (j.contains("op") && j.at("op").is_string()) {
        fc.op = Utils::stringToFilterOperator(j.at("op").get<std::string>());
    } else {
        errors.push_back("FilterCondition is missing or has invalid 'op'.");
    }

    if (j.contains("value") && j.at("value").is_string()) {
        fc.value = j.at("value").get<std::string>();
    } else {
        errors.push_back("FilterCondition is missing or has invalid 'value'.");
    }

    if (j.contains("value_type") && j.at("value_type").is_number_integer()) {
        fc.valueType = static_cast<FilterValueType>(j.at("value_type").get<int>());
    } else {
        errors.push_back("FilterCondition is missing or has invalid 'value_type'.");
    }

    if (j.contains("caseSensitive") && j.at("caseSensitive").is_boolean()) {
        fc.caseSensitive = j.at("caseSensitive").get<bool>();
    } else {
        fc.caseSensitive = false; // Default if not provided
    }

    if (j.contains("datetimeFormat") && j.at("datetimeFormat").is_string()) {
        fc.datetimeFormat = j.at("datetimeFormat").get<std::string>();
    }

    if (!errors.empty()) {
        throw nlohmann::json::exception(errors.size(), errors[0].c_str());
    }
}

// JSON conversion for FilterExpression (recursive)
inline void to_json(nlohmann::json& j, const FilterExpression& fe) {
    if (fe.getType() == FilterExpression::ExpressionType::CONDITION) {
        j["condition"] = *fe.getCondition();
    } else if (fe.getType() == FilterExpression::ExpressionType::LOGICAL) {
        j["operator"] = Utils::filterLogicalOperatorToString(*fe.getLogicalOperator());
        if (!fe.getOperands().empty()) {
            j["operands"] = nlohmann::json::array();
            for (const auto& operand : fe.getOperands()) {
                nlohmann::json operand_j;
                to_json(operand_j, operand); // Recursive call
                j["operands"].push_back(operand_j);
            }
        }
    }
}

inline void from_json(const nlohmann::json& j, FilterExpression& fe) {
    std::vector<std::string> errors;

    if (j.contains("condition") && j.at("condition").is_object()) {
        try {
            fe = FilterExpression(j.at("condition").get<FilterCondition>());
        } catch (const nlohmann::json::exception& e) {
            errors.push_back("Error parsing condition: " + std::string(e.what()));
        }
    } else if (j.contains("operator") && j.at("operator").is_string()) {
        FilterLogicalOperator op = Utils::stringToFilterLogicalOperator(j.at("operator").get<std::string>());
        if (op == FilterLogicalOperator::UNKNOWN) {
            errors.push_back("Unknown filter operator: " + j.at("operator").get<std::string>());
        } else {
            std::vector<FilterExpression> operands;
            if (j.contains("operands") && j.at("operands").is_array()) {
                for (const auto& operand_j : j.at("operands")) {
                    try {
                        FilterExpression operand_fe;
                        from_json(operand_j, operand_fe); // Recursive call
                        operands.push_back(operand_fe);
                    } catch (const nlohmann::json::exception& e) {
                        errors.push_back("Error parsing operand: " + std::string(e.what()));
                    }
                }
            } else {
                errors.push_back("FilterExpression with operator must contain 'operands' array.");
            }
            fe = FilterExpression(op, operands);
        }
    } else {
        errors.push_back("FilterExpression must contain either 'condition' or 'operator' with 'operands'.");
    }
    
    if (!errors.empty()) {
        throw nlohmann::json::exception(errors.size(), errors[0].c_str());
    }
}

// --- Implementations for LogAnalyzerSettings ---













} // end anonymous namespace

// --- Static Method Implementations ---

std::expected<LogAnalyzerSettings, std::vector<std::string>> LogAnalyzerSettings::fromJson(const std::string& jsonContent) {
    std::vector<std::string> errors;
    LogAnalyzerSettings settings;

    try {
        nlohmann::json j = nlohmann::json::parse(jsonContent);

        // Populate parsing configuration
        if (j.contains("lineParsePattern") && j.at("lineParsePattern").is_string()) {
            settings.lineParsePattern = j.at("lineParsePattern").get<std::string>();
        } else if (j.contains("lineParsePattern")) {
            errors.push_back("Invalid type for 'lineParsePattern'. Expected string.");
        }

        if (j.contains("fieldMappings") && j.at("fieldMappings").is_array()) {
            try {
                settings.fieldMappings = j.at("fieldMappings").get<std::vector<FieldMapping>>();
            } catch (const nlohmann::json::exception& e) {
                errors.push_back("Error parsing 'fieldMappings': " + std::string(e.what()));
            }
        } else if (j.contains("fieldMappings")) {
            errors.push_back("Invalid type for 'fieldMappings'. Expected array.");
        }
        
        if (j.contains("customLogLevelMappings") && j.at("customLogLevelMappings").is_object()) {
            settings.customLogLevelMappings.clear(); // Clear defaults
            for (auto const& [levelStr, levelVal] : j.at("customLogLevelMappings").items()) {
                if (levelVal.is_string()) {
                    settings.customLogLevelMappings[levelStr] = Utils::stringToLogLevel(levelVal.get<std::string>());
                } else {
                    errors.push_back("Invalid type for customLogLevelMapping value for key '" + levelStr + "'. Expected string.");
                }
            }
        } else if (j.contains("customLogLevelMappings")) {
            errors.push_back("Invalid type for 'customLogLevelMappings'. Expected object.");
        }

        if (j.contains("logEntryStartPattern") && j.at("logEntryStartPattern").is_string()) {
            settings.logEntryStartPattern = j.at("logEntryStartPattern").get<std::string>();
        } else if (j.contains("logEntryStartPattern") && !j.at("logEntryStartPattern").is_null()) {
            errors.push_back("Invalid type for 'logEntryStartPattern'. Expected string or null.");
        } else if (j.contains("logEntryStartPattern") && j.at("logEntryStartPattern").is_null()) {
            settings.logEntryStartPattern = std::nullopt; // Explicitly null means no pattern
        }

        if (j.contains("caseSensitiveParsing") && j.at("caseSensitiveParsing").is_boolean()) {
            settings.caseSensitiveParsing = j.at("caseSensitiveParsing").get<bool>();
        } else if (j.contains("caseSensitiveParsing")) {
            errors.push_back("Invalid type for 'caseSensitiveParsing'. Expected boolean.");
        }

        // Populate filtering configuration
        if (j.contains("filterRules") && j.at("filterRules").is_array()) {
            try {
                settings.filterRules = j.at("filterRules").get<std::vector<FilterRule>>();
            } catch (const nlohmann::json::exception& e) {
                errors.push_back("Error parsing 'filterRules': " + std::string(e.what()));
            }
        } else if (j.contains("filterRules")) {
            errors.push_back("Invalid type for 'filterRules'. Expected array.");
        }
        
        // Populate export configuration
        if (j.contains("exportSettings") && j.at("exportSettings").is_object()) {
            try {
                settings.exportSettings = j.at("exportSettings").get<ExportSettings>();
            } catch (const nlohmann::json::exception& e) {
                errors.push_back("Error parsing 'exportSettings': " + std::string(e.what()));
            }
        } else if (j.contains("exportSettings")) {
            errors.push_back("Invalid type for 'exportSettings'. Expected object.");
        }

        // Populate statistics configuration
        if (j.contains("statisticConfigs") && j.at("statisticConfigs").is_array()) {
            try {
                settings.statisticConfigs = j.at("statisticConfigs").get<std::vector<StatisticConfig>>();
            } catch (const nlohmann::json::exception& e) {
                errors.push_back("Error parsing 'statisticConfigs': " + std::string(e.what()));
            }
        } else if (j.contains("statisticConfigs")) {
            errors.push_back("Invalid type for 'statisticConfigs'. Expected array.");
        }

        // --- Handling the new FilterExpression (Advanced Filtering) ---
        // This part needs to be integrated once FilterExpression is fully implemented and its JSON conversion is finalized.
        // For now, we'll just check for the presence of 'rootFilterExpression' if it exists in JSON.
        if (j.contains("rootFilterExpression") && j.at("rootFilterExpression").is_object()) {
            try {
                settings.rootFilterExpression = j.at("rootFilterExpression").get<FilterExpression>();
            } catch (const nlohmann::json::exception& e) {
                errors.push_back("Error parsing 'rootFilterExpression': " + std::string(e.what()));
            }
        } else if (j.contains("rootFilterExpression")) {
             errors.push_back("Invalid type for 'rootFilterExpression'. Expected object.");
        }
        // If 'rootFilterExpression' is present, it should ideally replace 'filterRules'.
        // A more robust loader would handle this conflict or deprecation logic.

    } catch (const nlohmann::json::parse_error& e) {
        errors.push_back("JSON parsing error: " + std::string(e.what()));
    } catch (const nlohmann::json::exception& e) { // Catch other JSON errors
        errors.push_back("JSON processing error: " + std::string(e.what()));
    } catch (const std::exception& e) { // Catch standard exceptions
        errors.push_back("Standard exception during JSON processing: " + std::string(e.what()));
    }

    if (errors.empty()) {
        return settings;
    } else {
        return std::unexpected(errors);
    }
}

std::string LogAnalyzerSettings::toJson() const {
    nlohmann::json j;

    // Parsing Configuration
    j["lineParsePattern"] = lineParsePattern;
    
    j["fieldMappings"] = fieldMappings;

    j["customLogLevelMappings"] = nlohmann::json::object();
    for (const auto& pair : customLogLevelMappings) {
        j["customLogLevelMappings"][pair.first] = Utils::logLevelToString(pair.second);
    }

    if (logEntryStartPattern) {
        j["logEntryStartPattern"] = *logEntryStartPattern;
    } else {
        j["logEntryStartPattern"] = nullptr; // Explicitly null if not set
    }
    j["caseSensitiveParsing"] = caseSensitiveParsing;

    // Filtering Configuration
    // Note: This JSON output reflects the old 'filterRules' structure.
    // If 'rootFilterExpression' is implemented and preferred, this section needs adjustment.
    j["filterRules"] = filterRules;
    
    // --- New: Export settings ---
    // Uses the to_json helper defined for ExportSettings
    j["exportSettings"] = exportSettings;

    // --- New: Statistics configuration ---
    j["statisticConfigs"] = nlohmann::json::array();
    for (const auto& sc : statisticConfigs) {
        nlohmann::json sc_j;
        sc_j["type"] = Utils::statisticTypeToString(sc.type);
        if (sc.field) sc_j["field"] = Utils::logEntryFieldToString(*sc.field);
        if (sc.pattern) sc_j["pattern"] = *sc.pattern;
        if (sc.topN) sc_j["topN"] = *sc.topN;
        if (sc.groupByField) sc_j["groupByField"] = Utils::logEntryFieldToString(*sc.groupByField);
        j["statisticConfigs"].push_back(sc_j);
    }

    // --- New: Advanced Filtering (rootFilterExpression) ---
    // If rootFilterExpression is implemented, serialize it here.
    // For now, assuming it's not yet fully integrated for JSON serialization.
    // if (rootFilterExpression) {
    //     nlohmann::json fe_j;
    //     to_json(fe_j, *rootFilterExpression);
    //     j["rootFilterExpression"] = fe_j;
    // }

    return j.dump(4); // Use 4 spaces for indentation
}

std::expected<LogAnalyzerSettings, std::vector<std::string>> LogAnalyzerSettings::fromFile(const std::string& filePath) {
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        return std::unexpected<std::vector<std::string>>({"Failed to open configuration file: " + filePath});
    }

    std::string jsonContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return fromJson(jsonContent);
}

std::vector<std::string> LogAnalyzerSettings::validate() const {
    std::vector<std::string> errors;

    // Validate lineParsePattern
    try {
        std::regex(lineParsePattern);
    } catch (const std::regex_error& e) {
        errors.push_back("Invalid regex pattern for 'lineParsePattern': " + std::string(e.what()));
    }

    // Validate fieldMappings
    for (const auto& fm : fieldMappings) {
        if (!fm.groupIndex.has_value()) {
            errors.push_back("FieldMapping is missing groupIndex.");
        }
        // Check if field_identifier is a standard field and if it's UNKNOWN (if not handled correctly)
        if (std::holds_alternative<LogEntryField>(fm.field_identifier) && std::get<LogEntryField>(fm.field_identifier) == LogEntryField::UNKNOWN) {
             errors.push_back("FieldMapping has an unrecognized standard field name.");
        }
        // Check for custom field types if specified
        if (std::holds_alternative<std::string>(fm.field_identifier) && fm.customFieldType && !(*fm.customFieldType == "string" || *fm.customFieldType == "int" || *fm.customFieldType == "float" || *fm.customFieldType == "datetime" || *fm.customFieldType == "bool")) {
             errors.push_back("FieldMapping has an unsupported customFieldType: " + *fm.customFieldType);
        }
        // Further validation could check if formats are valid time formats if field is TIMESTAMP
    }

    // Validate filterRules
    for (const auto& fr : filterRules) {
        // Basic check: ensure field and op are not UNKNOWN if they were parsed from invalid strings.
        // More detailed validation would check type compatibility between field, op, and value.
        if (fr.field == LogEntryField::UNKNOWN) {
            errors.push_back("FilterRule has an unrecognized field.");
        }
        if (fr.op == FilterOperator::UNKNOWN) {
            errors.push_back("FilterRule has an unrecognized operator.");
        }
        // Example of more detailed validation: check if value type matches field type for certain ops.
        // This requires more context about field types.
    }

    // Validate exportSettings
    if (exportSettings.fieldsToExport.empty()) {
        errors.push_back("ExportSettings 'fieldsToExport' cannot be empty.");
    }
    // Further validation could check format-specific requirements for fieldsToExport.

    // Validate statisticConfigs
    for (const auto& sc : statisticConfigs) {
        if (sc.type == StatisticType::UNKNOWN) {
            errors.push_back("StatisticConfig has an unrecognized type.");
        }
        // Check if numeric stats have a field specified
        if ((sc.type == StatisticType::SUM || sc.type == StatisticType::AVERAGE || sc.type == StatisticType::MIN || sc.type == StatisticType::MAX) && !sc.field.has_value()) {
            errors.push_back("StatisticConfig of type SUM, AVERAGE, MIN, or MAX requires a 'field' to be specified.");
        }
        // Further validation could check if the specified field is suitable for the statistic type (e.g., numeric for SUM/AVG).
    }
    
    // --- Validate rootFilterExpression if implemented ---
    // if (rootFilterExpression) {
    //     // Perform validation on the FilterExpression tree.
    //     // This would involve recursively checking conditions and operands.
    // }

    return errors;
}

#endif // LOG_ANALYZER_CONFIG_H
