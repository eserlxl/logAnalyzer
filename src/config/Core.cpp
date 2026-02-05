#include "config/Core.h"
#include "config/Settings.h"
#include "utils/Core.h"
#include "filter/Core.h" 
#include "export/Exporter.h"
#include "stats/Statistics.h"
#include <fstream>
#include <regex>
#include <nlohmann/json.hpp>
#include <cstdlib> // for getenv
#include <set>
#include <filesystem>

// --- Forward Declarations for Internal Helpers ---

namespace {

/**
 * Expands environment variables in the format ${VAR} or $VAR within a string.
 */
std::string expandEnvironmentVariables(std::string_view content) {
    // Regex to find ${VAR} or $VAR patterns, capturing preceding backslashes.
    // Group 1: Preceding backslashes
    // Group 2: The full variable token (${...} or $...)
    // Group 3: Content of ${...}
    // Group 4: Content of $...
    static const std::regex envVarRegex(R"((\\*)(\$\{([^}]+)\}|\$([A-Za-z0-9_]+)))");
    
    std::string result;
    std::cregex_iterator it(content.data(), content.data() + content.size(), envVarRegex);
    std::cregex_iterator end;
    
    const char* last_match_end = content.data();

    for (; it != end; ++it) {
        const auto& match = *it;
        // Append the text between the last match and this one
        result.append(last_match_end, match[0].first);

        std::string backslashes = match[1].str();
        std::string varToken = match[2].str();
        std::string varName = match[3].matched ? match[3].str() : match[4].str();
        
        if (backslashes.length() % 2 != 0) {
            // Odd number of backslashes -> Escaped.
            // Remove one backslash and keep the variable token literally.
            result.append(backslashes.substr(0, backslashes.length() - 1));
            result.append(varToken);
        } else {
            // Even number of backslashes -> Not escaped.
            // Keep all backslashes and expand the variable.
            result.append(backslashes);
            
            const char* varValue = std::getenv(varName.c_str());
            if (varValue) {
                result.append(varValue);
            }
        }

        last_match_end = match[0].second;
    }

    // Append the remaining part of the string
    result.append(last_match_end, content.data() + content.size());
    return result;
}

/**
 * Recursively resolves "includes" in a JSON configuration file.
 * @param currentFile The path of the file being processed.
 * @param rootJson The JSON object to process.
 * @param visitedFiles A set to track visited files and prevent circular includes.
 * @param errors A vector to accumulate errors.
 * @return A merged JSON object.
 */
nlohmann::json resolveIncludes(
    const std::filesystem::path& currentFile, 
    nlohmann::json rootJson, 
    std::set<std::filesystem::path>& visitedFiles,
    std::vector<std::string>& errors) 
{
    if (visitedFiles.count(currentFile)) {
        errors.push_back("Circular include detected: " + currentFile.string());
        return rootJson;
    }
    visitedFiles.insert(currentFile);

    if (rootJson.contains("includes") && rootJson["includes"].is_array()) {
        nlohmann::json mergedIncludes = nlohmann::json::object();
        auto parentDir = currentFile.parent_path();

        for (const auto& includeNode : rootJson["includes"]) {
            if (!includeNode.is_string()) {
                errors.push_back("Include path must be a string in file: " + currentFile.string());
                continue;
            }

            std::filesystem::path includePath = parentDir / includeNode.get<std::string>();
            includePath = std::filesystem::canonical(includePath); // Resolve to absolute path

            std::ifstream ifs(includePath);
            if (!ifs.is_open()) {
                errors.push_back("Failed to open include file: " + includePath.string());
                continue;
            }

            std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            
            try {
                nlohmann::json includedJson = nlohmann::json::parse(content);
                // Recursively resolve includes in the included file first.
                nlohmann::json resolvedSubJson = resolveIncludes(includePath, std::move(includedJson), visitedFiles, errors);
                // Merge the resolved sub-JSON into our merged result.
                // Later includes will overwrite earlier ones.
                mergedIncludes.merge_patch(resolvedSubJson);
            } catch (const nlohmann::json::parse_error& e) {
                errors.push_back("JSON parsing error in include file " + includePath.string() + ": " + e.what());
            }
        }
        
        // Remove the "includes" key from the root JSON before merging.
        rootJson.erase("includes");
        // Merge the root object on top of the merged includes.
        // This ensures that values in the root file have precedence.
        mergedIncludes.merge_patch(rootJson);
        rootJson = mergedIncludes;
    }

    visitedFiles.erase(currentFile);
    return rootJson;
}

} // anonymous namespace

