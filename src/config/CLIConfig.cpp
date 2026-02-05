#include "config/CLIConfig.h"
#include "utils/Core.h"
#include "core/Error.h" // Add this include
#include "core/CiLess.h" // For ci_less
#include <CLI/CLI.hpp>
#include <algorithm> // For std::transform
#include <iostream> // For std::cerr
#include <string_view>

using namespace ErrorCode;

// Helper function to parse field map strings
ErrorCode::Result<std::vector<FieldMapping>> parseFieldMaps(const std::vector<std::string>& fieldMapStrings) {
    std::vector<FieldMapping> mappings;
    std::map<std::string, LogEntryField, LogAnalyzerInternal::ci_less> standardFieldMap = {
        {"timestamp", LogEntryField::TIMESTAMP},
        {"level", LogEntryField::LEVEL},
        {"message", LogEntryField::MESSAGE},
        {"source_file", LogEntryField::SOURCE_FILE},
        {"structured_field", LogEntryField::STRUCTURED_FIELD}
    };

    for (const auto& s : fieldMapStrings) {
        auto pos = s.find('=');
        if (pos == std::string::npos) {
            return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Invalid field map format: '" + s + "'. Expected format is 'group=field[:format]'."));
        }

        std::string groupStr = s.substr(0, pos);
        std::string rest = s.substr(pos + 1);

        size_t groupIndex;
        try {
            groupIndex = std::stoul(groupStr);
        } catch (const std::invalid_argument& e) {
            return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Invalid group index in field map: '" + groupStr + "'."));
        }

        std::string fieldName;
        std::string format;
        auto formatPos = rest.find(':');
        if (formatPos != std::string::npos) {
            fieldName = rest.substr(0, formatPos);
            format = rest.substr(formatPos + 1);
        } else {
            fieldName = rest;
        }

        if (auto it = standardFieldMap.find(fieldName); it != standardFieldMap.end()) {
            if (!format.empty()) {
                mappings.emplace_back(it->second, groupIndex, format);
            } else {
                mappings.emplace_back(it->second, groupIndex);
            }
        } else {
            // Custom field
            if (!format.empty()) {
                mappings.emplace_back(fieldName, groupIndex, std::vector<std::string>{format});
            } else {
                mappings.emplace_back(fieldName, groupIndex, std::vector<std::string>{});
            }
        }
    }

    return mappings;
}


