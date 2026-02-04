#ifndef LOG_ANALYZER_CONFIG_H
#define LOG_ANALYZER_CONFIG_H

#include "LogTypes.h"
#include "Filter.h"
#include "Exporter.h"
#include "Statistics.h"
#include "LogAnalyzerSettings.h"
#include "Utils.h"

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <string_view>
#include <functional>
#include <utility>
#include <expected>
#include <fstream>
#include <regex>
#include <nlohmann/json.hpp> 

// Defined as in LogAnalyzer.h comment
static constexpr std::string_view DEFAULT_LOG_REGEX_PATTERN_SV = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) ([A-Z]+): (.*)$)";

// --- Static Method Implementations for LogAnalyzerSettings ---

// Defined inline to avoid ODR violations when included in multiple translation units.
inline std::expected<LogAnalyzerSettings, std::vector<std::string>> LogAnalyzerSettings::fromJson(const std::string& jsonContent) {
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
            } catch (const std::exception& e) {
                errors.push_back("Error parsing 'fieldMappings': " + std::string(e.what()));
            }
        } else if (j.contains("fieldMappings")) {
            errors.push_back("Invalid type for 'fieldMappings'. Expected array.");
        }
        
        if (j.contains("customLogLevelMappings") && j.at("customLogLevelMappings").is_object()) {
            settings.customLogLevelMappings.clear(); // Clear defaults
            for (auto const& [levelStr, levelVal] : j.at("customLogLevelMappings").items()) {
                if (levelVal.is_string()) {
                    LogLevel parsedLevel = Utils::stringToLogLevel(levelVal.get<std::string>());
                    if (parsedLevel == LogLevel::UNKNOWN) {
                        errors.push_back("Invalid custom log level string '" + levelVal.get<std::string>() + "' for key '" + levelStr + "'.");
                    }
                    settings.customLogLevelMappings[levelStr] = parsedLevel;
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
            } catch (const std::exception& e) {
                errors.push_back("Error parsing 'filterRules': " + std::string(e.what()));
            }
        } else if (j.contains("filterRules")) {
            errors.push_back("Invalid type for 'filterRules'. Expected array.");
        }
        
        // Populate export configuration
        if (j.contains("exportSettings") && j.at("exportSettings").is_object()) {
            try {
                settings.exportSettings = j.at("exportSettings").get<ExportSettings>();
            } catch (const std::exception& e) {
                errors.push_back("Error parsing 'exportSettings': " + std::string(e.what()));
            }
        } else if (j.contains("exportSettings")) {
            errors.push_back("Invalid type for 'exportSettings'. Expected object.");
        }

        // Populate statistics configuration
        if (j.contains("statisticConfigs") && j.at("statisticConfigs").is_array()) {
            try {
                settings.statisticConfigs = j.at("statisticConfigs").get<std::vector<StatisticConfig>>();
            } catch (const std::exception& e) {
                errors.push_back("Error parsing 'statisticConfigs': " + std::string(e.what()));
            }
        } else if (j.contains("statisticConfigs")) {
            errors.push_back("Invalid type for 'statisticConfigs'. Expected array.");
        }

        // --- Handling the new FilterExpression (Advanced Filtering) ---
        if (j.contains("rootFilterExpression") && j.at("rootFilterExpression").is_object()) {
            try {
                settings.rootFilterExpression = j.at("rootFilterExpression").get<FilterExpression>();
            } catch (const std::exception& e) {
                errors.push_back("Error parsing 'rootFilterExpression': " + std::string(e.what()));
            }
        } else if (j.contains("rootFilterExpression")) {
            errors.push_back("Invalid type for 'rootFilterExpression'. Expected object.");
        }

    } catch (const nlohmann::json::parse_error& e) {
        errors.push_back("JSON parsing error: " + std::string(e.what()));
    } catch (const nlohmann::json::exception& e) { // Catch nlohmann::json specific exceptions
        errors.push_back("JSON data error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        errors.push_back("Exception during JSON processing: " + std::string(e.what()));
    }

    if (errors.empty()) {
        std::vector<std::string> validationErrors = settings.validate();
        if (!validationErrors.empty()) {
            errors.insert(errors.end(), validationErrors.begin(), validationErrors.end());
            return std::unexpected(errors);
        }
        return settings;
    } else {
        return std::unexpected(errors);
    }
}

inline std::string LogAnalyzerSettings::toJson() const {
    nlohmann::json j;

    j["lineParsePattern"] = lineParsePattern;
    j["fieldMappings"] = fieldMappings;

    j["customLogLevelMappings"] = nlohmann::json::object();
    for (const auto& pair : customLogLevelMappings) {
        j["customLogLevelMappings"][pair.first] = Utils::logLevelToString(pair.second);
    }

    if (logEntryStartPattern) {
        j["logEntryStartPattern"] = *logEntryStartPattern;
    } else {
        j["logEntryStartPattern"] = nullptr;
    }
    j["caseSensitiveParsing"] = caseSensitiveParsing;
    j["filterRules"] = filterRules;
    j["exportSettings"] = exportSettings;

    j["statisticConfigs"] = statisticConfigs;

    if (rootFilterExpression) {
        j["rootFilterExpression"] = *rootFilterExpression;
    }

    return j.dump(4);
}

inline std::expected<LogAnalyzerSettings, std::vector<std::string>> LogAnalyzerSettings::fromFile(const std::string& filePath) {
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        return std::unexpected<std::vector<std::string>>({"Failed to open configuration file: " + filePath});
    }

    std::string jsonContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return fromJson(jsonContent);
}

inline std::vector<std::string> LogAnalyzerSettings::validate() const {
    std::vector<std::string> errors;

    try {
        std::regex re(lineParsePattern);
    } catch (const std::regex_error& e) {
        errors.push_back("Invalid regex pattern for 'lineParsePattern': " + std::string(e.what()));
    }

    for (const auto& fm : fieldMappings) {
        if (!fm.groupIndex.has_value()) {
            errors.push_back("FieldMapping is missing groupIndex.");
        }
    }

    for (const auto& fr : filterRules) {
        if (fr.field == LogEntryField::UNKNOWN) {
            errors.push_back("FilterRule has an unrecognized field.");
        }
        if (fr.op == FilterOperator::UNKNOWN) {
            errors.push_back("FilterRule has an unrecognized operator.");
        }
    }

    if (exportSettings.fieldsToExport.empty()) {
        errors.push_back("ExportSettings 'fieldsToExport' cannot be empty.");
    }

    for (const auto& sc : statisticConfigs) {
        if (sc.type == StatisticType::UNKNOWN) {
            errors.push_back("StatisticConfig has an unrecognized type.");
        }
    }

    return errors;
}

#endif // LOG_ANALYZER_CONFIG_H
