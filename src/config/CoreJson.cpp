// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "config/Settings.h"
#include "config/Utils.h"
#include "config/CommonTypes.h"
#include "filter/Core.h"
#include "export/Core.h"
#include "stats/Core.h"
#include <fstream>
#include <regex>
#include <nlohmann/json.hpp>
#include <cstdlib> // for getenv
#include <set>
#include <filesystem>
#include <iostream>

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
            std::error_code ec;
            includePath = std::filesystem::canonical(includePath, ec); // Resolve to absolute path
            if (ec) {
                errors.push_back("Failed to resolve include path: " + includeNode.get<std::string>() + " (Error: " + ec.message() + ")");
                continue;
            }

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

std::string parserErrorActionToString(ParserErrorAction action) {
    switch (action) {
        case ParserErrorAction::Ignore: return "ignore";
        case ParserErrorAction::Warn: return "warn";
        case ParserErrorAction::Throw: return "throw";
        default: return "warn";
    }
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
        std::error_code ec;
        auto absolutePath = std::filesystem::canonical(filePath, ec);
        if (ec) {
            return std::unexpected<std::vector<std::string>>({"Failed to resolve configuration file path: " + filePath.string() + " (Error: " + ec.message() + ")"});
        }
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
        if (!j.is_object()) {
            errors.push_back("Invalid top-level JSON type. Expected object.");
            return std::unexpected(errors);
        }

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
                auto result = filter::from_json(ruleJson);
                if (!result.has_value()) {
                    errors.push_back("Error parsing 'filterRules': " + result.error().message);
                } else {
                    settings.filterRules.push_back(result.value());
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

        if (j.contains("parserErrorAction") && j.at("parserErrorAction").is_string()) {
            std::string actionStr = j.at("parserErrorAction").get<std::string>();
            if (Config::ParserErrorActionMap.count(actionStr)) {
                settings.parserErrorAction = Config::ParserErrorActionMap.at(actionStr);
            } else {
                errors.push_back("Invalid value for 'parserErrorAction': " + actionStr);
            }
        } else if (j.contains("parserErrorAction")) {
            errors.push_back("Invalid type for 'parserErrorAction'. Expected string.");
        }

        if (j.contains("maxMultilineBufferSize") && j.at("maxMultilineBufferSize").is_number_unsigned()) {
            settings.maxMultilineBufferSize = j.at("maxMultilineBufferSize").get<size_t>();
        } else if (j.contains("maxMultilineBufferSize") && j.at("maxMultilineBufferSize").is_string()) {
            auto sizeRes = Utils::parseHumanReadableSize(j.at("maxMultilineBufferSize").get<std::string>());
            if (sizeRes) {
                settings.maxMultilineBufferSize = *sizeRes;
            } else {
                errors.push_back("Invalid human-readable size for 'maxMultilineBufferSize': " + j.at("maxMultilineBufferSize").get<std::string>());
            }
        } else if (j.contains("maxMultilineBufferSize")) {
            errors.push_back("Invalid type for 'maxMultilineBufferSize'. Expected unsigned number or string.");
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
            filter::FilterExpression fe;
            auto result = filter::from_json(j.at("rootFilterExpression"), fe);
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
    
    if (caseSensitiveParsing.has_value()) {
        j["caseSensitiveParsing"] = caseSensitiveParsing.value();
    }

    if (parserErrorAction.has_value()) {
        j["parserErrorAction"] = parserErrorActionToString(parserErrorAction.value());
    }

    if (maxMultilineBufferSize.has_value()) {
        j["maxMultilineBufferSize"] = maxMultilineBufferSize.value();
    }

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
