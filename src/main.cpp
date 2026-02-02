#include "LogAnalyzer.h"
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm> // For std::transform
#include <chrono>    // For std::chrono::system_clock::time_point
#include <iomanip>   // For std::get_time
#include <optional>  // For std::optional

// Helper function to split a string by a delimiter
std::vector<std::string> splitString(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// Helper to parse timestamp from CLI
std::optional<std::chrono::system_clock::time_point> parseCommandLineTimestamp(const std::string& tsStr) {
    std::tm tm = {};
    std::istringstream ss(tsStr);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        // Try without seconds if parsing fails
        ss.clear();
        ss.str(tsStr);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M");
        if (ss.fail()) {
            // Try with date only
            ss.clear();
            ss.str(tsStr);
            ss >> std::get_time(&tm, "%Y-%m-%d");
            if (ss.fail()) return std::nullopt;
        }
    }
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

// Configuration struct to hold parsed arguments
struct Config {
    std::string filePath;
    std::vector<LogLevel> filterLevels;
    std::string filterKeyword;
    std::string regexPattern;
    bool keywordCaseSensitive = false;
    std::optional<std::chrono::system_clock::time_point> startTime;
    std::optional<std::chrono::system_clock::time_point> endTime;
    
    SortBy sortBy = SortBy::TIMESTAMP;
    SortOrder sortOrder = SortOrder::ASCENDING;

    std::string customPattern;
    std::string outputFormat = "text"; // Default to text
    std::string outputPath;           // Empty means stdout
    bool showHelp = false;
    bool includeSummary = false;
    bool prettyPrint = false;
    std::string textOutputFormat = "{timestamp} [{level}] {message}";
    bool showUniqueMessages = false;
    bool showTopMessages = false;
    int topMessagesCount = 10;
};

void printHelp() {
    std::cout << "Usage: logAnalyzer <log_file_path> [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --level <LEVEL1,LEVEL2>   Filter by log levels (e.g., ERROR,WARNING)" << std::endl;
    std::cout << "  --keyword <STRING>        Filter messages containing specific text" << std::endl;
    std::cout << "  --case-sensitive          Make keyword filter case-sensitive" << std::endl;
    std::cout << "  --regex <PATTERN>         Filter messages using regex (overrides --keyword)" << std::endl;
    std::cout << "  --start <\"YYYY-MM-DD HH:MM:SS\"> Start time filter (e.g., \"2023-10-27 10:00:00\")" << std::endl;
    std::cout << "  --end <\"YYYY-MM-DD HH:MM:SS\">   End time filter (e.g., \"2023-10-27 11:00:00\")" << std::endl;
    std::cout << "  --sort-by <time|level|msg> Sort entries by field (default: time)" << std::endl;
    std::cout << "  --order <asc|desc>        Sort order (default: asc)" << std::endl;
    std::cout << "  --pattern <REGEX>         Custom regex for parsing log lines (default: [YYYY-MM-DD HH:MM:SS] LEVEL: MESSAGE)" << std::endl;
    std::cout << "  --format <text|json>      Output format (default: text)" << std::endl;
    std::cout << "  --output <file_path>      Redirect output to a file" << std::endl;
    std::cout << "  --text-format <STRING>    Custom format string for text output (e.g., \"{level} {message}\")" << std::endl;
    std::cout << "  --include-summary         Include summary in JSON output" << std::endl;
    std::cout << "  --pretty                  Pretty-print JSON output" << std::endl;
    std::cout << "  --unique-messages         Show counts of unique messages" << std::endl;
    std::cout << "  --top-messages [N]        Show top N most frequent messages (default: 10)" << std::endl;
    std::cout << "  --help                    Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    Config config;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help") {
            config.showHelp = true;
            break;
        } else if (arg == "--level") {
            if (++i < argc) {
                std::vector<std::string> levelStrs = splitString(argv[i], ',');
                for (const auto& levelStr : levelStrs) {
                    LogLevel level = LogAnalyzer::stringToLogLevel(levelStr);
                    if (level != LogLevel::UNKNOWN) {
                        config.filterLevels.push_back(level);
                    } else {
                        std::cerr << "Warning: Unknown log level '" << levelStr << "' ignored." << std::endl;
                    }
                }
            } else { std::cerr << "Error: --level requires an argument." << std::endl; return 1; }
        } else if (arg == "--keyword") {
            if (++i < argc) config.filterKeyword = argv[i];
            else { std::cerr << "Error: --keyword requires an argument." << std::endl; return 1; }
        } else if (arg == "--case-sensitive") {
            config.keywordCaseSensitive = true;
        } else if (arg == "--regex") {
            if (++i < argc) config.regexPattern = argv[i];
            else { std::cerr << "Error: --regex requires an argument." << std::endl; return 1; }
        } else if (arg == "--start") {
            if (++i < argc) {
                config.startTime = parseCommandLineTimestamp(argv[i]);
                if (!config.startTime) { std::cerr << "Error: Invalid start time format. Use \"YYYY-MM-DD HH:MM:SS\"." << std::endl; return 1; }
            } else { std::cerr << "Error: --start requires an argument." << std::endl; return 1; }
        } else if (arg == "--end") {
            if (++i < argc) {
                config.endTime = parseCommandLineTimestamp(argv[i]);
                if (!config.endTime) { std::cerr << "Error: Invalid end time format. Use \"YYYY-MM-DD HH:MM:SS\"." << std::endl; return 1; }
            } else { std::cerr << "Error: --end requires an argument." << std::endl; return 1; }
        } else if (arg == "--sort-by") {
            if (++i < argc) {
                std::string sb = argv[i];
                std::transform(sb.begin(), sb.end(), sb.begin(), ::tolower);
                if (sb == "time") config.sortBy = SortBy::TIMESTAMP;
                else if (sb == "level") config.sortBy = SortBy::LEVEL;
                else if (sb == "msg") config.sortBy = SortBy::MESSAGE;
                else { std::cerr << "Error: Invalid --sort-by value. Use 'time', 'level', or 'msg'." << std::endl; return 1; }
            } else { std::cerr << "Error: --sort-by requires an argument." << std::endl; return 1; }
        } else if (arg == "--order") {
            if (++i < argc) {
                std::string so = argv[i];
                std::transform(so.begin(), so.end(), so.begin(), ::tolower);
                if (so == "asc") config.sortOrder = SortOrder::ASCENDING;
                else if (so == "desc") config.sortOrder = SortOrder::DESCENDING;
                else { std::cerr << "Error: Invalid --order value. Use 'asc' or 'desc'." << std::endl; return 1; }
            } else { std::cerr << "Error: --order requires an argument." << std::endl; return 1; }
        } else if (arg == "--pattern") {
            if (++i < argc) config.customPattern = argv[i];
            else { std::cerr << "Error: --pattern requires an argument." << std::endl; return 1; }
        } else if (arg == "--format") {
            if (++i < argc) {
                std::string format = argv[i];
                std::transform(format.begin(), format.end(), format.begin(), ::tolower);
                if (format == "text" || format == "json") {
                    config.outputFormat = format;
                } else {
                    std::cerr << "Error: Invalid output format '" << argv[i] << "'. Must be 'text' or 'json'." << std::endl;
                    return 1;
                }
            } else { std::cerr << "Error: --format requires an argument." << std::endl; return 1; }
        } else if (arg == "--output") {
            if (++i < argc) config.outputPath = argv[i];
            else { std::cerr << "Error: --output requires an argument." << std::endl; return 1; }
        } else if (arg == "--text-format") {
            if (++i < argc) config.textOutputFormat = argv[i];
            else { std::cerr << "Error: --text-format requires an argument." << std::endl; return 1; }
        } else if (arg == "--include-summary") {
            config.includeSummary = true;
        } else if (arg == "--pretty") {
            config.prettyPrint = true;
        } else if (arg == "--unique-messages") {
            config.showUniqueMessages = true;
        } else if (arg == "--top-messages") {
            config.showTopMessages = true;
            if (i + 1 < argc && argv[i+1][0] != '-') { // Check if next arg is not another option
                try {
                    config.topMessagesCount = std::stoi(argv[++i]);
                } catch (const std::exception& e) {
                    std::cerr << "Error: Invalid number for --top-messages. " << e.what() << std::endl;
                    return 1;
                }
            }
        }
        else {
            if (config.filePath.empty()) config.filePath = arg;
            else {
                std::cerr << "Error: Too many file paths or unrecognized argument: " << arg << std::endl;
                return 1;
            }
        }
    }

    if (config.showHelp) {
        printHelp();
        return 0;
    }

    if (config.filePath.empty()) {
        std::cerr << "Error: No log file path provided." << std::endl;
        printHelp();
        return 1;
    }

    LogAnalyzer analyzer;
    analyzer.analyze(config.filePath, config.customPattern);

    std::ofstream outFile;
    std::ostream* outputStream = &std::cout;
    if (!config.outputPath.empty()) {
        outFile.open(config.outputPath);
        if (!outFile.is_open()) {
            std::cerr << "Error: Could not open output file: " << config.outputPath << std::endl;
            return 1;
        }
        outputStream = &outFile;
    }

    FilterCriteria criteria;
    criteria.levels = config.filterLevels;
    criteria.keyword = config.filterKeyword;
    criteria.regexPattern = config.regexPattern;
    criteria.keywordCaseSensitive = config.keywordCaseSensitive;
    criteria.startTime = config.startTime;
    criteria.endTime = config.endTime;

    if (config.outputFormat == "json") {
        analyzer.exportAsJson(*outputStream, criteria, config.includeSummary, config.prettyPrint);
    } else { // text format
        if (config.showUniqueMessages) {
            *outputStream << "--- Unique Message Counts ---" << std::endl;
            for (const auto& pair : analyzer.getUniqueMessageCounts()) {
                *outputStream << "\"" << pair.first << "\": " << pair.second << std::endl;
            }
        } else if (config.showTopMessages) {
            *outputStream << "--- Top " << config.topMessagesCount << " Most Frequent Messages ---" << std::endl;
            for (const auto& pair : analyzer.getTopMessages(config.topMessagesCount)) {
                *outputStream << "\"" << pair.first << "\": " << pair.second << std::endl;
            }
        } else if (config.filterLevels.empty() && config.filterKeyword.empty() && 
                   config.regexPattern.empty() && !config.startTime && !config.endTime) {
            // If no filters and no specific analysis, print summary
            analyzer.printSummary(*outputStream);
        } else {
            // Otherwise, print filtered and sorted entries
            std::vector<LogEntry> entriesToPrint = analyzer.getSortedFilteredEntries(criteria, config.sortBy, config.sortOrder);
            *outputStream << "--- Filtered and Sorted Log Entries ---" << std::endl;
            // Manually format each entry using the custom format string
            for (const auto& entry : entriesToPrint) {
                std::string output = config.textOutputFormat;
                
                auto replaceAll = [&](std::string& str, const std::string& from, const std::string& to) {
                    size_t start_pos = 0;
                    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
                        str.replace(start_pos, from.length(), to);
                        start_pos += to.length();
                    }
                };

                replaceAll(output, "{timestamp}", analyzer.formatTimestamp(entry.timestamp));
                replaceAll(output, "{level}", LogAnalyzer::logLevelToString(entry.level));
                replaceAll(output, "{message}", entry.message);
                
                *outputStream << output << std::endl;
            }
            *outputStream << "Total filtered entries: " << entriesToPrint.size() << std::endl;
        }
    }

    if (outFile.is_open()) {
        outFile.close();
    }

    return 0;
}
