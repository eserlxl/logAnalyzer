// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef CLICONFIG_H
#define CLICONFIG_H

#include "core/log/types.h" // For LogLevel
#include "filter/types.h" // For SortBy, SortOrder, FilterLogicalOperator
#include "config/settings.h" // To return a populated LogAnalyzerSettings object
#include "config/common_types.h" // For shared types and maps
#include "core/error.h"    // For Error struct and Result alias
#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <utility> // For std::pair

class CLIConfig {
public:
    using ColorOption = Config::ColorOption;
    using ParserErrorAction = ::ParserErrorAction;

    // Static maps are now in Config::CommonTypes.h
    // We can add aliases if needed for backward compatibility in this class context, 
    // but the implementation should use Config::...Map directly.
    
    struct CLIOptions { // Renamed from CLIAppOptions
        std::vector<std::string> filePaths;
        std::string lineParsePattern = std::string(DEFAULT_LOG_REGEX_PATTERN_INTERNAL); // Added to match tests
        std::vector<LogLevel> filterLevels;
        std::optional<LogLevel> minLogLevel;
        std::vector<std::string> filterKeywords;
        std::vector<std::string> excludeKeywords;
        bool keywordCaseSensitive = false;
        std::vector<std::string> regexPatterns;
        std::vector<std::string> excludeRegexPatterns;
        std::optional<filter::FilterLogicalOperator> filterLogic;
        std::optional<std::chrono::system_clock::time_point> startTime;
        std::optional<std::chrono::system_clock::time_point> endTime;
        std::optional<std::chrono::microseconds> duration;
        std::optional<filter::SortBy> sortBy;
        std::optional<filter::SortOrder> sortOrder;
        std::string outputFormat = "text";
        std::string outputPath;
        std::string textOutputFormat = "{timestamp} {level}: {message}"; // Default, matches LogAnalyzer default formatEntry
        bool includeSummary = false;
        bool prettyPrint = false;
        CLIConfig::ColorOption colorOption = CLIConfig::ColorOption::AUTO;
        char csvSeparator = ',';
        std::vector<std::string> csvFields;
        int topMessagesCount = 10;
        bool streamMode = false;
        CLIConfig::ParserErrorAction parserErrorAction = CLIConfig::ParserErrorAction::Warn; // Default to Warn
        bool tailMode = false;
        std::chrono::milliseconds tailInterval = std::chrono::milliseconds(1000);
        std::string complexFilterExpression; // New: --expression filter
        std::vector<std::string> jsonFields; // New: Configurable JSON fields

        // New parser options
        std::optional<std::string> multilineStartPattern;
        size_t maxMultilineBufferSize = 10 * 1024 * 1024; // Default 10 MiB, avoiding include
        std::vector<std::string> fieldMaps;

        // Stats options (part of IStatisticCollector refactor)
        std::vector<std::string> enabledStatistics; // New: for IStatisticCollector
        std::optional<std::chrono::seconds> statsWindow; // To be integrated into collectors
        std::optional<std::chrono::milliseconds> findGapsDuration; // To be integrated into collectors
        bool readFromStdin = false; // New: Input from stdin
        bool exitAfterParse = false; // New: To handle --help, --version
        std::string configPath; // New: Path to the configuration file
        std::optional<size_t> limit;  // --limit N: cap output at N matching entries
        std::optional<size_t> offset; // --offset N: skip first N matching entries
        bool countOnly = false;       // --count: print only the match count
        std::string statsOutputPath;  // --stats-output PATH: write stats JSON here instead of stdout
        std::string dedupField;       // --dedup-field FIELD: keep only first entry per unique FIELD value
    };

    // CLI parsing function - now returns a pair of LogAnalyzerSettings and CLIOptions
    static ErrorCode::Result<std::pair<LogAnalyzerSettings, CLIOptions>> parseCLI(int argc, const char *const *argv);
};

#endif // CLICONFIG_H
