// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "config/cli.h"
#include "config/common_types.h"
#include "config/utils.h"
#include "utils/core.h"
#include "core/error.h"
#include "core/CaseInsensitiveLess.h"
#include "stats/core.h"
#include "utils/version.h"
#include <CLI/CLI.hpp>
#include <cctype>
#include <algorithm>
#include <iostream>
#include <string_view>
#include <sstream>
#include <regex>

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "0.0.0-dev"
#endif

using namespace ErrorCode;

namespace CLIConfigHelpers {
    void trimInPlace(std::string& s);
    std::optional<StatisticConfig> parseStatisticConfig(const std::string& statStr);
    std::pair<std::string, std::string> parseFieldAlias(const std::string& fieldStr);
}

// CLI Parsing
Result<std::pair<LogAnalyzerSettings, CLIConfig::CLIOptions>> CLIConfig::parseCLI(int argc, const char *const *argv) {
    LogAnalyzerSettings settings;
    CLIOptions appOptions;
    CLI::App app{"Log Analyzer Tool"};
    app.set_version_flag("--version", PROJECT_VERSION);

    app.set_config("--config", appOptions.configPath, "Read options from a configuration file", false);

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
          if (parsedTime) {
              appOptions.startTime = *parsedTime;
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
          if (parsedTime) {
              appOptions.endTime = *parsedTime;
              return tsStr; // Return original string for CLI internal use
          }
          throw CLI::ValidationError("Internal Error: Timestamp validation passed but parsing failed for --end. This should not happen.");
       });
    
    std::string durationStr;
    app.add_option("--duration", durationStr, "Duration for time filtering (e.g., '30m', '1h')");
    std::string sinceStr;
    app.add_option("--since", sinceStr, "Show entries from the last DURATION ago (e.g. 1h, 30m, 2d); shorthand for --start 'N ago'");

    // Sorting
    app.add_option("--sort-by", appOptions.sortBy, "Sort entries by field")
       ->transform(CLI::CheckedTransformer(Config::SortByMap, CLI::ignore_case))
       ->multi_option_policy(CLI::MultiOptionPolicy::TakeLast);
    
    app.add_option("--order", appOptions.sortOrder, "Sort order")
       ->transform(CLI::CheckedTransformer(Config::SortOrderMap, CLI::ignore_case))
       ->multi_option_policy(CLI::MultiOptionPolicy::TakeLast);

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

    app.add_option("--format", appOptions.outputFormat, "Output format (text, json, ndjson, csv, xml)")
       ->transform(CLI::IsMember({"text", "json", "ndjson", "csv", "xml"}, CLI::ignore_case));
    app.add_option("--output", appOptions.outputPath, "Redirect output to a file");
    app.add_option("--text-format", appOptions.textOutputFormat, "Custom format string for text output. Available: {timestamp}, {level}, {message}, {id}, {sourceFile}, {lineNumber}, {threadId}, {module}, {host}, {customFields}.")
       ->check([](const std::string &str) -> std::string {
           long open_braces = std::count(str.begin(), str.end(), '{');
           long close_braces = std::count(str.begin(), str.end(), '}');
           if (open_braces != close_braces) {
               return "Mismatched braces in format string.";
           }
           return "";
       });
    
    app.add_flag("--include-summary", appOptions.includeSummary, "Include summary in JSON output");
    app.add_flag("--pretty", appOptions.prettyPrint, "Pretty-print JSON output");

    app.add_option("--color", appOptions.colorOption, "Control output color (always, auto, never)")
       ->transform(CLI::CheckedTransformer(Config::ColorOptionMap, CLI::ignore_case));

    app.add_option("--csv-sep", appOptions.csvSeparator, "Custom separator for CSV output (defaults to ',')");
    
    // CSV and JSON Fields
    app.add_option("--csv-fields", appOptions.csvFields, "Ordered list of fields for CSV output (e.g., 'timestamp as Time, level, message')")
       ->delimiter(',')
       ->each([](const std::string& f_raw) { // Throws on error
           std::string f = f_raw;
           CLIConfigHelpers::trimInPlace(f);
           if (f.empty()) {
               throw CLI::ValidationError("Invalid --csv-fields: list contains an empty element.");
           }
           auto parsed = CLIConfigHelpers::parseFieldAlias(f);
           if (parsed.first.empty()) {
                throw CLI::ValidationError("Invalid --csv-fields entry: field '" + f_raw + "' has an empty name in an alias expression.");
           }
       });
    app.add_option("--json-fields", appOptions.jsonFields, "Ordered list of fields for JSON output (e.g., 'timestamp as time, log_level as level')")
       ->delimiter(',')
       ->each([](const std::string& f_raw) { // Throws on error
           std::string f = f_raw;
           CLIConfigHelpers::trimInPlace(f);
           if (f.empty()) {
               throw CLI::ValidationError("Invalid --json-fields: list contains an empty element.");
           }
           auto parsed = CLIConfigHelpers::parseFieldAlias(f);
           if (parsed.first.empty()) {
                throw CLI::ValidationError("Invalid --json-fields entry: field '" + f_raw + "' has an empty name in an alias expression.");
           }
       });

    // Analysis Options
    app.add_flag("--stdin", appOptions.readFromStdin, "Read log entries from standard input (stdin) if no file paths are provided.");
    
    app.add_option("--stats", appOptions.enabledStatistics, "Enable statistics collectors (e.g., unique_messages, 'type=TOP_MESSAGES,top_n=5')");
    // Delimiter removed to support complex strings with commas.
    // Multiple stats should be provided via multiple --stats flags.

    app.add_option("--stats-output", appOptions.statsOutputPath, "Write statistics JSON to this file instead of stdout");

    app.add_option("--dedup-field", appOptions.dedupField, "Keep only the first entry per unique value of FIELD (standard field: level, message, source; or a custom field name)");

    app.add_option("--stats-interval", appOptions.statsInterval, "In stream mode: emit a partial stats report every N matching entries (N must be > 0)")
       ->check(CLI::PositiveNumber);

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
            CLIConfigHelpers::trimInPlace(from);
            CLIConfigHelpers::trimInPlace(to);
            if (from.empty() || to.empty()) {
                throw CLI::ValidationError("Invalid KEY=VALUE format for --map-level");
            }
            std::string toUpper = to;
            std::transform(toUpper.begin(), toUpper.end(), toUpper.begin(), [](unsigned char c) {
                return static_cast<char>(std::toupper(c));
            });
            
            if(Config::LogLevelMap.count(toUpper)) {
                settings.customLogLevelMappings[from] = Config::LogLevelMap.at(toUpper);
            } else {
                throw CLI::ValidationError("Invalid log level in --map-level: " + to);
            }
        }
    }, "Map custom log levels (KEY=LEVEL)");

    // Stats (Legacy options)
    int statsWindowSec = 0;
    app.add_option("--stats-window", statsWindowSec, "Show log frequency distribution over a time window (seconds) (Deprecated)")
       ->check(CLI::PositiveNumber);
    
    int gapDurationMs = 0;
    app.add_option("--find-gaps", gapDurationMs, "Find time gaps longer than X ms (Deprecated)")
       ->check(CLI::PositiveNumber);

    // New options from Iteration 7 Design
    app.add_option("--on-parse-error", appOptions.parserErrorAction, "Action on parse error (skip, log, fail)")
       ->transform(CLI::CheckedTransformer(Config::ParserErrorActionMap, CLI::ignore_case));

    app.add_flag("--tail", appOptions.tailMode, "Enable tail mode to monitor files for new lines");
    int tailIntervalMs = 1000;
    app.add_option("--tail-interval", tailIntervalMs, "Polling interval for tail mode in ms (default: 1000)")
       ->check(CLI::PositiveNumber);

    app.add_option("--expression", appOptions.complexFilterExpression, "Complex filter expression");

    app.add_option("--limit", appOptions.limit, "Stop after N matching entries")
       ->check(CLI::PositiveNumber);
    app.add_option("--offset", appOptions.offset, "Skip first N matching entries")
       ->check(CLI::NonNegativeNumber);
    app.add_flag("--count", appOptions.countOnly, "Print only the count of matching entries, then exit");


    try {
        app.parse(argc, argv);
    } catch (const CLI::CallForHelp &e) {
        appOptions.exitAfterParse = true;
        std::cout << app.help() << '\n';
        return std::make_pair(settings, appOptions);
    } catch (const CLI::CallForVersion &e) {
        appOptions.exitAfterParse = true;
        std::cout << app.version() << '\n';
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
                      << appOptions.topMessagesCount << "\" instead." << '\n';
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
        auto statConfig = CLIConfigHelpers::parseStatisticConfig(statStr);
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
        std::cerr << "Warning: --stats-window is deprecated and has no effect; use --stats entry_rate instead.\n";
        StatisticConfig sc;
        sc.type = StatisticType::ENTRY_RATE; // Assuming this maps to entry rate over window?
        // Actually ENTRY_RATE usually implies a window.
        sc.params["window"] = std::to_string(statsWindowSec) + "s";
        settings.statisticConfigs.push_back(sc);
        appOptions.statsWindow = std::chrono::seconds(statsWindowSec); // Keep for legacy compatibility if used elsewhere
    }
    
    if (gapDurationMs > 0) {
        appOptions.findGapsDuration = std::chrono::milliseconds(gapDurationMs);
        StatisticConfig sc;
        sc.type = StatisticType::FIND_GAPS;
        sc.params["threshold_ms"] = std::to_string(gapDurationMs);
        settings.statisticConfigs.push_back(sc);
    }


    // Post-processing options
    if (!durationStr.empty()) {
        auto parsedDuration = Utils::parseDuration(durationStr, false);
        if (parsedDuration) {
            appOptions.duration = *parsedDuration;
        } else {
            return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Error parsing --duration: " + parsedDuration.error().toString()));
        }
    }

    if (appOptions.duration.has_value()) {
        if (appOptions.startTime.has_value() && !appOptions.endTime.has_value()) {
            appOptions.endTime = *appOptions.startTime + *appOptions.duration;
        } else if (!appOptions.startTime.has_value() && appOptions.endTime.has_value()) {
            appOptions.startTime = *appOptions.endTime - *appOptions.duration;
        } else if (!appOptions.startTime.has_value() && !appOptions.endTime.has_value()){
            return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Error: --duration requires either --start or --end to be specified."));
        }
    }

    if (!sinceStr.empty()) {
        if (appOptions.startTime.has_value()) {
            return std::unexpected(ErrorCode::Error(::Code::InvalidCLIOption, "--since and --start cannot be used together."));
        }
        auto parsedSince = Utils::parseDuration(sinceStr, false);
        if (!parsedSince) {
            return std::unexpected(ErrorCode::Error(::Code::InvalidCLIOption, "Error parsing --since: " + parsedSince.error().toString()));
        }
        appOptions.startTime = std::chrono::system_clock::now() - *parsedSince;
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
        return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "No input files provided. Use '-' for stdin or specify file paths."));
    }

    if (appOptions.tailMode && appOptions.readFromStdin) {
        // If it was automatically set to readFromStdin, but tailMode is on, and no files were provided, we have an error.
        // But if the user explicitly provided --stdin and --tail, it's also an error.
        if (app.count("--stdin") || stdinViaDash) {
            return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Error: --tail mode is not compatible with --stdin."));
        } else {
            return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Error: --tail mode requires file paths and is not compatible with stdin."));
        }
    }

    // Sync appOptions to settings
    if (app.count("--pattern")) {
        settings.lineParsePattern = appOptions.lineParsePattern;
    }

    if (app.count("--output")) {
        settings.exportSettings.outputPath = appOptions.outputPath;
    }
    
    if (app.count("--format")) {
        if (appOptions.outputFormat == "json") {
            settings.exportSettings.format = ExportFormat::JSON;
        } else if (appOptions.outputFormat == "csv") {
            settings.exportSettings.format = ExportFormat::CSV;
        } else if (appOptions.outputFormat == "ndjson") {
            settings.exportSettings.format = ExportFormat::NDJSON;
        } else if (appOptions.outputFormat == "xml") {
            settings.exportSettings.format = ExportFormat::XML;
        } else if (appOptions.outputFormat == "text") {
            settings.exportSettings.format = ExportFormat::PLAINTEXT;
        }
    } else {
        // Default to PLAINTEXT if no format is specified
        settings.exportSettings.format = ExportFormat::PLAINTEXT;
    }
    
    if (app.count("--sort-by")) {
        settings.exportSettings.sortBy = appOptions.sortBy;
    }
    if (app.count("--order")) {
        settings.exportSettings.sortOrder = appOptions.sortOrder;
    }
    if (app.count("--color")) {
        settings.exportSettings.outputNoColor = (appOptions.colorOption == CLIConfig::ColorOption::NEVER);
    }
    if (app.count("--text-format")) {
        settings.exportSettings.textOutputFormat = appOptions.textOutputFormat;
    }
    if (app.count("--include-summary")) {
        settings.exportSettings.includeSummary = appOptions.includeSummary;
    }
    if (app.count("--pretty")) {
        settings.exportSettings.prettyPrint = appOptions.prettyPrint;
    }
    if (app.count("--csv-sep")) {
        settings.exportSettings.csvSeparator = appOptions.csvSeparator;
    }
    
    // Convert CSV/JSON fields to aliases
    if (app.count("--csv-fields")) {
        settings.exportSettings.csvFields.clear();
        for (const auto& f : appOptions.csvFields) {
            auto parsed = CLIConfigHelpers::parseFieldAlias(f);
            if (parsed.first.empty()) {
                return std::unexpected(ErrorCode::Error(::Code::InvalidCLIOption, "Invalid --csv-fields entry: field name cannot be empty."));
            }
            settings.exportSettings.csvFields.push_back(std::move(parsed));
        }
    }
    
    if (app.count("--json-fields")) {
        settings.exportSettings.jsonFields.clear();
        for (const auto& f : appOptions.jsonFields) {
            auto parsed = CLIConfigHelpers::parseFieldAlias(f);
            if (parsed.first.empty()) {
                return std::unexpected(ErrorCode::Error(::Code::InvalidCLIOption, "Invalid --json-fields entry: field name cannot be empty."));
            }
            settings.exportSettings.jsonFields.push_back(std::move(parsed));
        }
    }

    if (app.count("--top-n")) {
        settings.exportSettings.topMessagesCount = appOptions.topMessagesCount;
    }
    if (app.count("--stream")) {
        settings.exportSettings.streamMode = appOptions.streamMode;
    }
    if (app.count("--tail")) {
        settings.exportSettings.tailMode = appOptions.tailMode;
    }
    if (app.count("--tail-interval")) {
        settings.exportSettings.tailInterval = appOptions.tailInterval;
    }

    if (app.count("--on-parse-error")) {
        settings.parserErrorAction = appOptions.parserErrorAction;
    }
    
    if (app.count("--multiline-start-pattern")) {
        settings.logEntryStartPattern = appOptions.multilineStartPattern;
    }
    
    if (app.count("--max-multiline-buffer")) {
        settings.maxMultilineBufferSize = appOptions.maxMultilineBufferSize;
    }

    return std::make_pair(settings, appOptions);
}