// --- Static Method Implementations for LogAnalyzerSettings ---

std::expected<LogAnalyzerSettings, std::vector<std::string>> LogAnalyzerSettings::fromFile(
    const std::filesystem::path& filePath, 
    bool expandEnv) 
{
    if (!std::filesystem::exists(filePath)) {
        return std::unexpected<std::vector<std::string>>({"Configuration file does not exist: " + filePath.string()});
    }

    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        return std::unexpected<std::vector<std::string>>({"Failed to open configuration file: " + filePath.string()});
    }

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    
    if (expandEnv) {
        content = expandEnvironmentVariables(content);
    }

    std::vector<std::string> errors;
    try {
        nlohmann::json rootJson = nlohmann::json::parse(content);
        
        // Resolve includes
        std::set<std::filesystem::path> visitedFiles;
        auto absolutePath = std::filesystem::canonical(filePath);
        nlohmann::json finalJson = resolveIncludes(absolutePath, std::move(rootJson), visitedFiles, errors);
        
        if (!errors.empty()) {
            return std::unexpected(errors);
        }

        // Now, parse the fully resolved JSON into settings
        return fromJson(finalJson.dump());

    } catch (const nlohmann::json::parse_error& e) {
        errors.push_back("JSON parsing error in " + filePath.string() + ": " + e.what());
        return std::unexpected(errors);
    }
}