// CLI Parsing
Result<std::pair<LogAnalyzerSettings, CLIConfig::CLIOptions>> CLIConfig::parseCLI(int argc, const char *const *argv) {
    LogAnalyzerSettings settings;
    CLIOptions appOptions;
    CLI::App app{"Log Analyzer Tool"};

    app.set_config("--config", "", "Read options from a configuration file", false);

    // Positional Arguments
    app.add_option("log_files", appOptions.filePaths, "Path to log files or '-' for stdin");

    // Filters
    app.add_option("--level", appOptions.filterLevels, "Filter by log levels (e.g., ERROR,WARNING)")
       ->transform(CLI::CheckedTransformer(CLIConfig::levelMap, CLI::ignore_case));
       
    app.add_option("--min-level", appOptions.minLogLevel, "Filter entries with level greater than or equal to a specified level")
       ->transform(CLI::CheckedTransformer(CLIConfig::levelMap, CLI::ignore_case));

    app.add_option("--keyword", appOptions.filterKeywords, "Filter messages containing specific text");
    app.add_option("--exclude-keyword", appOptions.excludeKeywords, "Exclude log entries containing a specific keyword");
       
    app.add_flag("--case-sensitive", appOptions.keywordCaseSensitive, "Make keyword filter case-sensitive");
    
    app.add_option("--regex", appOptions.regexPatterns, "Filter messages using regex");
    app.add_option("--exclude-regex", appOptions.excludeRegexPatterns, "Filter out log entries matching a specific regular expression");

    app.add_option("--logic", appOptions.filterLogic, "Logic to combine multiple filters of the same type (AND or OR)")
       ->transform(CLI::CheckedTransformer(CLIConfig::logicMap, CLI::ignore_case));

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
       ->transform(CLI::CheckedTransformer(CLIConfig::sortMap, CLI::ignore_case));
    
    app.add_option("--order", appOptions.sortOrder, "Sort order")
       ->transform(CLI::CheckedTransformer(CLIConfig::orderMap, CLI::ignore_case));

    // Output Configuration
    app.add_option("--pattern", appOptions.lineParsePattern, "Custom regex for parsing log lines");
    app.add_option("--multiline-start-pattern", appOptions.multilineStartPattern, "Regex to identify the start of a multi-line log entry");
    app.add_option("--max-multiline-buffer", appOptions.maxMultilineBufferSize, "Max buffer size for multi-line entries in bytes (default: 10MB)");
    app.add_option("--field-map", appOptions.fieldMaps, "Map regex capture group to a field (e.g., '1=timestamp:%Y-%m-%d %H:%M:%S')");

    app.add_option("--format", appOptions.outputFormat, "Output format (text, json, csv)")
       ->transform(CLI::IsMember({"text", "json", "csv"}, CLI::ignore_case));
    app.add_option("--output", appOptions.outputPath, "Redirect output to a file");
    app.add_option("--text-format", appOptions.textOutputFormat, "Custom format string for text output. Available: {timestamp}, {level}, {message}, {lineNumber}, {fileName}, {elapsedTime}.");
    
    app.add_flag("--include-summary", appOptions.includeSummary, "Include summary in JSON output");
    app.add_flag("--pretty", appOptions.prettyPrint, "Pretty-print JSON output");

    app.add_option("--color", appOptions.colorOption, "Control output color (always, auto, never)")
       ->transform(CLI::CheckedTransformer(CLIConfig::colorOptionMap, CLI::ignore_case));

    app.add_option("--csv-sep", appOptions.csvSeparator, "Custom separator for CSV output (defaults to ',')");
    app.add_option("--csv-fields", appOptions.csvFields, "Ordered list of fields for CSV output (e.g., timestamp,level,message)")
       ->delimiter(',');
    app.add_option("--json-fields", appOptions.jsonFields, "Ordered list of fields for JSON output (e.g., timestamp,level,message)")
       ->delimiter(',');

    // Analysis Options
    app.add_flag("--stdin", appOptions.readFromStdin, "Read log entries from standard input (stdin) if no file paths are provided.");
    
    app.add_option("--stats", appOptions.enabledStatistics, "Enable statistics collectors (e.g., unique_messages,top_messages:10)")
       ->delimiter(',');

    app.add_option("--top-n", appOptions.topMessagesCount, "Number of top messages to show for top_messages statistic")
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
            
            if(CLIConfig::levelMap.count(toUpper)) {
                settings.customLogLevelMappings[from] = CLIConfig::levelMap.at(toUpper);
            } else {
                throw CLI::ValidationError("Invalid log level in --map-level: " + to);
            }
        }
    }, "Map custom log levels (KEY=LEVEL)");

    // Stats
    int statsWindowSec = 0;
    app.add_option("--stats-window", statsWindowSec, "Show log frequency distribution over a time window (seconds)");
    
    int gapDurationMs = 0;
    app.add_option("--find-gaps", gapDurationMs, "Find time gaps longer than X ms");

    // New options from Iteration 7 Design
    app.add_option("--on-parse-error", appOptions.parserErrorAction, "Action on parse error (skip, log, fail)")
       ->transform(CLI::CheckedTransformer(CLIConfig::errorActionMap, CLI::ignore_case));

    app.add_flag("--tail", appOptions.tailMode, "Enable tail mode to monitor files for new lines");
    int tailIntervalMs = 1000;
    app.add_option("--tail-interval", tailIntervalMs, "Polling interval for tail mode in ms (default: 1000)");

    app.add_option("--expression", appOptions.complexFilterExpression, "Complex filter expression");


    try {
        app.parse(argc, argv);
    } catch (const CLI::Error &e) {
        std::stringstream ss;
        app.exit(e, ss, ss);
        return std::unexpected(ErrorCode::Error(::Code::InvalidCLIOption, ss.str()));
    }

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

    if(statsWindowSec > 0) appOptions.statsWindow = std::chrono::seconds(statsWindowSec);
    if(gapDurationMs > 0) appOptions.findGapsDuration = std::chrono::milliseconds(gapDurationMs);
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
        settings.exportSettings.format = ExportFormat::TEXT;
    }
    
    settings.exportSettings.sortBy = appOptions.sortBy;
    settings.exportSettings.sortOrder = appOptions.sortOrder;
    settings.exportSettings.outputNoColor = (appOptions.colorOption == CLIConfig::ColorOption::NEVER);
    settings.exportSettings.textOutputFormat = appOptions.textOutputFormat;
    settings.exportSettings.includeSummary = appOptions.includeSummary;
    settings.exportSettings.prettyPrint = appOptions.prettyPrint;
    settings.exportSettings.csvSeparator = appOptions.csvSeparator;
    settings.exportSettings.csvFields = appOptions.csvFields;
    settings.exportSettings.jsonFields = appOptions.jsonFields;
    settings.exportSettings.topMessagesCount = appOptions.topMessagesCount;
    settings.exportSettings.streamMode = appOptions.streamMode;
    settings.exportSettings.tailMode = appOptions.tailMode;
    settings.exportSettings.tailInterval = appOptions.tailInterval;
    settings.parserErrorAction = appOptions.parserErrorAction;
    settings.logEntryStartPattern = appOptions.multilineStartPattern;
    settings.maxMultilineBufferSize = appOptions.maxMultilineBufferSize;

    return std::make_pair(settings, appOptions);
}
