#include "LogAnalyzer.h"
#include "Utils.h"
#include "Filter.h"
#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <unistd.h> // For isatty

using json = nlohmann::json;

// Function to parse timestamp, now using Utils
std::optional<std::chrono::system_clock::time_point>
stringToTimePointWrapper(const std::string &tsStr) {
    if (tsStr.empty()) return std::nullopt;
    
    // Try parsing as absolute time
    std::tm tm = {};
    std::istringstream ss(tsStr);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (!ss.fail()) return std::chrono::system_clock::from_time_t(std::mktime(&tm));

    ss.clear(); ss.str(tsStr);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M");
    if (!ss.fail()) return std::chrono::system_clock::from_time_t(std::mktime(&tm));

    ss.clear(); ss.str(tsStr);
    ss >> std::get_time(&tm, "%Y-%m-%d");
    if (!ss.fail()) return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    
    // Try parsing as relative time
    auto relativeTimeResult = Utils::parseRelativeTime(tsStr);
    if (relativeTimeResult.has_value()) {
        return relativeTimeResult.value();
    }
    
    return std::nullopt;
}

// Helper to validate timestamp CLI option
std::string validateTimestamp(const std::string &tsStr) {
    if (tsStr.empty()) return tsStr; // Optional, so empty is fine
    if (stringToTimePointWrapper(tsStr).has_value()) {
        return tsStr;
    }
    throw CLI::ValidationError("Invalid time format. Expected YYYY-MM-DD [HH:MM[:SS]], or relative time like '1h ago'.");
}

struct Config {
  std::vector<std::string> filePaths;
  std::vector<LogLevel> filterLevels;
  std::vector<std::string> filterKeywords;
  std::vector<std::string> excludeKeywords;
  std::vector<std::string> regexPatterns;
  std::vector<std::string> excludeRegexPatterns;
  bool keywordCaseSensitive = false;
  std::optional<std::chrono::system_clock::time_point> startTime;
  std::optional<std::chrono::system_clock::time_point> endTime;
  std::optional<std::chrono::seconds> duration;
  std::optional<LogLevel> minLogLevel;
  CompositeFilter::Logic filterLogic = CompositeFilter::Logic::AND;

  SortBy sortBy = SortBy::TIMESTAMP;
  SortOrder sortOrder = SortOrder::ASCENDING;

  std::string customPattern;
  std::string outputFormat = "text";
  std::string outputPath;
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

  enum class ColorOption { ALWAYS, AUTO, NEVER };
  ColorOption colorOption = ColorOption::AUTO;
  char csvSeparator = ',';
  std::string configFile;
};

