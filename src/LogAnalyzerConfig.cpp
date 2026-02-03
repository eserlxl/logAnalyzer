#include "LogAnalyzerConfig.h"
#include "Utils.h"
#include <CLI/CLI.hpp>
#include <algorithm> // For std::transform
#include <iostream> // For std::cerr

// Define static maps
const std::map<std::string, LogLevel> LogAnalyzerConfig::levelMap = {
    {"DEBUG", LogLevel::DEBUG}, {"INFO", LogLevel::INFO},
    {"WARNING", LogLevel::WARNING}, {"ERROR", LogLevel::ERROR},
    {"UNKNOWN", LogLevel::UNKNOWN}, {"TRACE", LogLevel::TRACE}, {"FATAL", LogLevel::FATAL}
};

const std::map<std::string, CompositeFilter::Logic> LogAnalyzerConfig::logicMap = {
    {"AND", CompositeFilter::Logic::AND}, {"OR", CompositeFilter::Logic::OR}
};

const std::map<std::string, SortBy> LogAnalyzerConfig::sortMap = {
    {"time", SortBy::TIMESTAMP}, {"level", SortBy::LEVEL}, {"msg", SortBy::MESSAGE}
};

const std::map<std::string, SortOrder> LogAnalyzerConfig::orderMap = {
    {"asc", SortOrder::ASCENDING}, {"desc", SortOrder::DESCENDING}
};

const std::map<std::string, LogAnalyzerConfig::ColorOption> LogAnalyzerConfig::colorOptionMap = {
    {"always", LogAnalyzerConfig::ColorOption::ALWAYS}, {"auto", LogAnalyzerConfig::ColorOption::AUTO}, {"never", LogAnalyzerConfig::ColorOption::NEVER}
};

const std::map<std::string, LogAnalyzerConfig::ParserErrorAction> LogAnalyzerConfig::errorActionMap = {
    {"skip", ParserErrorAction::SKIP_LINE}, {"log", ParserErrorAction::LOG_AND_SKIP}, {"fail", ParserErrorAction::FAIL}
};

// Constructor definition
LogAnalyzerConfig::LogAnalyzerConfig() = default;

