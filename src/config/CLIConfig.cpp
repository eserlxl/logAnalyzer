// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "config/CLIConfig.h"
#include "config/CommonTypes.h" // Added for centralized types
#include "config/Utils.h" // Added for centralized config utilities
#include "utils/Core.h"
#include "core/Error.h" // Add this include
#include "core/CiLess.h" // For ci_less
#include "stats/Statistics.h" // For StatisticType, StatisticConfig
#include "utils/Version.h"
#include <CLI/CLI.hpp>
#include <algorithm> // For std::transform
#include <iostream> // For std::cerr
#include <string_view>
#include <sstream>

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "0.0.0-dev"
#endif

using namespace ErrorCode;

namespace {
    // Helper to parse extended --stats syntax (e.g., "type=TOP_MESSAGES,top_n=5")
    // or legacy syntax (e.g., "unique_messages", "top_messages:10")
    std::optional<StatisticConfig> parseStatisticConfig(const std::string& statStr) {
        StatisticConfig config;
        
        // Handle legacy top_messages:N
        if (statStr.find("top_messages:") == 0) {
            config.type = StatisticType::TOP_MESSAGES;
            config.params["top_n"] = statStr.substr(13);
            return config;
        }

        // Try to parse as legacy simple name first
        auto legacyType = Utils::stringToStatisticType(statStr);
        if (legacyType.has_value()) {
            config.type = *legacyType;
            return config;
        }

        // Try parsing key-value pairs
        bool typeFound = false;
        std::string token;
        std::istringstream tokenStream(statStr);
        
        while (std::getline(tokenStream, token, ',')) {
            auto pos = token.find('=');
            if (pos != std::string::npos) {
                std::string key = token.substr(0, pos);
                std::string value = token.substr(pos + 1);
                
                // Trim key and value? CLI11 usually handles spaces around args, but internal commas might need care. 
                // Assuming simple parsing for now.
                
                if (key == "type") {
                    auto type = Utils::stringToStatisticType(value);
                    if (type) {
                        config.type = *type;
                        typeFound = true;
                    } else {
                        // Invalid type in key-value pair
                        return std::nullopt; 
                    }
                } else {
                    config.params[key] = value;
                }
            } else {
                // Token without '=', maybe it's just the type name mixed with params? 
                // e.g. "TOP_MESSAGES,top_n=5"
                auto type = Utils::stringToStatisticType(token);
                if (type) {
                    config.type = *type;
                    typeFound = true;
                }
            }
        }
        
        if (typeFound) {
            return config;
        }
        
        return std::nullopt;
    }

    // Helper to parse "field as alias" string
    std::pair<std::string, std::string> parseFieldAlias(const std::string& fieldStr) {
        std::string lower = fieldStr;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        auto pos = lower.find(" as ");
        if (pos != std::string::npos) {
            std::string field = fieldStr.substr(0, pos);
            std::string alias = fieldStr.substr(pos + 4); // +4 for " as "
            
            // Trim whitespace
            field.erase(0, field.find_first_not_of(" \t"));
            auto fieldEnd = field.find_last_not_of(" \t");
            if (fieldEnd != std::string::npos) field.erase(fieldEnd + 1);
            
            alias.erase(0, alias.find_first_not_of(" \t"));
            auto aliasEnd = alias.find_last_not_of(" \t");
            if (aliasEnd != std::string::npos) alias.erase(aliasEnd + 1);
            
            return {field, alias};
        }
        
        std::string field = fieldStr;
        field.erase(0, field.find_first_not_of(" \t"));
        auto end = field.find_last_not_of(" \t");
        if (end != std::string::npos) field.erase(end + 1);
        return {field, field}; // No alias, use field name
    }
} // namespace