std::expected<LogAnalyzerSettings, std::vector<std::string>> LogAnalyzerSettings::fromJson(const std::string& jsonContent) {
    std::vector<std::string> errors;
    LogAnalyzerSettings settings = LogAnalyzerSettings::createDefault();

    try {
        nlohmann::json j = nlohmann::json::parse(jsonContent);

        if (j.contains("version") && j["version"].is_string()) {
            settings.version = j["version"].get<std::string>();
            // Basic version check (example)
            if (settings.version > "1.0") {
                // For now, just a note. In the future, this could be a hard error for major versions.
                // std::cout << "Warning: Configuration file version '" << settings.version << "' is newer than the supported version '1.0'." << std::endl;
            }
        }

        if (j.contains("lineParsePattern") && j.at("lineParsePattern").is_string()) {
            settings.lineParsePattern = j.at("lineParsePattern").get<std::string>();
        } else if (j.contains("lineParsePattern")) {
            errors.push_back("Invalid type for 'lineParsePattern'. Expected string.");
        }

        if (j.contains("fieldMappings") && j.at("fieldMappings").is_array()) {
            try {
                // Overwrite default mappings if provided.
                settings.fieldMappings = j.at("fieldMappings").get<std::vector<FieldMapping>>();
            } catch (const std::exception& e) {
                errors.push_back("Error parsing 'fieldMappings': " + std::string(e.what()));
            }
        } else if (j.contains("fieldMappings")) {
            errors.push_back("Invalid type for 'fieldMappings'. Expected array.");
        }
        
        if (j.contains("customLogLevelMappings") && j.at("customLogLevelMappings").is_object()) {
            settings.customLogLevelMappings.clear(); // Clear defaults before adding new ones
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
            settings.logEntryStartPattern = std::nullopt;
        }

        if (j.contains("caseSensitiveParsing") && j.at("caseSensitiveParsing").is_boolean()) {
            settings.caseSensitiveParsing = j.at("caseSensitiveParsing").get<bool>();
        } else if (j.contains("caseSensitiveParsing")) {
            errors.push_back("Invalid type for 'caseSensitiveParsing'. Expected boolean.");
        }

        if (j.contains("filterRules") && j.at("filterRules").is_array()) {
            settings.filterRules.clear();
            for (const auto& ruleJson : j.at("filterRules")) {
                FilterRule rule;
                auto result = from_json(ruleJson, rule);
                if (!result.has_value()) {
                    errors.push_back("Error parsing 'filterRules': " + result.error().message);
                } else {
                    settings.filterRules.push_back(rule);
                }
            }
        } else if (j.contains("filterRules")) {
            errors.push_back("Invalid type for 'filterRules'. Expected array.");
        }
        
        if (j.contains("exportSettings") && j.at("exportSettings").is_object()) {
            try {
                settings.exportSettings = j.at("exportSettings").get<ExportSettings>();
            } catch (const std::exception& e) {
                errors.push_back("Error parsing 'exportSettings': " + std::string(e.what()));
            }
        } else if (j.contains("exportSettings")) {
            errors.push_back("Invalid type for 'exportSettings'. Expected object.");
        }

        if (j.contains("statisticConfigs") && j.at("statisticConfigs").is_array()) {
            settings.statisticConfigs.clear();
            try {
                settings.statisticConfigs = j.at("statisticConfigs").get<std::vector<StatisticConfig>>();
            } catch (const std::exception& e) {
                errors.push_back("Error parsing 'statisticConfigs': " + std::string(e.what()));
            }
        } else if (j.contains("statisticConfigs")) {
            errors.push_back("Invalid type for 'statisticConfigs'. Expected array.");
        }

        if (j.contains("rootFilterExpression") && j.at("rootFilterExpression").is_object()) {
            FilterExpression fe;
            auto result = from_json(j.at("rootFilterExpression"), fe);
            if (result) {
                settings.rootFilterExpression = fe;
            } else {
                errors.push_back("Error parsing 'rootFilterExpression': " + result.error().message);
            }
        } else if (j.contains("rootFilterExpression")) {
            errors.push_back("Invalid type for 'rootFilterExpression'. Expected object.");
        }

    } catch (const nlohmann::json::parse_error& e) {
        errors.push_back("JSON parsing error: " + std::string(e.what()));
    } catch (const nlohmann::json::exception& e) {
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

std::string LogAnalyzerSettings::toJson() const {
    nlohmann::json j;

    j["version"] = version;
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

    if (!filterRules.empty()) {
        j["filterRules"] = filterRules;
    }
    j["exportSettings"] = exportSettings;

    if (!statisticConfigs.empty()) {
        j["statisticConfigs"] = statisticConfigs;
    }

    if (rootFilterExpression) {
        j["rootFilterExpression"] = *rootFilterExpression;
    }

    return j.dump(4);
}

void LogAnalyzerSettings::merge(const LogAnalyzerSettings& other) {
    // Overwrite simple scalar values if the source has them set to a non-default.
    // This logic can be fine-tuned based on what "default" means for each property.
    if (!other.lineParsePattern.empty() && other.lineParsePattern != LogAnalyzerSettings::createDefault().lineParsePattern) {
        lineParsePattern = other.lineParsePattern;
    }
    
    // For optionals, overwrite if 'other' has a value.
    if (other.logEntryStartPattern.has_value()) {
        logEntryStartPattern = other.logEntryStartPattern;
    }
    
    // Simple boolean overwrite
    caseSensitiveParsing = other.caseSensitiveParsing;

    // For collections, replace if the 'other' collection is not empty.
    if (!other.fieldMappings.empty()) {
        fieldMappings = other.fieldMappings;
    }
    if (!other.customLogLevelMappings.empty()) {
        customLogLevelMappings = other.customLogLevelMappings;
    }
    if (!other.filterRules.empty()) {
        filterRules = other.filterRules;
    }
    if (!other.statisticConfigs.empty()) {
        statisticConfigs = other.statisticConfigs;
    }
    
    // For complex objects, you might need a deeper merge logic, but for now, we replace.
    // This simple replacement assumes `other` provides a complete replacement.
    exportSettings = other.exportSettings; // Assumes ExportSettings has its own sane copy/move semantics

    if (other.rootFilterExpression.has_value()) {
        rootFilterExpression = other.rootFilterExpression;
    }
}


LogAnalyzerSettings LogAnalyzerSettings::createDefault() {
    LogAnalyzerSettings defaults;
    defaults.lineParsePattern = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) ([A-Z]+): (.*)$)";
    defaults.fieldMappings.clear();
    defaults.fieldMappings.emplace_back(LogEntryField::TIMESTAMP, 1, "%Y-%m-%d %H:%M:%S");
    defaults.fieldMappings.emplace_back(LogEntryField::LEVEL, 2);
    defaults.fieldMappings.emplace_back(LogEntryField::MESSAGE, 3);
    defaults.caseSensitiveParsing = false;
    
    // Default export settings
    defaults.exportSettings.fieldsToExport = {
        LogEntryField::TIMESTAMP,
        LogEntryField::LEVEL,
        LogEntryField::MESSAGE
    };
    
    return defaults;
}