// CLI Parsing
std::expected<LogAnalyzerConfig, std::string> LogAnalyzerConfig::parseCLI(int argc, char *argv[]) {
    LogAnalyzerConfig config;
    CLI::App app{"Log Analyzer Tool"};

    app.set_config("--config", "", "Read options from a configuration file", false);

    // Positional Arguments
    app.add_option("log_files", config.filePaths, "Path to log files or '-' for stdin")
       ->check(CLI::ExistingFile | CLI::IsMember({"-"}));

    // Filters
    app.add_option("--level", config.filterLevels, "Filter by log levels (e.g., ERROR,WARNING)")
       ->transform(CLI::CheckedTransformer(LogAnalyzerConfig::levelMap, CLI::ignore_case));
       
    app.add_option("--min-level", config.minLogLevel, "Filter entries with level greater than or equal to a specified level")
       ->transform(CLI::CheckedTransformer(LogAnalyzerConfig::levelMap, CLI::ignore_case));

    app.add_option("--keyword", config.filterKeywords, "Filter messages containing specific text");
    app.add_option("--exclude-keyword", config.excludeKeywords, "Exclude log entries containing a specific keyword");
       
    app.add_flag("--case-sensitive", config.keywordCaseSensitive, "Make keyword filter case-sensitive");
    
    app.add_option("--regex", config.regexPatterns, "Filter messages using regex");
    app.add_option("--exclude-regex", config.excludeRegexPatterns, "Filter out log entries matching a specific regular expression");

    app.add_option("--logic", config.filterLogic, "Logic to combine multiple filters of the same type (AND or OR)")
       ->transform(CLI::CheckedTransformer(LogAnalyzerConfig::logicMap, CLI::ignore_case));

    app.add_option("--start", "Start time filter (YYYY-MM-DD HH:MM:SS, ISO 8601, Unix timestamp, or relative like '1h ago')")
       ->check(Utils::validateTimestampCliOption)
       ->transform([&](const std::string& tsStr){
          auto parsedTime = Utils::parseTime(tsStr);
          if (parsedTime.has_value()) {
              config.startTime = parsedTime.value();
              return tsStr; // Return original string for CLI internal use
          }
          throw CLI::ValidationError("Internal Error: Timestamp validation passed but parsing failed for --start. This should not happen.");
       });
    app.add_option("--end", "End time filter (YYYY-MM-DD HH:MM:SS, ISO 8601, Unix timestamp, or relative like '1h ago')")
       ->check(Utils::validateTimestampCliOption)
       ->transform([&](const std::string& tsStr){
          auto parsedTime = Utils::parseTime(tsStr);
          if (parsedTime.has_value()) {
              config.endTime = parsedTime.value();
              return tsStr; // Return original string for CLI internal use
          }
          throw CLI::ValidationError("Internal Error: Timestamp validation passed but parsing failed for --end. This should not happen.");
       });
    
    std::string durationStr;
    app.add_option("--duration", durationStr, "Duration for time filtering (e.g., '30m', '1h')");

    // Sorting
    app.add_option("--sort-by", config.sortBy, "Sort entries by field")
       ->transform(CLI::CheckedTransformer(LogAnalyzerConfig::sortMap, CLI::ignore_case));
    
    app.add_option("--order", config.sortOrder, "Sort order")
       ->transform(CLI::CheckedTransformer(LogAnalyzerConfig::orderMap, CLI::ignore_case));

    // Output Configuration
    app.add_option("--pattern", config.customParserPattern, "Custom regex for parsing log lines");
    app.add_option("--format", config.outputFormat, "Output format (text, json, csv)")
       ->check(CLI::IsMember({"text", "json", "csv"}));
    app.add_option("--output", config.outputPath, "Redirect output to a file");
    app.add_option("--text-format", config.textOutputFormat, "Custom format string for text output. Available: {timestamp}, {level}, {message}, {lineNumber}, {fileName}, {elapsedTime}.");
    
    app.add_flag("--include-summary", config.includeSummary, "Include summary in JSON output");
    app.add_flag("--pretty", config.prettyPrint, "Pretty-print JSON output");

    app.add_option("--color", config.colorOption, "Control output color (always, auto, never)")
       ->transform(CLI::CheckedTransformer(LogAnalyzerConfig::colorOptionMap, CLI::ignore_case));

    app.add_option("--csv-sep", config.csvSeparator, "Custom separator for CSV output (defaults to ',')");
    app.add_option("--csv-fields", config.csvFields, "Ordered list of fields for CSV output (e.g., timestamp,level,message)");

    // Analysis Options
    app.add_flag("--unique-messages", config.showUniqueMessages, "Show counts of unique messages");
    
    auto *topMsgOpt = app.add_flag("--top-messages", config.showTopMessages, "Show top N most frequent messages");
    app.add_option("top_n", config.topMessagesCount, "Number of top messages to show")
       ->needs(topMsgOpt);

    app.add_flag("--stream", config.streamMode, "Enable streaming mode for large files");

    // Custom Level Mapping
    app.add_option_function<std::vector<std::string>>("--map-level", [&](const std::vector<std::string>& val){
        for(const auto& s : val) {
            auto pos = s.find('=');
            if(pos == std::string::npos) throw CLI::ValidationError("Invalid KEY=VALUE format for --map-level");
            std::string from = s.substr(0, pos);
            std::string to = s.substr(pos + 1);
            std::string toUpper = to;
            std::transform(toUpper.begin(), toUpper.end(), toUpper.begin(), ::toupper);
            
            if(LogAnalyzerConfig::levelMap.count(toUpper)) {
                config.customLogLevelMappings.push_back({from, LogAnalyzerConfig::levelMap.at(toUpper)});
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

    app.add_flag("--rate", config.showEntryRate, "Calculate and show average log entry rate");

    // New options from Iteration 7 Design
    app.add_option("--on-parse-error", config.parserErrorAction, "Action on parse error (skip, log, fail)")
       ->transform(CLI::CheckedTransformer(LogAnalyzerConfig::errorActionMap, CLI::ignore_case));

    app.add_flag("--tail", config.tailMode, "Enable tail mode to monitor files for new lines");
    int tailIntervalMs = 1000;
    app.add_option("--tail-interval", tailIntervalMs, "Polling interval for tail mode in ms (default: 1000)");

    app.add_option("--expression", config.complexFilterExpression, "Complex filter expression");


    try {
        app.parse(argc, argv);
    } catch (const CLI::Error &e) {
        std::stringstream ss;
        app.exit(e, ss, ss);
        return std::unexpected(ss.str());
    }

    // Post-processing options
    if (!durationStr.empty()) {
        auto parsedDuration = Utils::parseDuration(durationStr);
        if (parsedDuration.has_value()) {
            config.duration = parsedDuration.value();
        } else {
            return std::unexpected("Error parsing --duration: " + parsedDuration.error());
        }
    }

    if (config.duration.has_value()) {
        if (config.startTime.has_value() && !config.endTime.has_value()) {
            config.endTime = *config.startTime + *config.duration;
        } else if (!config.startTime.has_value() && config.endTime.has_value()) {
            config.startTime = *config.endTime - *config.duration;
        } else if (!config.startTime.has_value() && !config.endTime.has_value()){
            return std::unexpected("Error: --duration requires either --start or --end to be specified.");
        }
    }

    if(statsWindowSec > 0) config.statsWindow = std::chrono::seconds(statsWindowSec);
    if(gapDurationMs > 0) config.findGapsDuration = std::chrono::milliseconds(gapDurationMs);
    if(config.tailMode) config.tailInterval = std::chrono::milliseconds(tailIntervalMs);

    // Logic Validation
    if (config.streamMode && (config.showUniqueMessages || config.showTopMessages)) {
        return std::unexpected("Error: --stream is incompatible with --unique-messages or --top-messages.");
    }
    if (config.filePaths.empty()) {
        return std::unexpected("Error: No log files provided. Use '-' for stdin or provide file paths.\n" + app.help());
    }
    if (config.tailMode && (std::find(config.filePaths.begin(), config.filePaths.end(), "-") != config.filePaths.end())) {
        return std::unexpected("Error: --tail mode is not compatible with stdin ('-').");
    }

    return config;
}