// CLI Parsing
Result<std::pair<LogAnalyzerSettings, CLIConfig::CLIOptions>> CLIConfig::parseCLI(int argc, const char *const *argv) {
    LogAnalyzerSettings settings;
    CLIOptions appOptions;
    CLI::App app{"Log Analyzer Tool"};
    app.set_version_flag("--version", PROJECT_VERSION);

    app.set_config("--config", "", "Read options from a configuration file", false);

    // Positional Arguments
    app.add_option("log_files", appOptions.filePaths, "Path to log files or '-' for stdin");

    // Filters
    app.add_option("--level", appOptions.filterLevels, "Filter by log levels (e.g., ERROR,WARNING)")
       ->transform(CLI::CheckedTransformer(Config::LogLevelMap, CLI::ignore_case));
       
    app.add_option("--min-level", appOptions.minLogLevel, "Filter entries with level greater than or equal to a specified level")
       ->transform(CLI::CheckedTransformer(Config::LogLevelMap, CLI::ignore_case));

    app.add_option("--keyword", appOptions.filterKeywords, "Filter messages containing specific text");
    app.add_option("--exclude-keyword", appOptions.excludeKeywords, "Exclude log entries containing a specific keyword");
       
    app.add_flag("--case-sensitive", appOptions.keywordCaseSensitive, "Make keyword filter case-sensitive");
    
    app.add_option("--regex", appOptions.regexPatterns, "Filter messages using regex");
    app.add_option("--exclude-regex", appOptions.excludeRegexPatterns, "Filter out log entries matching a specific regular expression");

    app.add_option("--logic", appOptions.filterLogic, "Logic to combine multiple filters of the same type (AND or OR)")
       ->transform(CLI::CheckedTransformer(Config::FilterLogicMap, CLI::ignore_case));

    app.add_option("--start", "Start time filter (YYYY-MM-DD HH:MM:SS, ISO 8601, Unix timestamp, or relative like '1h ago')")
       ->check([](const std::string &str) -> std::string {
            auto result = Utils::validateTimestampCliOption(str);
            if (!result) {
                return result.error().toString();
            }
            return "";
        })
       ->transform([&](const std::string& tsStr){
          auto parsedTime = Utils::parseTime(tsStr);
          if (parsedTime.has_value()) {
              appOptions.startTime = parsedTime.value();
              return tsStr; // Return original string for CLI internal use
          }
          throw CLI::ValidationError("Internal Error: Timestamp validation passed but parsing failed for --start. This should not happen.");
       });
    app.add_option("--end", "End time filter (YYYY-MM-DD HH:MM:SS, ISO 8601, Unix timestamp, or relative like '1h ago')")
       ->check([](const std::string &str) -> std::string {
            auto result = Utils::validateTimestampCliOption(str);
            if (!result) {
                return result.error().toString();
            }
            return "";
        })
       ->transform([&](const std::string& tsStr){
          auto parsedTime = Utils::parseTime(tsStr);
          if (parsedTime.has_value()) {
              appOptions.endTime = parsedTime.value();
              return tsStr; // Return original string for CLI internal use
          }
          throw CLI::ValidationError("Internal Error: Timestamp validation passed but parsing failed for --end. This should not happen.");
       });
    
    std::string durationStr;
    app.add_option("--duration", durationStr, "Duration for time filtering (e.g., '30m', '1h')");

    // Sorting
    app.add_option("--sort-by", appOptions.sortBy, "Sort entries by field")
       ->transform(CLI::CheckedTransformer(Config::SortByMap, CLI::ignore_case));
    
    app.add_option("--order", appOptions.sortOrder, "Sort order")
       ->transform(CLI::CheckedTransformer(Config::SortOrderMap, CLI::ignore_case));

    // Output Configuration
    app.add_option("--pattern", appOptions.lineParsePattern, "Custom regex for parsing log lines");
    app.add_option("--multiline-start-pattern", appOptions.multilineStartPattern, "Regex to identify the start of a multi-line log entry");
    
    // Human-readable max multiline buffer size
    std::string maxBufferStr;
    app.add_option("--max-multiline-buffer", maxBufferStr, "Max buffer size for multi-line entries (e.g. 10MB, 50KB, 1048576). Default: 10MB")
       ->check([](const std::string &str) -> std::string {
           if (Utils::parseHumanReadableSize(str)) return "";
           return "Invalid size format. Use numeric value optionally followed by B, KB, MB, GB, TB.";
       });

    app.add_option("--field-map", appOptions.fieldMaps, "Map regex capture group to a field (e.g., '1=timestamp:%Y-%m-%d %H:%M:%S')");

    app.add_option("--format", appOptions.outputFormat, "Output format (text, json, csv)")
       ->transform(CLI::IsMember({"text", "json", "csv"}, CLI::ignore_case));
    app.add_option("--output", appOptions.outputPath, "Redirect output to a file");
    app.add_option("--text-format", appOptions.textOutputFormat, "Custom format string for text output. Available: {timestamp}, {level}, {message}, {lineNumber}, {fileName}, {elapsedTime}.");
    
    app.add_flag("--include-summary", appOptions.includeSummary, "Include summary in JSON output");
    app.add_flag("--pretty", appOptions.prettyPrint, "Pretty-print JSON output");

    app.add_option("--color", appOptions.colorOption, "Control output color (always, auto, never)")
       ->transform(CLI::CheckedTransformer(Config::ColorOptionMap, CLI::ignore_case));

    app.add_option("--csv-sep", appOptions.csvSeparator, "Custom separator for CSV output (defaults to ',')");
    
    // CSV and JSON Fields
    app.add_option("--csv-fields", appOptions.csvFields, "Ordered list of fields for CSV output (e.g., 'timestamp as Time, level, message')")
       ->delimiter(',');
    app.add_option("--json-fields", appOptions.jsonFields, "Ordered list of fields for JSON output (e.g., 'timestamp as time, log_level as level')")
       ->delimiter(',');

    // Analysis Options
    app.add_flag("--stdin", appOptions.readFromStdin, "Read log entries from standard input (stdin) if no file paths are provided.");
    
    app.add_option("--stats", appOptions.enabledStatistics, "Enable statistics collectors (e.g., unique_messages, 'type=TOP_MESSAGES,top_n=5')");
    // Delimiter removed to support complex strings with commas. 
    // Multiple stats should be provided via multiple --stats flags.

    app.add_option("--top-n", appOptions.topMessagesCount, "Number of top messages to show for top_messages statistic (Deprecated: use --stats \"type=TOP_MESSAGES,top_n=X\")") 
       ->check(CLI::PositiveNumber); 

    app.add_flag("--stream", appOptions.streamMode, "Enable streaming mode for large files");

    // Custom Level Mapping
    app.add_option_function<std::vector<std::string>>("--map-level", [&](const std::vector<std::string>& val){
        for(const auto& s : val) {
            auto pos = s.find('=');
            if(pos == std::string::npos) throw CLI::ValidationError("Invalid KEY=VALUE format for --map-level");
            std::string from = s.substr(0, pos);
            std::string to = s.substr(pos + 1);
            std::string toUpper = to;
            std::transform(toUpper.begin(), toUpper.end(), toUpper.begin(), ::toupper);
            
            if(Config::LogLevelMap.count(toUpper)) {
                settings.customLogLevelMappings[from] = Config::LogLevelMap.at(toUpper);
            } else {
                throw CLI::ValidationError("Invalid log level in --map-level: " + to);
            }
        }
    }, "Map custom log levels (KEY=LEVEL)");

    // Stats (Legacy options)
    int statsWindowSec = 0;
    app.add_option("--stats-window", statsWindowSec, "Show log frequency distribution over a time window (seconds) (Deprecated)");
    
    int gapDurationMs = 0;
    app.add_option("--find-gaps", gapDurationMs, "Find time gaps longer than X ms (Deprecated)");

    // New options from Iteration 7 Design
    app.add_option("--on-parse-error", appOptions.parserErrorAction, "Action on parse error (skip, log, fail)")
       ->transform(CLI::CheckedTransformer(Config::ParserErrorActionMap, CLI::ignore_case));

    app.add_flag("--tail", appOptions.tailMode, "Enable tail mode to monitor files for new lines");
    int tailIntervalMs = 1000;
    app.add_option("--tail-interval", tailIntervalMs, "Polling interval for tail mode in ms (default: 1000)");

    app.add_option("--expression", appOptions.complexFilterExpression, "Complex filter expression");


    try {
        app.parse(argc, argv);
    } catch (const CLI::CallForHelp &e) {
        appOptions.exitAfterParse = true;
        std::cout << app.help() << std::endl;
        return std::make_pair(settings, appOptions);
    } catch (const CLI::CallForVersion &e) {
        appOptions.exitAfterParse = true;
        std::cout << app.version() << std::endl;
        return std::make_pair(settings, appOptions);
    } catch (const CLI::Error &e) {
        // Refined error handling could inspect 'e' more here if needed
        std::stringstream ss;
        app.exit(e, ss, ss);
        return std::unexpected(ErrorCode::Error(::Code::InvalidCLIOption, ss.str()));
    }

    // Deprecation warnings
    if (!appOptions.enabledStatistics.empty()) {
        if (app.count("--top-n")) {
            std::cerr << "Warning: --top-n is deprecated. Please use --stats \"type=TOP_MESSAGES,top_n=" 
                      << appOptions.topMessagesCount << "\" instead." << std::endl;
        }
        // stats-window and find-gaps can be checked similarly if they map to new stats
    }

    // Parse --max-multiline-buffer
    if (!maxBufferStr.empty()) {
        auto sizeRes = Utils::parseHumanReadableSize(maxBufferStr);
        if (sizeRes) {
            appOptions.maxMultilineBufferSize = *sizeRes;
        } else {
            // Should be caught by check(), but just in case
            return std::unexpected(sizeRes.error());
        }
    }

    // Process field maps from CLI options
    auto parsedFieldMapsResult = ConfigUtils::parseFieldMappingStrings(appOptions.fieldMaps);
    if (!parsedFieldMapsResult) {
        return std::unexpected(parsedFieldMapsResult.error());
    }
    settings.fieldMappings = *parsedFieldMapsResult;

    // Process statistics
    for (const auto& statStr : appOptions.enabledStatistics) {
        auto statConfig = parseStatisticConfig(statStr);
        if (statConfig) {
            // Backward compatibility for top_n
            if (statConfig->type == StatisticType::TOP_MESSAGES && statConfig->params.find("top_n") == statConfig->params.end()) {
                if (app.count("--top-n")) {
                    statConfig->params["top_n"] = std::to_string(appOptions.topMessagesCount);
                }
            }
            settings.statisticConfigs.push_back(*statConfig);
        } else {
            return std::unexpected(ErrorCode::Error(::Code::InvalidCLIOption, "Invalid statistic configuration: " + statStr));
        }
    }
    
    // Map legacy stats flags to StatisticConfig if not already present?
    // The design says: "The simple --stats NAME syntax ... will be retained ... mapping to a default StatisticConfig"
    // What about --stats-window?
    if (statsWindowSec > 0) {
        StatisticConfig sc;
        sc.type = StatisticType::ENTRY_RATE; // Assuming this maps to entry rate over window?
        // Actually ENTRY_RATE usually implies a window.
        sc.params["window"] = std::to_string(statsWindowSec) + "s";
        settings.statisticConfigs.push_back(sc);
        appOptions.statsWindow = std::chrono::seconds(statsWindowSec); // Keep for legacy compatibility if used elsewhere
    }
    
    // What about --find-gaps? No direct statistic type for "Gaps" in Statistics.h yet, but maybe implicitly handled or I missed it.
    // Statistics.h has: UNIQUE_MESSAGES, TOP_MESSAGES, ENTRY_RATE, LOG_LEVEL_COUNT, FIELD_VALUE_COUNT, TOP_N_FIELD_VALUES.
    // No FIND_GAPS.
    // If FIND_GAPS is not in StatisticType, I cannot map it to StatisticConfig.
    // So I leave it as is in appOptions for legacy handling.
    if(gapDurationMs > 0) appOptions.findGapsDuration = std::chrono::milliseconds(gapDurationMs);


    // Post-processing options
    if (!durationStr.empty()) {
        auto parsedDuration = Utils::parseDuration(durationStr, false);
        if (parsedDuration.has_value()) {
            appOptions.duration = parsedDuration.value();
        } else {
            return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Error parsing --duration: " + parsedDuration.error().toString()));
        }
    }

    if (appOptions.duration.has_value()) {
        if (appOptions.startTime.has_value() && !appOptions.endTime.has_value()) {
            appOptions.endTime = *appOptions.startTime + *appOptions.duration;
        } else if (!appOptions.startTime.has_value() and appOptions.endTime.has_value()) {
            appOptions.startTime = *appOptions.endTime - *appOptions.duration;
        } else if (!appOptions.startTime.has_value() && !appOptions.endTime.has_value()){
            return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Error: --duration requires either --start or --end to be specified."));
        }
    }

    if(appOptions.tailMode) appOptions.tailInterval = std::chrono::milliseconds(tailIntervalMs);

    // Logic Validation for input sources and mode compatibility

    bool stdinViaDash = (std::find(appOptions.filePaths.begin(), appOptions.filePaths.end(), "-") != appOptions.filePaths.end());

    if (stdinViaDash) {
        // If '-' is present in filePaths, set readFromStdin to true and remove '-'
        appOptions.readFromStdin = true;
        auto it = std::remove(appOptions.filePaths.begin(), appOptions.filePaths.end(), "-");
        appOptions.filePaths.erase(it, appOptions.filePaths.end());
    }

    if (appOptions.readFromStdin && !appOptions.filePaths.empty()) {
        return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Error: Cannot specify --stdin (or '-') and other file paths simultaneously."));
    }

    if (!appOptions.readFromStdin && appOptions.filePaths.empty()) {
        return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Error: No log files or --stdin provided. Please specify input sources.\n" + app.help()));
    }

    if (appOptions.tailMode && appOptions.readFromStdin) {
        return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Error: --tail mode is not compatible with --stdin."));
    }

    // Sync appOptions to settings
    settings.lineParsePattern = appOptions.lineParsePattern;
    settings.exportSettings.outputPath = appOptions.outputPath;
    if (appOptions.outputFormat == "json") {
        settings.exportSettings.format = ExportFormat::JSON;
    } else if (appOptions.outputFormat == "csv") {
        settings.exportSettings.format = ExportFormat::CSV;
    } else {
        settings.exportSettings.format = ExportFormat::PLAINTEXT;
    }
    
    settings.exportSettings.sortBy = appOptions.sortBy;
    settings.exportSettings.sortOrder = appOptions.sortOrder;
    settings.exportSettings.outputNoColor = (appOptions.colorOption == CLIConfig::ColorOption::NEVER);
    settings.exportSettings.textOutputFormat = appOptions.textOutputFormat;
    settings.exportSettings.includeSummary = appOptions.includeSummary;
    settings.exportSettings.prettyPrint = appOptions.prettyPrint;
    settings.exportSettings.csvSeparator = appOptions.csvSeparator;
    
    // Convert CSV/JSON fields to aliases
    settings.exportSettings.csvFields.clear();
    for (const auto& f : appOptions.csvFields) {
        settings.exportSettings.csvFields.push_back(parseFieldAlias(f));
    }
    
    settings.exportSettings.jsonFields.clear();
    for (const auto& f : appOptions.jsonFields) {
        settings.exportSettings.jsonFields.push_back(parseFieldAlias(f));
    }

    settings.exportSettings.topMessagesCount = appOptions.topMessagesCount;
    settings.exportSettings.streamMode = appOptions.streamMode;
    settings.exportSettings.tailMode = appOptions.tailMode;
    settings.exportSettings.tailInterval = appOptions.tailInterval;
    settings.parserErrorAction = appOptions.parserErrorAction;
    settings.logEntryStartPattern = appOptions.multilineStartPattern;
    settings.maxMultilineBufferSize = appOptions.maxMultilineBufferSize;

    return std::make_pair(settings, appOptions);
}
