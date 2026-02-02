#include "LogAnalyzer.h"
#include <algorithm> // For std::transform
#include <chrono>    // For std::chrono::system_clock::time_point
#include <fstream>
#include <iomanip> // For std::get_time
#include <iostream>
#include <optional> // For std::optional
#include <sstream>
#include <string>
#include <vector>

// Helper function to split a string by a delimiter
std::vector<std::string> splitString(const std::string &s, char delimiter) {
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream tokenStream(s);
  while (std::getline(tokenStream, token, delimiter)) {
    tokens.push_back(token);
  }
  return tokens;
}

// Helper to parse timestamp from CLI
std::optional<std::chrono::system_clock::time_point>
parseCommandLineTimestamp(const std::string &tsStr) {
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
      if (ss.fail())
        return std::nullopt;
    }
  }
  return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

// Configuration struct to hold parsed arguments
struct Config {
  std::vector<std::string> filePaths;
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
  std::string outputPath;            // Empty means stdout
  bool showHelp = false;
  bool includeSummary = false;
  bool prettyPrint = false;
  std::string textOutputFormat = "{timestamp} [{level}] {message}";
  bool showUniqueMessages = false;
  bool showTopMessages = false;
  int topMessagesCount = 10;
  bool streamMode = false;
  std::vector<std::pair<std::string, LogLevel>> customLogLevelMappings;
  std::optional<std::chrono::seconds> statsWindow;
  std::optional<std::chrono::milliseconds> findGapsDuration;
  bool showEntryRate = false;
};

