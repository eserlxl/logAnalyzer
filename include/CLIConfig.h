#ifndef CLICONFIG_H
#define CLICONFIG_H

#include "LogTypes.h" // For LogLevel, SortBy, SortOrder
#include "Filter.h"   // For CompositeFilter::Logic
#include "LogAnalyzerSettings.h" // To return a populated LogAnalyzerSettings object
#include <string>
#include <vector>
#include <map>
#include <expected>
#include <chrono>
#include <optional>
#include <utility> // For std::pair

class CLIConfig {
public:
    // Nested enums from original LogAnalyzerConfig
    enum class ColorOption {
        ALWAYS, AUTO, NEVER
    };

    enum class ParserErrorAction {
        SKIP_LINE, LOG_AND_SKIP, FAIL
    };

    // Static maps for CLI argument parsing
    static const std::map<std::string, LogLevel> levelMap;
    static const std::map<std::string, CompositeFilter::Logic> logicMap;
    static const std::map<std::string, SortBy> sortMap;
    static const std::map<std::string, SortOrder> orderMap;
    static const std::map<std::string, ColorOption> colorOptionMap;
    static const std::map<std::string, ParserErrorAction> errorActionMap;

    struct CLIAppOptions {
        std::vector<std::string> filePaths;
        std::vector<LogLevel> filterLevels;
        std::optional<LogLevel> minLogLevel;
        std::vector<std::string> filterKeywords;
        std::vector<std::string> excludeKeywords;
        bool keywordCaseSensitive = false;
        std::vector<std::string> regexPatterns;
        std::vector<std::string> excludeRegexPatterns;
        std::optional<CompositeFilter::Logic> filterLogic;
        std::optional<std::chrono::system_clock::time_point> startTime;
        std::optional<std::chrono::system_clock::time_point> endTime;
        std::optional<std::chrono::seconds> duration; // Store as seconds after parsing
        std::optional<SortBy> sortBy;
        std::optional<SortOrder> sortOrder;
        std::string outputFormat = "text";
        std::string outputPath;
        std::string textOutputFormat = "{timestamp} {level}: {message}"; // Default, matches LogAnalyzer default formatEntry
        bool includeSummary = false;
        bool prettyPrint = false;
        CLIConfig::ColorOption colorOption = CLIConfig::ColorOption::AUTO;
        char csvSeparator = ',';
        std::vector<std::string> csvFields;
        bool showUniqueMessages = false;
        bool showTopMessages = false;
        int topMessagesCount = 10;
        bool streamMode = false;
        CLIConfig::ParserErrorAction parserErrorAction = CLIConfig::ParserErrorAction::LOG_AND_SKIP;
        bool tailMode = false;
        std::chrono::milliseconds tailInterval = std::chrono::milliseconds(1000);
        std::string complexFilterExpression;

        // Stats options
        std::optional<std::chrono::seconds> statsWindow;
        std::optional<std::chrono::milliseconds> findGapsDuration;
        bool showEntryRate = false;
    };

    // CLI parsing function - now returns a pair of LogAnalyzerSettings and CLIAppOptions
    static std::expected<std::pair<LogAnalyzerSettings, CLIAppOptions>, std::string> parseCLI(int argc, char *argv[]);
};

#endif // CLICONFIG_H