std::vector<std::string> LogAnalyzerSettings::validate() const {
    std::vector<std::string> errors;

    try {
        std::regex re(lineParsePattern);
    } catch (const std::regex_error& e) {
        errors.push_back("Invalid regex pattern for 'lineParsePattern': " + std::string(e.what()));
    }

    if (fieldMappings.empty()) {
        errors.push_back("'fieldMappings' cannot be empty for parsing to work.");
    }
    for (const auto& fm : fieldMappings) {
        if (!fm.groupIndex.has_value()) {
            errors.push_back("FieldMapping is missing required 'groupIndex'.");
        }
    }

    for (const auto& fr : filterRules) {
        if (fr.field == LogEntryField::UNKNOWN) {
            errors.push_back("FilterRule has an unrecognized field.");
        }
        if (fr.op == FilterOperator::REGEX_MATCH) {
            try {
                std::regex re(fr.value);
            } catch (const std::regex_error& e) {
                errors.push_back("Invalid regex pattern in FilterRule: " + std::string(e.what()));
            }
        }
    }



    for (const auto& sc : statisticConfigs) {
        if (sc.type == StatisticType::UNKNOWN) {
            errors.push_back("StatisticConfig has an unrecognized type.");
            continue;
        }

        auto it_top_n = sc.params.find("top_n");
        bool has_top_n = (it_top_n != sc.params.end());
        
        auto it_target_field = sc.params.find("target_field");
        bool has_target_field = (it_target_field != sc.params.end());

        std::string typeStr = Utils::statisticTypeToString(sc.type);

        if (sc.type == StatisticType::TOP_MESSAGES || sc.type == StatisticType::TOP_N_FIELD_VALUES) {
            if (!has_top_n) {
                errors.push_back("Statistic '" + typeStr + "' requires a 'top_n' parameter.");
            } else {
                try {
                    if (std::stoi(it_top_n->second) <= 0) {
                        errors.push_back("Parameter 'top_n' for statistic '" + typeStr + "' must be a positive integer.");
                    }
                } catch (...) {
                    errors.push_back("Parameter 'top_n' for statistic '" + typeStr + "' must be a positive integer.");
                }
            }
        }

        if (sc.type == StatisticType::FIELD_VALUE_COUNT || sc.type == StatisticType::TOP_N_FIELD_VALUES) {
            if (!has_target_field) {
                errors.push_back("Statistic '" + typeStr + "' requires a 'target_field' parameter.");
            } else if (it_target_field->second == "customFields") {
                auto it_custom_field_key = sc.params.find("custom_field_key");
                if (it_custom_field_key == sc.params.end()) {
                    errors.push_back("Statistic '" + typeStr + "' with target_field 'customFields' requires a 'custom_field_key' parameter.");
                }
            }
        }
    }

    return errors;
}
