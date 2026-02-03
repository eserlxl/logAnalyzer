#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <optional>
#include <map> // For level map
#include <expected> // For std::expected in parseCLI
#include "LogTypes.h" // For LogLevel, SortBy, SortOrder
#include "Filter.h"   // For CompositeFilter::Logic

// Forward declaration for CLI::App
namespace CLI { class App; }

class LogAnalyzerConfig {
public:
    // Input
    std::vector<std::string> filePaths;
    std::string customParserPattern; // Renamed for clarity from customPattern

    // Filtering
    std::vector<LogLevel> filterLevels;
    std::optional<LogLevel> minLogLevel;
    std::vector<std::string> filterKeywords;
    std::vector<std::string> excludeKeywords;
    bool keywordCaseSensitive = false;
    std::vector<std::string> regexPatterns;
    std::vector<std::string> excludeRegexPatterns;
    std::optional<std::chrono::system_clock::time_point> startTime;
    std::optional<std::chrono::system_clock::time_point> endTime;
    std::optional<std::chrono::seconds> duration; // Parsed from string
    CompositeFilter::Logic filterLogic = CompositeFilter::Logic::AND; // Renamed from defaultFilterLogic for consistency

    // Advanced Filtering (New)
    std::string complexFilterExpression; // e.g., "(level=ERROR OR keyword=critical) AND regex=..."

    // Sorting
    SortBy sortBy = SortBy::TIMESTAMP;
    SortOrder sortOrder = SortOrder::ASCENDING;

    // Output
    std::string outputFormat = "text";
    std::string outputPath;
    bool includeSummary = false;
    bool prettyPrint = false;
    std::string textOutputFormat = "{timestamp} [{level}] {message}";
    enum class ColorOption { ALWAYS, AUTO, NEVER };
    ColorOption colorOption = ColorOption::AUTO;
    char csvSeparator = ',';
    std::vector<std::string> csvFields; // New: ordered list of fields for CSV
    
    // Analysis Options
    bool showUniqueMessages = false;
    bool showTopMessages = false;
    int topMessagesCount = 10;
    bool streamMode = false;
    std::vector<std::pair<std::string, LogLevel>> customLogLevelMappings;
    std::optional<std::chrono::seconds> statsWindow;
    std::optional<std::chrono::milliseconds> findGapsDuration;
    bool showEntryRate = false;
    
    // New: Configurable Error Handling
    enum class ParserErrorAction { SKIP_LINE, LOG_AND_SKIP, FAIL };
    ParserErrorAction parserErrorAction = ParserErrorAction::LOG_AND_SKIP;

    // New: Real-time/Interactive Mode
    bool tailMode = false;
    std::optional<std::chrono::milliseconds> tailInterval; // Polling interval for tail mode

    // CLI config file
    std::string configFile;

    // Constructor
    LogAnalyzerConfig();

    // Static method for CLI parsing
    static std::expected<LogAnalyzerConfig, std::string> parseCLI(int argc, char *argv[]);

    // Helper maps for CLI parsing
    static const std::map<std::string, LogLevel> levelMap;
    static const std::map<std::string, CompositeFilter::Logic> logicMap;
    static const std::map<std::string, SortBy> sortMap;
    static const std::map<std::string, SortOrder> orderMap;
    static const std::map<std::string, ColorOption> colorOptionMap;
    static const std::map<std::string, ParserErrorAction> errorActionMap;
};