void printHelp() {
  std::cout << "Usage: logAnalyzer <log_file_path...> [options]" << std::endl;
  std::cout << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  --level <LEVEL1,LEVEL2>   Filter by log levels (e.g., "
               "ERROR,WARNING)"
            << std::endl;
  std::cout
      << "  --keyword <STRING>        Filter messages containing specific text"
      << std::endl;
  std::cout << "  --case-sensitive          Make keyword filter case-sensitive"
            << std::endl;
  std::cout << "  --regex <PATTERN>         Filter messages using regex "
               "(overrides --keyword)"
            << std::endl;
  std::cout << "  --start <\"YYYY-MM-DD HH:MM:SS\"> Start time filter (e.g., "
               "\"2023-10-27 10:00:00\")"
            << std::endl;
  std::cout << "  --end <\"YYYY-MM-DD HH:MM:SS\">   End time filter (e.g., "
               "\"2023-10-27 11:00:00\")"
            << std::endl;
  std::cout
      << "  --sort-by <time|level|msg> Sort entries by field (default: time)"
      << std::endl;
  std::cout << "  --order <asc|desc>        Sort order (default: asc)"
            << std::endl;
  std::cout << "  --pattern <REGEX>         Custom regex for parsing log lines "
               "(default: [YYYY-MM-DD HH:MM:SS] LEVEL: MESSAGE)"
            << std::endl;
  std::cout << "  --format <text|json|csv>  Output format (default: text)"
            << std::endl;
  std::cout << "  --output <file_path>      Redirect output to a file"
            << std::endl;
  std::cout << "  --text-format <STRING>    Custom format string for text "
               "output (e.g., \"{level} {message}\")"
            << std::endl;
  std::cout << "  --include-summary         Include summary in JSON output"
            << std::endl;
  std::cout << "  --pretty                  Pretty-print JSON output"
            << std::endl;
  std::cout << "  --unique-messages         Show counts of unique messages"
            << std::endl;
  std::cout << "  --top-messages [N]        Show top N most frequent messages "
               "(default: 10)"
            << std::endl;
  std::cout << "  --stream                  Enable streaming mode for large "
               "files (incompatible with sorting)"
            << std::endl;
  std::cout << "  --map-level <FROM=TO>     Map a custom log level string to a "
               "standard one (e.g., FATAL=ERROR)"
            << std::endl;
  std::cout << "  --stats-window <SECONDS>  Show log frequency distribution "
               "over a time window"
            << std::endl;
  std::cout << "  --find-gaps <MS>          Find time gaps in logs longer than "
               "the specified milliseconds"
            << std::endl;
  std::cout << "  --rate                    Calculate and show the average log "
               "entry rate (entries/second)"
            << std::endl;
  std::cout << "  --help                    Show this help message"
            << std::endl;
}

int main(int argc, char *argv[]) {
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
        for (const auto &levelStr : levelStrs) {
          LogLevel level = LogAnalyzer::stringToLogLevel(levelStr);
          if (level != LogLevel::UNKNOWN) {
            config.filterLevels.push_back(level);
          } else {
            std::cerr << "Warning: Unknown log level '" << levelStr
                      << "' ignored." << std::endl;
          }
        }
      } else {
        std::cerr << "Error: --level requires an argument." << std::endl;
        return 1;
      }
    } else if (arg == "--keyword") {
      if (++i < argc)
        config.filterKeyword = argv[i];
      else {
        std::cerr << "Error: --keyword requires an argument." << std::endl;
        return 1;
      }
    } else if (arg == "--case-sensitive") {
      config.keywordCaseSensitive = true;
    } else if (arg == "--regex") {
      if (++i < argc)
        config.regexPattern = argv[i];
      else {
        std::cerr << "Error: --regex requires an argument." << std::endl;
        return 1;
      }
    } else if (arg == "--start") {
      if (++i < argc) {
        config.startTime = parseCommandLineTimestamp(argv[i]);
        if (!config.startTime) {
          std::cerr << "Error: Invalid start time format. Use \"YYYY-MM-DD "
                       "HH:MM:SS\"."
                    << std::endl;
          return 1;
        }
      } else {
        std::cerr << "Error: --start requires an argument." << std::endl;
        return 1;
      }
    } else if (arg == "--end") {
      if (++i < argc) {
        config.endTime = parseCommandLineTimestamp(argv[i]);
        if (!config.endTime) {
          std::cerr
              << "Error: Invalid end time format. Use \"YYYY-MM-DD HH:MM:SS\"."
              << std::endl;
          return 1;
        }
      } else {
        std::cerr << "Error: --end requires an argument." << std::endl;
        return 1;
      }
    } else if (arg == "--sort-by") {
      if (++i < argc) {
        std::string sb = argv[i];
        std::transform(sb.begin(), sb.end(), sb.begin(), ::tolower);
        if (sb == "time")
          config.sortBy = SortBy::TIMESTAMP;
        else if (sb == "level")
          config.sortBy = SortBy::LEVEL;
        else if (sb == "msg")
          config.sortBy = SortBy::MESSAGE;
        else {
          std::cerr << "Error: Invalid --sort-by value. Use 'time', 'level', "
                       "or 'msg'."
                    << std::endl;
          return 1;
        }
      } else {
        std::cerr << "Error: --sort-by requires an argument." << std::endl;
        return 1;
      }
    } else if (arg == "--order") {
      if (++i < argc) {
        std::string so = argv[i];
        std::transform(so.begin(), so.end(), so.begin(), ::tolower);
        if (so == "asc")
          config.sortOrder = SortOrder::ASCENDING;
        else if (so == "desc")
          config.sortOrder = SortOrder::DESCENDING;
        else {
          std::cerr << "Error: Invalid --order value. Use 'asc' or 'desc'."
                    << std::endl;
          return 1;
        }
      } else {
        std::cerr << "Error: --order requires an argument." << std::endl;
        return 1;
      }
    } else if (arg == "--pattern") {
      if (++i < argc)
        config.customPattern = argv[i];
      else {
        std::cerr << "Error: --pattern requires an argument." << std::endl;
        return 1;
      }
    } else if (arg == "--format") {
      if (++i < argc) {
        std::string format = argv[i];
        std::transform(format.begin(), format.end(), format.begin(), ::tolower);
        if (format == "text" || format == "json" || format == "csv") {
          config.outputFormat = format;
        } else {
          std::cerr << "Error: Invalid output format '" << argv[i]
                    << "'. Must be 'text', 'json', or 'csv'." << std::endl;
          return 1;
        }
      } else {
        std::cerr << "Error: --format requires an argument." << std::endl;
        return 1;
      }
    } else if (arg == "--output") {
      if (++i < argc)
        config.outputPath = argv[i];
      else {
        std::cerr << "Error: --output requires an argument." << std::endl;
        return 1;
      }
    } else if (arg == "--text-format") {
      if (++i < argc)
        config.textOutputFormat = argv[i];
      else {
        std::cerr << "Error: --text-format requires an argument." << std::endl;
        return 1;
      }
    } else if (arg == "--include-summary") {
      config.includeSummary = true;
    } else if (arg == "--pretty") {
      config.prettyPrint = true;
    } else if (arg == "--unique-messages") {
      config.showUniqueMessages = true;
    } else if (arg == "--top-messages") {
      config.showTopMessages = true;
      if (i + 1 < argc &&
          argv[i + 1][0] != '-') { // Check if next arg is not another option
        try {
          config.topMessagesCount = std::stoi(argv[++i]);
        } catch (const std::exception &e) {
          std::cerr << "Error: Invalid number for --top-messages. " << e.what()
                    << std::endl;
          return 1;
        }
      }
    } else if (arg == "--stream") {
      config.streamMode = true;
    } else if (arg == "--map-level") {
      if (++i < argc) {
        std::string mapStr = argv[i];
        size_t eqPos = mapStr.find('=');
        if (eqPos != std::string::npos) {
          std::string from = mapStr.substr(0, eqPos);
          std::string to = mapStr.substr(eqPos + 1);
          LogLevel mappedLevel = LogAnalyzer::stringToLogLevel(to);
          if (mappedLevel != LogLevel::UNKNOWN) {
            config.customLogLevelMappings.push_back({from, mappedLevel});
          } else {
            std::cerr << "Error: Invalid target log level for --map-level: '"
                      << to << "'." << std::endl;
            return 1;
          }
        } else {
          std::cerr << "Error: --map-level argument format should be KEY=LEVEL."
                    << std::endl;
          return 1;
        }
      } else {
        std::cerr << "Error: --map-level requires an argument." << std::endl;
        return 1;
      }
    } else if (arg == "--stats-window") {
      if (++i < argc) {
        try {
          config.statsWindow = std::chrono::seconds(std::stoi(argv[i]));
        } catch (const std::exception &e) {
          std::cerr << "Error: Invalid number for --stats-window. " << e.what()
                    << std::endl;
          return 1;
        }
      } else {
        std::cerr << "Error: --stats-window requires an argument." << std::endl;
        return 1;
      }
    } else if (arg == "--find-gaps") {
      if (++i < argc) {
        try {
          config.findGapsDuration =
              std::chrono::milliseconds(std::stoi(argv[i]));
        } catch (const std::exception &e) {
          std::cerr << "Error: Invalid number for --find-gaps. " << e.what()
                    << std::endl;
          return 1;
        }
      } else {
        std::cerr << "Error: --find-gaps requires an argument." << std::endl;
        return 1;
      }
    } else if (arg == "--rate") {
      config.showEntryRate = true;
    } else {
      // Positional argument, assume it's a file path
      config.filePaths.push_back(arg);
    }
  }

  if (config.showHelp) {
    printHelp();
    return 0;
  }

  if (config.streamMode) {
    if (config.sortBy != SortBy::TIMESTAMP ||
        config.sortOrder != SortOrder::ASCENDING) {
      std::cerr << "Error: --stream is incompatible with --sort-by. Streaming "
                   "mode does not support sorting."
                << std::endl;
      return 1;
    }
    if (config.showUniqueMessages) {
      std::cerr << "Error: --stream is incompatible with --unique-messages. "
                   "Unique message counts require global knowledge."
                << std::endl;
      return 1;
    }
    if (config.showTopMessages) {
      std::cerr << "Error: --stream is incompatible with --top-messages. Top "
                   "messages require global knowledge."
                << std::endl;
      return 1;
    }
    if (!config.filePaths.empty() && config.filePaths.size() > 1) {
      std::cerr << "Error: --stream is not compatible with multiple input "
                   "files. Please provide a single file for streaming."
                << std::endl;
      return 1;
    }
  }

  if (config.filePaths.empty() &&
      !config.streamMode) { // If no file paths and not in streaming mode
    std::cerr << "Error: No log file path provided and not in streaming mode."
              << std::endl;
    printHelp();
    return 1;
  }

  LogAnalyzer analyzer;

  // Apply custom log level mappings
  for (const auto &mapping : config.customLogLevelMappings) {
    analyzer.setCustomLogLevelMapping(mapping.first, mapping.second);
  }

  if (!config.streamMode) {
    // Load the first file, then append others
    if (!config.filePaths.empty()) {
      auto report = analyzer.analyze(
          config.filePaths[0],
          config
              .customPattern); // The analyze method now returns AnalysisReport
      if (report.status != ParseError::SUCCESS &&
          report.status != ParseError::PARTIAL_FAILURE) {
        std::cerr << "Error analyzing file " << config.filePaths[0]
                  << ". Status: ";
        if (report.status == ParseError::FILE_OPEN_FAILED) {
          std::cerr << "File open failed.";
        } else if (report.status == ParseError::INVALID_REGEX_PATTERN) {
          std::cerr << "Invalid regex pattern.";
        } else {
          std::cerr << "Unknown error.";
        }
        if (!report.parseErrors.empty()) {
          std::cerr << " First parse error: " << report.parseErrors[0].second;
        }
        std::cerr << std::endl;
        return 1;
      }
      if (!report.parseErrors.empty()) {
        std::cerr << "Warning: " << report.parseErrors.size()
                  << " lines failed to parse in " << config.filePaths[0]
                  << std::endl;
      }

      for (size_t i = 1; i < config.filePaths.size(); ++i) {
        auto result =
            analyzer.append(config.filePaths[i], config.customPattern);
        if (!result) { // std::expected holds the error on failure
          std::cerr << "Error appending file " << config.filePaths[i] << ": "
                    << result.error().message << std::endl;
          return 1;
        }
        auto appendReport =
            analyzer.getAnalysisReport(); // Get report for append operation
        if (!appendReport.parseErrors.empty()) {
          std::cerr << "Warning: " << appendReport.parseErrors.size()
                    << " lines failed to parse in " << config.filePaths[i]
                    << std::endl;
        }
      }
    }
  }

  std::ofstream outFile;
  std::ostream *outputStream = &std::cout;
  if (!config.outputPath.empty()) {
    outFile.open(config.outputPath);
    if (!outFile.is_open()) {
      std::cerr << "Error: Could not open output file: " << config.outputPath
                << std::endl;
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

  if (config.streamMode) {
    // In streaming mode, only text and CSV output are supported for entries
    if (config.outputFormat != "text" && config.outputFormat != "csv") {
      std::cerr << "Error: Streaming mode only supports 'text' or 'csv' output "
                   "format."
                << std::endl;
      return 1;
    }

    auto streamEntryCallback = [&](const LogEntry &entry) {
      // Apply filtering criteria within the callback
      bool matchesLevel = criteria.levels.empty();
      if (!criteria.levels.empty()) {
        for (LogLevel level : criteria.levels) {
          if (entry.level == level) {
            matchesLevel = true;
            break;
          }
        }
      }

      bool matchesKeyword = true;
      if (!criteria.keyword.empty()) {
        if (criteria.keywordCaseSensitive) {
          matchesKeyword =
              (entry.message.find(criteria.keyword) != std::string::npos);
        } else {
          std::string messageLower = entry.message;
          std::transform(messageLower.begin(), messageLower.end(),
                         messageLower.begin(), ::tolower);
          std::string keywordLower = criteria.keyword;
          std::transform(keywordLower.begin(), keywordLower.end(),
                         keywordLower.begin(), ::tolower);
          matchesKeyword =
              (messageLower.find(keywordLower) != std::string::npos);
        }
      }

      bool matchesRegex = true;
      if (!criteria.regexPattern.empty()) {
        try {
          std::regex re(criteria.regexPattern);
          matchesRegex = std::regex_search(entry.message, re);
        } catch (const std::regex_error &e) {
          // This error should ideally be caught earlier during argument parsing
          // But as a fallback, we'll treat it as not matching
          matchesRegex = false;
        }
      }

      bool matchesTime = true;
      if (criteria.startTime && entry.timestamp < *criteria.startTime)
        matchesTime = false;
      if (criteria.endTime && entry.timestamp > *criteria.endTime)
        matchesTime = false;

      if (matchesLevel && matchesKeyword && matchesRegex && matchesTime) {
        if (config.outputFormat == "text") {
          std::string output = config.textOutputFormat;
          auto replaceAll = [&](std::string &str, const std::string &from,
                                const std::string &to) {
            size_t start_pos = 0;
            while ((start_pos = str.find(from, start_pos)) !=
                   std::string::npos) {
              str.replace(start_pos, from.length(), to);
              start_pos += to.length();
            }
          };
          replaceAll(
              output, "{timestamp}",
              analyzer.formatTimestamp(entry.timestamp, "%Y-%m-%d %H:%M:%S"));
          replaceAll(output, "{level}",
                     LogAnalyzer::logLevelToString(entry.level));
          replaceAll(output, "{message}", entry.message);
          *outputStream << output << std::endl;
        } else if (config.outputFormat == "csv") {
          // Simplified CSV output for streaming
          *outputStream << "\""
                        << analyzer.formatTimestamp(entry.timestamp,
                                                    "%Y-%m-%d %H:%M:%S")
                        << "\",\"" << LogAnalyzer::logLevelToString(entry.level)
                        << "\",\"" << entry.message << "\"" << std::endl;
        }
      }
      return true; // Continue processing stream
    };

    // Assuming only one file for streaming due to conflict check
    if (!config.filePaths.empty()) {
      analyzer.analyzeStream(config.filePaths[0], streamEntryCallback,
                             config.customPattern);
    } else {
      // Should not happen if argument parsing is correct (file path required if
      // not streaming)
      std::cerr << "Error: No file specified for streaming analysis."
                << std::endl;
      return 1;
    }

  } else { // Not streaming mode
    if (config.statsWindow) {
      *outputStream << "--- Log Frequency Distribution (Window: "
                    << config.statsWindow->count() << "s) ---" << std::endl;
      for (const auto &stats :
           analyzer.getFrequencyDistributionOptimized(*config.statsWindow)) {
        *outputStream << analyzer.formatTimestamp(stats.windowStart,
                                                  "%Y-%m-%d %H:%M:%S")
                      << ": Total=" << stats.totalCount;
        for (const auto &levelCount : stats.counts) {
          *outputStream << ", "
                        << LogAnalyzer::logLevelToString(levelCount.first)
                        << "=" << levelCount.second;
        }
        *outputStream << std::endl;
      }
    }
    if (config.findGapsDuration) {
      *outputStream << "--- Time Gaps (Min Duration: "
                    << config.findGapsDuration->count() << "ms) ---"
                    << std::endl;
      for (const auto &gap : analyzer.findTimeGaps(*config.findGapsDuration)) {
        *outputStream << "Gap from "
                      << analyzer.formatTimestamp(gap.start,
                                                  "%Y-%m-%d %H:%M:%S")
                      << " to "
                      << analyzer.formatTimestamp(gap.end, "%Y-%m-%d %H:%M:%S")
                      << " (Duration: "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(
                             gap.duration)
                             .count()
                      << "ms)" << std::endl;
      }
    }
    if (config.showEntryRate) {
      *outputStream << "--- Average Entry Rate ---" << std::endl;
      *outputStream << "Average entries/second: " << std::fixed
                    << std::setprecision(2) << analyzer.getAverageEntryRate()
                    << std::endl;
    }

    // If any specific statistical analysis was requested, we don't proceed with
    // other output types unless they are explicitly requested and make sense.
    // For simplicity, if stats are requested, only stats are printed.
    bool statsRequested = config.statsWindow.has_value() ||
                          config.findGapsDuration.has_value() ||
                          config.showEntryRate;

    if (!statsRequested) {
      if (config.outputFormat == "json") {
        analyzer.exportAsJson(*outputStream, criteria, config.includeSummary,
                              config.prettyPrint);
      } else if (config.outputFormat == "csv") {
        analyzer.exportAsCsv(*outputStream, criteria);
      } else { // text format
        if (config.showUniqueMessages) {
          *outputStream << "--- Unique Message Counts ---" << std::endl;
          for (const auto &pair : analyzer.getUniqueMessageCounts()) {
            *outputStream << "\"" << pair.first << "\": " << pair.second
                          << std::endl;
          }
        } else if (config.showTopMessages) {
          *outputStream << "--- Top " << config.topMessagesCount
                        << " Most Frequent Messages ---" << std::endl;
          for (const auto &pair :
               analyzer.getTopMessages(config.topMessagesCount)) {
            *outputStream << "\"" << pair.first << "\": " << pair.second
                          << std::endl;
          }
        } else if (config.filterLevels.empty() &&
                   config.filterKeyword.empty() &&
                   config.regexPattern.empty() && !config.startTime &&
                   !config.endTime) {
          // If no filters and no specific analysis, print summary
          analyzer.printSummary(*outputStream);
        } else {
          // Otherwise, print filtered and sorted entries
          std::vector<LogEntry> entriesToPrint =
              analyzer.getSortedFilteredEntries(criteria, config.sortBy,
                                                config.sortOrder);
          *outputStream << "--- Filtered and Sorted Log Entries ---"
                        << std::endl;
          // Manually format each entry using the custom format string
          for (const auto &entry : entriesToPrint) {
            std::string output = config.textOutputFormat;

            auto replaceAll = [&](std::string &str, const std::string &from,
                                  const std::string &to) {
              size_t start_pos = 0;
              while ((start_pos = str.find(from, start_pos)) !=
                     std::string::npos) {
                str.replace(start_pos, from.length(), to);
                start_pos += to.length();
              }
            };

            replaceAll(
                output, "{timestamp}",
                analyzer.formatTimestamp(entry.timestamp, "%Y-%m-%d %H:%M:%S"));
            replaceAll(output, "{level}",
                       LogAnalyzer::logLevelToString(entry.level));
            replaceAll(output, "{message}", entry.message);

            *outputStream << output << std::endl;
          }
          *outputStream << "Total filtered entries: " << entriesToPrint.size()
                        << std::endl;
        }
      }
    }
  }

  if (outFile.is_open()) {
    outFile.close();
  }

  return 0;
}