int main(int argc, char *argv[]) {
  Config config;
  CLI::App app{"Log Analyzer Tool"};

  app.set_config("--config", "", "Read options from a configuration file", false);

  // Positional Arguments
  app.add_option("log_files", config.filePaths, "Path to log files or '-' for stdin")
     ->check(CLI::ExistingFile | CLI::IsMember({"-"}));

  // Filters
  std::map<std::string, LogLevel> levelMap{
      {"DEBUG", LogLevel::DEBUG}, {"INFO", LogLevel::INFO},
      {"WARNING", LogLevel::WARNING}, {"ERROR", LogLevel::ERROR},
      {"UNKNOWN", LogLevel::UNKNOWN}, {"TRACE", LogLevel::TRACE}, {"FATAL", LogLevel::FATAL}
  };

  app.add_option("--level", config.filterLevels, "Filter by log levels (e.g., ERROR,WARNING)")
     ->transform(CLI::CheckedTransformer(levelMap, CLI::ignore_case));
     
  app.add_option("--min-level", config.minLogLevel, "Filter entries with level greater than or equal to a specified level")
     ->transform(CLI::CheckedTransformer(levelMap, CLI::ignore_case));

  app.add_option("--keyword", config.filterKeywords, "Filter messages containing specific text");
  app.add_option("--exclude-keyword", config.excludeKeywords, "Exclude log entries containing a specific keyword");
     
  app.add_flag("--case-sensitive", config.keywordCaseSensitive, "Make keyword filter case-sensitive");
  
  app.add_option("--regex", config.regexPatterns, "Filter messages using regex");
  app.add_option("--exclude-regex", config.excludeRegexPatterns, "Filter out log entries matching a specific regular expression");

  std::map<std::string, CompositeFilter::Logic> logicMap{
      {"AND", CompositeFilter::Logic::AND}, {"OR", CompositeFilter::Logic::OR}
  };
  app.add_option("--logic", config.filterLogic, "Logic to combine multiple filters of the same type (AND or OR)")
     ->transform(CLI::CheckedTransformer(logicMap, CLI::ignore_case));

  std::string startTimeStr, endTimeStr;
  app.add_option("--start", startTimeStr, "Start time filter (YYYY-MM-DD HH:MM:SS or relative like '1h ago')")
     ->check(validateTimestamp);
  app.add_option("--end", endTimeStr, "End time filter (YYYY-MM-DD HH:MM:SS or relative like '1h ago')")
     ->check(validateTimestamp);
  
  std::string durationStr;
  app.add_option("--duration", durationStr, "Duration for time filtering (e.g., '30m', '1h')");

  // Sorting
  std::map<std::string, SortBy> sortMap{
      {"time", SortBy::TIMESTAMP}, {"level", SortBy::LEVEL}, {"msg", SortBy::MESSAGE}
  };
  app.add_option("--sort-by", config.sortBy, "Sort entries by field")
     ->transform(CLI::CheckedTransformer(sortMap, CLI::ignore_case));
  
  std::map<std::string, SortOrder> orderMap{
      {"asc", SortOrder::ASCENDING}, {"desc", SortOrder::DESCENDING}
  };
  app.add_option("--order", config.sortOrder, "Sort order")
     ->transform(CLI::CheckedTransformer(orderMap, CLI::ignore_case));

  // Output Configuration
  app.add_option("--pattern", config.customPattern, "Custom regex for parsing log lines");
  app.add_option("--format", config.outputFormat, "Output format (text, json, csv)")
     ->check(CLI::IsMember({"text", "json", "csv"}));
  app.add_option("--output", config.outputPath, "Redirect output to a file");
  app.add_option("--text-format", config.textOutputFormat, "Custom format string for text output. Available: {timestamp}, {level}, {message}, {lineNumber}, {fileName}, {elapsedTime}.");
  
  app.add_flag("--include-summary", config.includeSummary, "Include summary in JSON output");
  app.add_flag("--pretty", config.prettyPrint, "Pretty-print JSON output");

  std::map<std::string, Config::ColorOption> colorMap{
      {"always", Config::ColorOption::ALWAYS}, {"auto", Config::ColorOption::AUTO}, {"never", Config::ColorOption::NEVER}
  };
  app.add_option("--color", config.colorOption, "Control output color (always, auto, never)")
     ->transform(CLI::CheckedTransformer(colorMap, CLI::ignore_case));

  app.add_option("--csv-sep", config.csvSeparator, "Custom separator for CSV output (defaults to ',')");

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
          
          if(levelMap.count(toUpper)) {
              config.customLogLevelMappings.push_back({from, levelMap.at(toUpper)});
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

  try {
      app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
      return app.exit(e);
  }

  // Post-processing options
  if(!startTimeStr.empty()) config.startTime = stringToTimePointWrapper(startTimeStr);
  if(!endTimeStr.empty()) config.endTime = stringToTimePointWrapper(endTimeStr);
  if (!durationStr.empty()) {
      auto parsedDuration = Utils::parseDuration(durationStr);
      if (parsedDuration.has_value()) {
          config.duration = parsedDuration.value();
      } else {
          std::cerr << "Error parsing --duration: " << parsedDuration.error() << std::endl;
          return 1;
      }
  }

  if (config.duration.has_value()) {
      if (config.startTime.has_value() && !config.endTime.has_value()) {
          config.endTime = *config.startTime + *config.duration;
      } else if (!config.startTime.has_value() && config.endTime.has_value()) {
          config.startTime = *config.endTime - *config.duration;
      } else if (!config.startTime.has_value() && !config.endTime.has_value()){
          std::cerr << "Error: --duration requires either --start or --end to be specified." << std::endl;
          return 1;
      }
  }

  if(statsWindowSec > 0) config.statsWindow = std::chrono::seconds(statsWindowSec);
  if(gapDurationMs > 0) config.findGapsDuration = std::chrono::milliseconds(gapDurationMs);

  // Logic Validation
  if (config.streamMode && (config.showUniqueMessages || config.showTopMessages)) {
      std::cerr << "Error: --stream is incompatible with --unique-messages or --top-messages." << std::endl;
      return 1;
  }
  if (config.filePaths.empty()) {
      std::cerr << "Error: No log files provided. Use '-' for stdin or provide file paths." << std::endl;
      std::cout << app.help() << std::endl;
      return 1;
  }

  LogAnalyzer analyzer;
  for (const auto &mapping : config.customLogLevelMappings) {
    analyzer.setCustomLogLevelMapping(mapping.first, mapping.second);
  }

  std::ofstream outFile;
  std::ostream *outputStream = &std::cout;
  if (!config.outputPath.empty()) {
    outFile.open(config.outputPath);
    if (!outFile.is_open()) {
      std::cerr << "Error: Could not open output file: " << config.outputPath << std::endl;
      return 1;
    }
    outputStream = &outFile;
  }
  
  bool useColors = (config.colorOption == Config::ColorOption::ALWAYS) || 
                   (config.colorOption == Config::ColorOption::AUTO && isatty(fileno(stdout)));

  auto rootFilter = std::make_shared<CompositeFilter>(CompositeFilter::Logic::AND);

  // Inclusion filters
  auto inclusionFilters = std::make_shared<CompositeFilter>(config.filterLogic);
  if (config.minLogLevel.has_value()) inclusionFilters->add(std::make_shared<MinLevelFilter>(*config.minLogLevel));
  if (!config.filterLevels.empty()) {
      auto levelSet = std::make_shared<CompositeFilter>(CompositeFilter::Logic::OR);
      for (auto l : config.filterLevels) levelSet->add(std::make_shared<LevelFilter>(l));
      inclusionFilters->add(levelSet);
  }
  if (!config.filterKeywords.empty()) {
      auto keywordSet = std::make_shared<CompositeFilter>(config.filterLogic);
      for (const auto& keyword : config.filterKeywords) keywordSet->add(std::make_shared<KeywordFilter>(keyword, config.keywordCaseSensitive));
      inclusionFilters->add(keywordSet);
  }
  if (!config.regexPatterns.empty()) {
      auto regexSet = std::make_shared<CompositeFilter>(config.filterLogic);
      for (const auto& regex : config.regexPatterns) regexSet->add(std::make_shared<RegexFilter>(regex));
      inclusionFilters->add(regexSet);
  }
  rootFilter->add(inclusionFilters);

  // Exclusion filters (always ANDed)
  if (!config.excludeKeywords.empty()) {
      auto exclusionSet = std::make_shared<CompositeFilter>(CompositeFilter::Logic::OR);
      for (const auto& keyword : config.excludeKeywords) exclusionSet->add(std::make_shared<KeywordFilter>(keyword, config.keywordCaseSensitive));
      rootFilter->add(std::make_shared<ExclusionFilter>(exclusionSet));
  }
  if (!config.excludeRegexPatterns.empty()) {
      auto exclusionSet = std::make_shared<CompositeFilter>(CompositeFilter::Logic::OR);
      for (const auto& regex : config.excludeRegexPatterns) exclusionSet->add(std::make_shared<RegexFilter>(regex));
      rootFilter->add(std::make_shared<ExclusionFilter>(exclusionSet));
  }
  
  if (config.startTime || config.endTime) {
      rootFilter->add(std::make_shared<TimeRangeFilter>(
          config.startTime.value_or(std::chrono::system_clock::time_point::min()),
          config.endTime.value_or(std::chrono::system_clock::time_point::max())
      ));
  }
  
  if (config.streamMode) {
      if (config.outputFormat != "text" && config.outputFormat != "csv") {
          std::cerr << "Error: Streaming mode only supports 'text' or 'csv' output format." << std::endl;
          return 1;
      }
      auto streamEntryCallback = [&](const LogEntry &entry) {
          if (rootFilter->matches(entry)) {
               if (config.outputFormat == "text") {
                    *outputStream << analyzer.formatEntry(entry, config.textOutputFormat, useColors) << std::endl;
               } else { // CSV
                   *outputStream << "\"" << LogAnalyzer::formatTimestamp(entry.timestamp) << "\"" << config.csvSeparator
                                 << "\"" << LogAnalyzer::logLevelToString(entry.level) << "\"" << config.csvSeparator
                                 << "\"" << entry.message << "\"" 
                                 << config.csvSeparator << "\"" << entry.sourceFile << "\"" << std::endl;
               }
          }
          return true;
      };
      analyzer.analyzeStream(config.filePaths, streamEntryCallback, config.customPattern);
  } else {
      for (const auto& path : config.filePaths) {
          if(auto res = analyzer.append(path, config.customPattern); !res) {
               std::cerr << "Error analyzing file " << path << ": " << res.error().message << std::endl;
               return 1;
          }
      }
      std::vector<LogEntry> filteredEntries;
      for (const auto& entry : analyzer.getEntries()) {
          if (rootFilter->matches(entry)) {
              filteredEntries.push_back(entry);
          }
      }
      
      // Sorting
      if (config.sortBy != SortBy::TIMESTAMP || config.sortOrder != SortOrder::ASCENDING) {
          std::sort(filteredEntries.begin(), filteredEntries.end(), [&](const LogEntry& a, const LogEntry& b) {
              if (config.sortBy == SortBy::TIMESTAMP) {
                  return config.sortOrder == SortOrder::ASCENDING ? a.timestamp < b.timestamp : a.timestamp > b.timestamp;
              } else if (config.sortBy == SortBy::LEVEL) {
                  return config.sortOrder == SortOrder::ASCENDING ? a.level < b.level : a.level > b.level;
              } else { // MESSAGE
                  return config.sortOrder == SortOrder::ASCENDING ? a.message < b.message : a.message > b.message;
              }
          });
      }
      
      if (config.outputFormat == "text") {
          for(const auto& entry : filteredEntries) {
              *outputStream << analyzer.formatEntry(entry, config.textOutputFormat, useColors) << std::endl;
          }
      } else if (config.outputFormat == "csv") {
          *outputStream << "Timestamp" << config.csvSeparator << "Level" << config.csvSeparator << "Message" << config.csvSeparator << "File\n";
          for (const auto& entry : filteredEntries) {
              *outputStream << LogAnalyzer::formatTimestamp(entry.timestamp) << config.csvSeparator
                            << LogAnalyzer::logLevelToString(entry.level) << config.csvSeparator;
              std::string msg = entry.message;
              bool needsQuotes = msg.find(config.csvSeparator) != std::string::npos || msg.find('"') != std::string::npos;
              if (needsQuotes) {
                  Utils::replaceAll(msg, "\"", "\"\"");
                  *outputStream << "\"" << msg << "\"";
              } else {
                  *outputStream << msg;
              }
              *outputStream << config.csvSeparator << entry.sourceFile << "\n";
          }
      } else if (config.outputFormat == "json") {
           json j;
           if (config.includeSummary) {
               j["totalEntries"] = filteredEntries.size();
           }
           j["entries"] = json::array();
           for (const auto& entry : filteredEntries) {
               j["entries"].push_back({
                   {"timestamp", LogAnalyzer::formatTimestamp(entry.timestamp)},
                   {"level", LogAnalyzer::logLevelToString(entry.level)},
                   {"message", entry.message},
                   {"file", entry.sourceFile}
               });
           }
           if (config.prettyPrint) {
               *outputStream << std::setw(4) << j << std::endl;
           } else {
               *outputStream << j << std::endl;
           }
      }
      
      if (config.showUniqueMessages) {
          std::map<std::string, int> counts;
          for (const auto& entry : filteredEntries) counts[entry.message]++;
          *outputStream << "\nUnique Messages: " << counts.size() << "\n";
      }
      
      if (config.showTopMessages) {
          std::map<std::string, int> counts;
          for (const auto& entry : filteredEntries) counts[entry.message]++;
          std::vector<std::pair<std::string, int>> sorted(counts.begin(), counts.end());
          std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b){ return a.second > b.second; });
          *outputStream << "\nTop " << config.topMessagesCount << " Messages:\n";
          for (int i=0; i < std::min((int)sorted.size(), config.topMessagesCount); ++i) {
              *outputStream << sorted[i].second << ": " << sorted[i].first << "\n";
          }
      }

      if (config.showEntryRate) {
           if (filteredEntries.size() > 1) {
               auto dur = filteredEntries.back().timestamp - filteredEntries.front().timestamp;
               auto secs = std::chrono::duration_cast<std::chrono::seconds>(dur).count();
               double rate = secs > 0 ? (double)filteredEntries.size() / secs : filteredEntries.size();
               *outputStream << "\nAverage Entry Rate: " << rate << " entries/sec\n";
           }
      }
  }

  return 0;
}
