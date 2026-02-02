#include "LogAnalyzer.h"
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm> // For std::transform

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

// Configuration struct to hold parsed arguments
struct Config {
    std::string filePath;
    std::vector<LogLevel> filterLevels;
    std::string filterKeyword;
    std::string customPattern;
    std::string outputFormat = "text"; // Default to text
    std::string outputPath;           // Empty means stdout
    bool showHelp = false;
};

void printHelp() {
    std::cout << "Usage: logAnalyzer <log_file_path> [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --level <LEVEL1,LEVEL2>   Filter by log levels (e.g., ERROR,WARNING)" << std::endl;
    std::cout << "  --keyword <STRING>        Filter messages containing specific text (case-insensitive)" << std::endl;
    std::cout << "  --pattern <REGEX>         Custom regex for parsing log lines. Must have 3 capture groups: (timestamp), (level), (message)." << std::endl;
    std::cout << "  --format <text|json>      Output format (default: text)" << std::endl;
    std::cout << "  --output <file_path>      Redirect output to a file" << std::endl;
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
            } else {
                std::cerr << "Error: --level requires an argument." << std::endl;
                return 1;
            }
        } else if (arg == "--keyword") {
            if (++i < argc) {
                config.filterKeyword = argv[i];
            } else {
                std::cerr << "Error: --keyword requires an argument." << std::endl;
                return 1;
            }
        } else if (arg == "--pattern") {
            if (++i < argc) {
                config.customPattern = argv[i];
            } else {
                std::cerr << "Error: --pattern requires an argument." << std::endl;
                return 1;
            }
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
            } else {
                std::cerr << "Error: --format requires an argument." << std::endl;
                return 1;
            }
        } else if (arg == "--output") {
            if (++i < argc) {
                config.outputPath = argv[i];
            } else {
                std::cerr << "Error: --output requires an argument." << std::endl;
                return 1;
            }
        } else {
            // Assume it's the file path if not a recognized option
            if (config.filePath.empty()) {
                config.filePath = arg;
            } else {
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

    // Prepare output stream
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

    if (config.outputFormat == "json") {
        analyzer.exportAsJson(*outputStream, criteria);
    } else { // text format
        if (config.filterLevels.empty() && config.filterKeyword.empty()) {
            // If no filters, print summary
            analyzer.printSummary(*outputStream);
        } else {
            // Otherwise, print filtered entries
            std::vector<LogEntry> filteredEntries = analyzer.getFilteredEntries(criteria);
            *outputStream << "--- Filtered Log Entries ---" << std::endl;
            for (const auto& entry : filteredEntries) {
                *outputStream << "[" << entry.timestamp << "] " << LogAnalyzer::logLevelToString(entry.level) << ": " << entry.message << std::endl;
            }
            *outputStream << "Total filtered entries: " << filteredEntries.size() << std::endl;
        }
    }

    if (outFile.is_open()) {
        outFile.close();
    }

    return 0;
}
