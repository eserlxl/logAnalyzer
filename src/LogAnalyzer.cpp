#include "LogAnalyzer.h"
#include <algorithm> // For std::transform, std::sort, std::min_element, std::max_element
#include <atomic> // For std::atomic
#include <chrono> // For std::chrono utilities
#include <ctime>  // For std::tm
#include <format> // For std::format (C++20)
#include <fstream>
#include <future>  // For std::future
#include <iomanip> // For std::get_time, std::put_time
#include <iostream>
#include <memory>  // For std::make_unique, std::unique_ptr
#include <numeric> // For std::iota, etc.
#include <ranges>  // For std::ranges::sort
#include <regex>
#include <span> // For std::span
#include <sstream>
#include <stdexcept>  // For std::runtime_error, std::logic_error
#include <stop_token> // For std::stop_token
#include <thread>     // For std::jthread
#include <utility>    // For std::move
#include <vector>     // For std::vector

// Helper function to convert string to LogLevel enum
LogLevel LogAnalyzer::stringToLogLevel(const std::string &levelStr) {
  std::string upperLevelStr = levelStr;
  std::transform(upperLevelStr.begin(), upperLevelStr.end(),
                 upperLevelStr.begin(), ::toupper);
  if (upperLevelStr == "INFO")
    return LogLevel::INFO;
  if (upperLevelStr == "WARNING")
    return LogLevel::WARNING;
  if (upperLevelStr == "ERROR")
    return LogLevel::ERROR;
  if (upperLevelStr == "DEBUG")
    return LogLevel::DEBUG;
  return LogLevel::UNKNOWN;
}

// Helper function to convert LogLevel enum to string
std::string LogAnalyzer::logLevelToString(LogLevel level) {
  switch (level) {
  case LogLevel::INFO:
    return "INFO";
  case LogLevel::WARNING:
    return "WARNING";
  case LogLevel::ERROR:
    return "ERROR";
  case LogLevel::DEBUG:
    return "DEBUG";
  case LogLevel::UNKNOWN:
    return "UNKNOWN";
  }
  return "UNKNOWN"; // Should not be reached
}

LogAnalyzer::LogAnalyzer()
    : defaultParser_(std::make_unique<DefaultLogParser>()),
      log_source_view_(nullptr), active_filter_(nullptr),
      async_control_(nullptr) {
  clear(); // clear() should handle lastReport and potentially other states
}

// Opens a log file and prepares it for iterator-based access.
std::expected<void, LogParseError>
LogAnalyzer::open(const std::string &filePath,
                  std::unique_ptr<ILogParser> parser) {
  // Reset previous state
  log_source_view_.reset();
  active_filter_.reset();
  async_control_.reset();        // Ensure no stale async operations
  lastReport = AnalysisReport{}; // Reset report

  // Ensure default parser is initialized if needed
  if (!defaultParser_) {
    try {
      defaultParser_ = std::make_unique<DefaultLogParser>();
    } catch (const std::regex_error &e) {
      return std::unexpected(
          LogParseError{ParseError::INVALID_REGEX_PATTERN,
                        std::string("Invalid default regex: ") + e.what(), 0});
    }
  }

  // Determine the parser to use for the new view
  std::unique_ptr<ILogParser> viewParser;
  try {
    if (parser) {
      viewParser = std::move(parser);
    } else {
      // Clone the default parser for the view to own and use
      if (defaultParser_) {
        viewParser = defaultParser_->clone();
      } else {
        // Fallback: create a new default parser if defaultParser_ somehow
        // became null
        viewParser = std::make_unique<DefaultLogParser>();
      }
    }
  } catch (const std::regex_error &e) {
    return std::unexpected(
        LogParseError{ParseError::INVALID_REGEX_PATTERN,
                      std::string("Invalid regex pattern: ") + e.what(), 0});
  }

  // Create the LogFileView
  try {
    // LogFileView takes ownership of viewParser
    log_source_view_ =
        std::make_unique<LogFileView>(filePath, std::move(viewParser));
  } catch (const std::exception &e) {
    return std::unexpected(LogParseError{
        ParseError::FILE_OPEN_FAILED,
        std::string("Failed to create log view: ") + e.what(), 0});
  }

  // The file is not read here, only the view is set up. Reading happens via
  // iterators.

  // Report success
  lastReport.status = ParseError::SUCCESS;
  lastReport.linesProcessed = 0;
  lastReport.successfulParses = 0;
  lastReport.parseErrors.clear();

  return {}; // Success
}

// --- Filter Implementations ---

RegexFilter::RegexFilter(std::string pattern,
                         std::regex_constants::syntax_option_type flags)
    : pattern_(pattern, flags) {}

bool RegexFilter::matches(const LogEntry &entry) const {
  return std::regex_search(entry.message, pattern_);
}

FilterSet::FilterSet(Logic logic) : logic_(logic) {}

void FilterSet::add(std::shared_ptr<IFilter> filter) {
  if (filter) {
    filters_.push_back(std::move(filter));
  }
}

void FilterSet::negate() { negated_ = !negated_; }

bool FilterSet::matches(const LogEntry &entry) const {
  if (filters_.empty())
    return !negated_;

  bool result;
  if (logic_ == Logic::AND) {
    result = std::all_of(filters_.begin(), filters_.end(),
                         [&entry](const auto &f) { return f->matches(entry); });
  } else {
    result = std::any_of(filters_.begin(), filters_.end(),
                         [&entry](const auto &f) { return f->matches(entry); });
  }

  return negated_ ? !result : result;
}

// --- DefaultLogParser Implementation ---
DefaultLogParser::DefaultLogParser(
    std::string pattern,
    const std::map<std::string, LogLevel, std::less<>> &mappings)
    : patternString(std::move(pattern)), customLevelMappings(mappings) {
  if (!patternString.empty()) {
    logPattern = std::regex(patternString);
  }
}

std::unique_ptr<ILogParser> DefaultLogParser::clone() const {
  return std::make_unique<DefaultLogParser>(patternString, customLevelMappings);
}

std::optional<LogEntry> DefaultLogParser::parseLine(std::string_view line,
                                                    size_t lineNumber) const {
  std::string lineStr(line);
  std::smatch match;

  // Use the stored logPattern if available, otherwise use a default pattern.
  // Default pattern: [TIMESTAMP] LEVEL: MESSAGE
  static const std::regex defaultRegex(
      R"(\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}(?:\.\d{3})?)\]\s+([A-Z]+):\s+(.*))");

  const std::regex &activeRegex =
      patternString.empty() ? defaultRegex : logPattern;

  if (std::regex_search(lineStr, match, activeRegex) && match.size() == 4) {
    LogEntry entry;
    entry.id = lineNumber;

    // Parse timestamp
    std::string timestampStr = match[1].str();
    std::tm tm = {};
    std::istringstream ss(timestampStr);

    if (timestampStr.length() > 19 && timestampStr[19] == '.') {
      ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
      if (!ss.fail()) {
        long long milliseconds = 0;
        ss.ignore(1); // Skip the dot
        std::string ms_str;
        ss >> ms_str;
        if (!ms_str.empty()) {
          try {
            milliseconds = std::stoll(ms_str);
          } catch (...) {
          }
        }
        entry.timestamp =
            std::chrono::system_clock::from_time_t(std::mktime(&tm)) +
            std::chrono::milliseconds(milliseconds);
      }
    } else {
      ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
      if (!ss.fail()) {
        entry.timestamp =
            std::chrono::system_clock::from_time_t(std::mktime(&tm));
      }
    }

    // Use the parser's own custom mappings
    std::string levelStr = match[2].str();
    std::string upperLevelStr = levelStr;
    std::transform(upperLevelStr.begin(), upperLevelStr.end(),
                   upperLevelStr.begin(), ::toupper);

    if (auto it = customLevelMappings.find(upperLevelStr);
        it != customLevelMappings.end()) {
      entry.level = it->second;
    } else {
      // Fallback to default mapping
      entry.level = LogAnalyzer::stringToLogLevel(levelStr);
    }

    entry.message = match[3].str();
    return entry;
  }

  return std::nullopt;
}

// Sets the active filter for subsequent operations that use it.
void LogAnalyzer::setFilter(std::shared_ptr<IFilter> filter) {
  active_filter_ = std::move(filter);
}

// Provides access to the log data as a view, enabling lazy evaluation.
const LogFileView &LogAnalyzer::getView() const {
  if (!log_source_view_) {
    throw std::runtime_error(
        "No log file is open. Call LogAnalyzer::open() first.");
  }
  // Return a const reference to the managed LogFileView
  return *log_source_view_;
}

void LogAnalyzer::clear() {
  levelCounts[LogLevel::INFO] = 0;
  levelCounts[LogLevel::WARNING] = 0;
  levelCounts[LogLevel::ERROR] = 0;
  levelCounts[LogLevel::DEBUG] = 0;
  levelCounts[LogLevel::UNKNOWN] = 0;
  lastReport = AnalysisReport(); // Reset report

  // Also clear the view and related state
  log_source_view_.reset();
  active_filter_.reset();
  async_control_.reset();
  entries_.clear();
}

// Deprecated getter for entries. Implementation removed as `entries` is gone.
const std::vector<LogEntry> &LogAnalyzer::getEntries() const {
  if (entries_.empty() && log_source_view_) {
    materializeEntries();
  }
  return entries_;
}

std::vector<LogEntry> LogAnalyzer::materializeEntries() const {
  if (!log_source_view_)
    return {};

  entries_.clear();
  for (auto it = log_source_view_->begin(); it != log_source_view_->end();
       ++it) {
    if (it->has_value()) {
      entries_.push_back(it->value());
    } else {
      // If it's a parse error, we still want to keep the entry as UNKNOWN for
      // compatibility but the new model stores the error in std::expected. For
      // materialization, we'll create a dummy entry.
      LogEntry entry;
      entry.id = 0; // Unknown ID
      entry.timestamp = std::chrono::system_clock::now();
      entry.level = LogLevel::UNKNOWN;
      entry.message = it->error().message;
      entries_.push_back(entry);
    }
  }
  return entries_;
}

// View-based access (Iteration 1 Feature)
std::span<const LogEntry> LogAnalyzer::entries_view() const {
  if (entries_.empty() && log_source_view_) {
    materializeEntries();
  }
  return entries_;
}

void LogAnalyzer::printFilteredEntries(std::ostream &out,
                                       const FilterCriteria &criteria,
                                       std::string_view formatString) const {
  auto filteredEntries = getFilteredEntries(criteria);
  for (const auto &entry : filteredEntries) {
    std::string line(formatString);

    // Simple placeholder replacement
    auto replacePlaceholder = [&](std::string &str, const std::string &from,
                                  const std::string &to) {
      size_t start_pos = 0;
      while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
      }
    };

    replacePlaceholder(line, "{level}",
                       LogAnalyzer::logLevelToString(entry.level));
    replacePlaceholder(
        line, "{timestamp}",
        formatTimestamp(entry.timestamp)); // Fixed in main.cpp, but here no
                                           // default is needed
    replacePlaceholder(line, "{message}", entry.message);

    out << line << std::endl;
  }
}

void LogAnalyzer::setCustomLogLevelMapping(std::string_view levelString,
                                           LogLevel mappedLevel) {
  std::string key(levelString);
  std::transform(key.begin(), key.end(), key.begin(), ::toupper);
  customLevelMappings[key] = mappedLevel;
}

LogLevel LogAnalyzer::resolveLogLevel(const std::string &levelStr) const {
  std::string upperLevelStr = levelStr;
  std::transform(upperLevelStr.begin(), upperLevelStr.end(),
                 upperLevelStr.begin(), ::toupper);

  // Check custom mappings first
  if (auto it = customLevelMappings.find(upperLevelStr);
      it != customLevelMappings.end()) {
    return it->second;
  }

  // Fallback to static default mapping
  return LogAnalyzer::stringToLogLevel(levelStr);
}

// Helper for timestamp formatting (Iteration 1 Feature)
std::string
LogAnalyzer::formatTimestamp(std::chrono::system_clock::time_point tp,
                             std::string_view format) const {
  std::time_t time = std::chrono::system_clock::to_time_t(tp);
  std::tm tm = *std::localtime(&time);
  std::stringstream ss;
  // Use the format string provided. std::put_time expects a C-style format
  // string.
  ss << std::put_time(&tm, std::string(format).c_str());
  return ss.str();
}

void LogAnalyzer::exportAsCsv(std::ostream &out,
                              const FilterCriteria &filter) const {
  auto filteredEntries = getFilteredEntries(filter);
  out << "Timestamp,Level,Message" << std::endl;
  for (const auto &entry : filteredEntries) {
    std::string formattedTime = formatTimestamp(entry.timestamp);
    std::string level = LogAnalyzer::logLevelToString(entry.level);
    std::string message = entry.message;

    // CSV Escaping: if message contains comma or quote, wrap in quotes and
    // escape quotes
    if (message.find(',') != std::string::npos ||
        message.find('"') != std::string::npos) {
      std::string escaped;
      escaped += '"';
      for (char c : message) {
        if (c == '"')
          escaped += "\"\"";
        else
          escaped += c;
      }
      escaped += '"';
      message = escaped;
    }
    out << formattedTime << "," << level << "," << message << std::endl;
  }
}

std::vector<LogEntry>
LogAnalyzer::getFilteredEntries(const FilterCriteria &criteria) const {
  std::vector<LogEntry> results;
  for (const auto &entry : entries_) {
    bool match = true;

    // Filter by Level
    if (!criteria.levels.empty()) {
      if (std::find(criteria.levels.begin(), criteria.levels.end(),
                    entry.level) == criteria.levels.end()) {
        match = false;
      }
    }

    // Filter by minLogLevel
    if (match && criteria.minLogLevel.has_value()) {
      // Let's use a custom priority for filtering.
      auto getPriority = [](LogLevel l) {
        switch (l) {
        case LogLevel::DEBUG:
          return 0;
        case LogLevel::INFO:
          return 1;
        case LogLevel::WARNING:
          return 2;
        case LogLevel::ERROR:
          return 3;
        default:
          return -1;
        }
      };

      if (getPriority(entry.level) <
          getPriority(criteria.minLogLevel.value())) {
        match = false;
      }
    }

    // Filter by Keyword
    if (match && !criteria.keyword.empty()) {
      std::string msg = entry.message;
      std::string key = criteria.keyword;
      if (!criteria.keywordCaseSensitive) {
        std::transform(msg.begin(), msg.end(), msg.begin(), ::tolower);
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
      }
      if (msg.find(key) == std::string::npos) {
        match = false;
      }
    }

    // Filter by Regex
    if (match && !criteria.regexPattern.empty()) {
      try {
        std::regex re(criteria.regexPattern);
        if (!std::regex_search(entry.message, re)) {
          match = false;
        }
      } catch (...) {
        match = false;
      }
    }

    // Filter by Time
    if (match && criteria.startTime.has_value()) {
      if (entry.timestamp < criteria.startTime.value()) {
        match = false;
      }
    }
    if (match && criteria.endTime.has_value()) {
      if (entry.timestamp > criteria.endTime.value()) {
        match = false;
      }
    }

    if (match) {
      results.push_back(entry);
    }
  }
  return results;
}

std::vector<LogEntry> LogAnalyzer::getSortedFilteredEntries(
    const FilterCriteria &criteria, SortBy sortBy, SortOrder sortOrder) const {
  auto filtered = getFilteredEntries(criteria);

  std::function<bool(const LogEntry &, const LogEntry &)> comparator;

  switch (sortBy) {
  case SortBy::LEVEL:
    comparator = [](const LogEntry &a, const LogEntry &b) {
      return a.level < b.level;
    };
    break;
  case SortBy::MESSAGE:
    comparator = [](const LogEntry &a, const LogEntry &b) {
      return a.message < b.message;
    };
    break;
  case SortBy::TIMESTAMP:
  default:
    comparator = [](const LogEntry &a, const LogEntry &b) {
      return a.timestamp < b.timestamp;
    };
    break;
  }

  if (sortOrder == SortOrder::DESCENDING) {
    std::sort(
        filtered.begin(), filtered.end(),
        [&](const LogEntry &a, const LogEntry &b) { return comparator(b, a); });
  } else {
    std::sort(filtered.begin(), filtered.end(), comparator);
  }

  return filtered;
}

std::map<std::string, int> LogAnalyzer::getUniqueMessageCounts() const {
  std::map<std::string, int> messageCounts;
  for (const auto &entry : entries_) {
    messageCounts[entry.message]++;
  }
  return messageCounts;
}

std::vector<std::pair<std::string, int>>
LogAnalyzer::getTopMessages(int n) const {
  std::map<std::string, int> messageCounts = getUniqueMessageCounts();

  std::vector<std::pair<std::string, int>> sortedMessages(messageCounts.begin(),
                                                          messageCounts.end());

  std::sort(sortedMessages.begin(), sortedMessages.end(),
            [](const auto &a, const auto &b) {
              return a.second > b.second; // Sort by count, descending
            });

  if (n < 0 || static_cast<size_t>(n) > sortedMessages.size()) {
    return sortedMessages;
  }
  return std::vector<std::pair<std::string, int>>(sortedMessages.begin(),
                                                  sortedMessages.begin() + n);
}

// New API Extensions for Iteration 1 - Search functionality
std::optional<LogEntry>
LogAnalyzer::findFirst(const FilterCriteria &criteria) const {
  auto filtered = getFilteredEntries(criteria);
  if (filtered.empty())
    return std::nullopt;
  return filtered.front();
}

std::optional<LogEntry>
LogAnalyzer::findLast(const FilterCriteria &criteria) const {
  auto filtered = getFilteredEntries(criteria);
  if (filtered.empty())
    return std::nullopt;
  return filtered.back();
}

std::string LogAnalyzer::getSummaryString() const {
  // This method relies on entries.size() and levelCounts.
  // Refactor needed.
  std::stringstream ss;
  ss << "--- Log Analysis Summary ---" << std::endl;
  ss << "Total entries: " << entries_.size()
     << std::endl; // Now that entries_ is populated
  for (auto const &[level, count] : levelCounts) {
    ss << LogAnalyzer::logLevelToString(level) << ": " << count << std::endl;
  }
  return ss.str();
}

// THIS METHOD `parseFile` IS A MAJOR PART OF THE OLD MODEL AND MUST BE
// REWRITTEN. It uses `entries` vector and direct file parsing with regex. It
// must be refactored to use `LogFileView`, `ILogParser`, and `IFilter`.
std::expected<void, LogParseError>
LogAnalyzer::parseFile(const std::string &filePath, const std::string &pattern,
                       bool appendMode) {
  if (!appendMode) {
    // If not in append mode, clear existing data
    clear();
    std::unique_ptr<ILogParser> parser;
    try {
      parser = std::make_unique<DefaultLogParser>(pattern, customLevelMappings);
    } catch (const std::regex_error &e) {
      lastReport.status = ParseError::INVALID_REGEX_PATTERN;
      lastReport.message = e.what();
      lastReport.parseErrors.push_back({0, e.what()});
      return std::unexpected(
          LogParseError{ParseError::INVALID_REGEX_PATTERN, e.what(), 0});
    }
    // Then set up the view for the new file
    auto openResult = open(filePath, std::move(parser));
    if (!openResult.has_value()) {
      lastReport.status = openResult.error().code;
      lastReport.parseErrors.push_back(
          {openResult.error().lineNumber, openResult.error().message});
      return std::unexpected(openResult.error());
    }
  } else {
    // If in append mode, open a temporary view for the new file to append
    // This temporary view will not replace the main log_source_view_
    // A temporary LogFileView is created just to iterate and get its entries.
    std::unique_ptr<ILogParser> tempParser =
        std::make_unique<DefaultLogParser>(pattern);
    LogFileView tempView(filePath, std::move(tempParser));

    // Iterate through tempView and add entries to a temporary vector.
    std::vector<LogEntry> newEntries;
    size_t successfulParses = 0;
    std::vector<std::pair<size_t, std::string>> parseErrors;

    size_t currentLineNumber = 0; // Line number within the appended file

    for (auto it = tempView.begin(); it != tempView.end(); ++it) {
      currentLineNumber++;
      if (it->has_value()) {
        newEntries.push_back(it->value());
        successfulParses++;
      } else {
        parseErrors.push_back({currentLineNumber, it->error().message});
      }
    }

    // Merge newEntries into existing entries_
    entries_.insert(entries_.end(), std::make_move_iterator(newEntries.begin()),
                    std::make_move_iterator(newEntries.end()));
    std::sort(entries_.begin(), entries_.end(),
              [](const LogEntry &a, const LogEntry &b) {
                return a.timestamp < b.timestamp;
              });

    // Update lastReport for append operation
    lastReport.linesProcessed += currentLineNumber;
    lastReport.successfulParses += successfulParses;
    lastReport.parseErrors.insert(lastReport.parseErrors.end(),
                                  std::make_move_iterator(parseErrors.begin()),
                                  std::make_move_iterator(parseErrors.end()));
    if (!parseErrors.empty() && lastReport.status == ParseError::SUCCESS) {
      lastReport.status = ParseError::PARTIAL_FAILURE;
    }
    return {};
  }

  // After setting up the view (if not appendMode), iterate and materialize.
  if (log_source_view_) {
    entries_.clear();
    size_t successfulParses = 0;
    std::vector<std::pair<size_t, std::string>> parseErrors;
    size_t totalLines = 0;

    for (auto it = log_source_view_->begin(); it != log_source_view_->end();
         ++it) {
      totalLines++;
      if (it->has_value()) {
        entries_.push_back(it->value());
        successfulParses++;
      } else {
        // Store the error, but still create a dummy entry for backward
        // compatibility
        parseErrors.push_back({totalLines, it->error().message});
        LogEntry dummyEntry;
        dummyEntry.id = totalLines; // Use line number as ID
        dummyEntry.timestamp = std::chrono::system_clock::now(); // Placeholder
        dummyEntry.level = LogLevel::UNKNOWN;
        dummyEntry.message = "Parse Error: " + it->error().message;
        entries_.push_back(dummyEntry);
      }
    }

    // Sort entries by timestamp
    std::sort(entries_.begin(), entries_.end(),
              [](const LogEntry &a, const LogEntry &b) {
                return a.timestamp < b.timestamp;
              });

    lastReport.linesProcessed = totalLines;
    lastReport.successfulParses = successfulParses;
    lastReport.parseErrors = parseErrors;
    lastReport.status =
        parseErrors.empty() ? ParseError::SUCCESS : ParseError::PARTIAL_FAILURE;
    if (totalLines == 0 && parseErrors.empty())
      lastReport.message = "No log entries found.";
    else if (!parseErrors.empty())
      lastReport.message = "Log parsed with some errors.";
    else
      lastReport.message = "Log parsed successfully.";
  } else {
    lastReport.status = ParseError::FILE_OPEN_FAILED;
    lastReport.message = "File view could not be created or is not valid.";
    return std::unexpected(
        LogParseError{ParseError::FILE_OPEN_FAILED, lastReport.message, 0});
  }

  return {};
}

std::expected<void, LogParseError>
LogAnalyzer::load(const std::string &filePath, const std::string &pattern) {
  return parseFile(filePath, pattern, false);
}

std::expected<void, LogParseError>
LogAnalyzer::append(const std::string &filePath, const std::string &pattern) {
  // Before appending, ensure a file is already loaded to append to.
  if (!log_source_view_ && entries_.empty()) {
    return std::unexpected(
        LogParseError{ParseError::FILE_OPEN_FAILED,
                      "Cannot append: no initial log file loaded.", 0});
  }
  return parseFile(filePath, pattern, true);
}

AnalysisReport LogAnalyzer::analyze(const std::string &filePath,
                                    const std::string &pattern) {
  auto result = load(filePath, pattern);
  // lastReport is already updated by parseFile/load
  return lastReport;
}

std::future<AnalysisReport>
LogAnalyzer::load_async(const std::string &filePath,
                        const std::string &pattern) {
  return std::async(std::launch::async, [this, filePath, pattern] {
    return analyze(filePath, pattern);
  });
}

[[nodiscard]]
AsyncControl &LogAnalyzer::load_async_cancellable(const std::string &filePath,
                                                  const std::string &pattern) {
  if (async_control_ && async_control_->worker.joinable()) {
    async_control_->worker.request_stop();
    async_control_->worker.join();
  }

  auto progress = std::make_shared<std::atomic<float>>(0.0f);
  std::promise<AnalysisReport> reportPromise;
  std::future<AnalysisReport> reportFuture = reportPromise.get_future();
  async_control_ = std::make_unique<AsyncControl>();
  async_control_->progress = progress;
  async_control_->reportFuture = std::move(reportFuture); // Store the future

  async_control_->worker = std::jthread(
      [this, filePath, pattern, progress,
       promise_obj = std::move(reportPromise)](std::stop_token st) mutable {
        AnalysisReport report;

        // Call parseFile which populates `this->lastReport` and
        // `this->entries_` We will need to make parseFile aware of stop_token
        // for proper cancellation. For now, it will complete fully unless
        // stopped.
        auto parseResult = this->parseFile(filePath, pattern,
                                           false); // Pass pattern from outside

        if (st.stop_requested()) {
          report.status = ParseError::PARTIAL_FAILURE;
          report.message = "Operation cancelled.";
        } else if (parseResult.has_value()) {
          report =
              this->lastReport; // Get the report after parseFile has updated it
        } else {
          report = this->lastReport; // Get error report from parseFile
        }

        // Simulate progress for now, will be updated by parseFile in Iteration
        // 2
        progress->store(1.0f);
        promise_obj.set_value(report);
      });

  return *async_control_;
}

// Function signature for streaming (Iteration 1 Feature)
void LogAnalyzer::analyzeStream(
    const std::string &filePath,
    std::function<bool(const LogEntry &)> entryCallback,
    const std::string &pattern) {
  std::ifstream file(filePath);
  if (!file.is_open()) {
    std::cerr << "Could not open file for streaming: " << filePath << std::endl;
    return;
  }

  std::string line;
  std::string finalPattern =
      pattern.empty()
          ? R"(\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}(?:\.\d{3})?)\]\s+([A-Z]+):\s+(.*))"
          : pattern;

  std::regex logRegex;
  try {
    logRegex = std::regex(finalPattern);
  } catch (const std::regex_error &e) {
    std::cerr << "Invalid regex pattern provided for streaming: " << e.what()
              << std::endl;
    return;
  }

  std::smatch match;

  while (std::getline(file, line)) {
    LogEntry entry;
    bool parsedSuccessfully = false;

    if (std::regex_search(line, match, logRegex) && match.size() == 4) {
      // Parse timestamp
      std::string timestampStr = match[1].str();
      std::tm tm = {};
      std::istringstream ss(timestampStr);

      // Handle optional milliseconds
      if (timestampStr.length() > 19 && timestampStr[19] == '.') {
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (!ss.fail()) {
          long long milliseconds = 0;
          ss.ignore(1); // Skip the dot
          std::string ms_str;
          ss >> ms_str;
          if (!ms_str.empty()) {
            try {
              milliseconds = std::stoll(ms_str);
            } catch (...) {
            } // Ignore malformed milliseconds
          }
          entry.timestamp =
              std::chrono::system_clock::from_time_t(std::mktime(&tm)) +
              std::chrono::milliseconds(milliseconds);
          parsedSuccessfully = true;
        }
      } else {
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (!ss.fail()) {
          entry.timestamp =
              std::chrono::system_clock::from_time_t(std::mktime(&tm));
          parsedSuccessfully = true;
        }
      }

      if (parsedSuccessfully) {
        entry.level = resolveLogLevel(match[2].str());
        entry.message = match[3].str();
      }
    }

    if (!parsedSuccessfully) {
      // Fallback for unparseable lines in streaming mode
      entry.timestamp = std::chrono::system_clock::now();
      entry.level = LogLevel::UNKNOWN;
      entry.message = line;
    }

    if (!entryCallback(entry)) {
      break; // Stop processing if callback returns false
    }
  }
  file.close();
}

void LogAnalyzer::printSummary(std::ostream &out) const {
  out << getSummaryString();
}

void LogAnalyzer::exportAsJson(std::ostream &out, const FilterCriteria &filter,
                               bool includeSummary, bool prettyPrint) const {
  throw std::logic_error("LogAnalyzer::exportAsJson is deprecated and must be "
                         "refactored to use IFilter and getView().");
}

// New API Extensions for Iteration 1 - Advanced Statistical Analysis

std::vector<TimeGap>
LogAnalyzer::findTimeGaps(std::chrono::milliseconds minGapDuration) const {
  if (entries_.size() < 2) {
    return {};
  }

  std::vector<TimeGap> gaps;
  for (size_t i = 1; i < entries_.size(); ++i) {
    auto duration = entries_[i].timestamp - entries_[i - 1].timestamp;
    if (duration > minGapDuration) {
      gaps.push_back(
          {entries_[i - 1].timestamp, entries_[i].timestamp, duration});
    }
  }
  return gaps;
}

double LogAnalyzer::getAverageEntryRate() const {
  if (entries_.size() < 2) {
    return 0.0;
  }

  auto totalDuration = entries_.back().timestamp - entries_.front().timestamp;
  auto durationInSeconds =
      std::chrono::duration_cast<std::chrono::seconds>(totalDuration).count();

  if (durationInSeconds == 0) {
    return 0.0;
  }

  return static_cast<double>(entries_.size()) / durationInSeconds;
}

std::vector<TimeWindowStats>
LogAnalyzer::getFrequencyDistribution(std::chrono::seconds windowSize) const {
  return getFrequencyDistributionOptimized(windowSize);
}

// Optimized O(N) implementation of distribution (Iteration 1 Feature)
std::vector<TimeWindowStats> LogAnalyzer::getFrequencyDistributionOptimized(
    std::chrono::seconds windowSize) const {
  if (entries_.empty()) {
    return {};
  }

  std::vector<TimeWindowStats> distribution;
  auto startTime = entries_.front().timestamp;
  // auto endTime = entries_.back().timestamp; // Removed unused variable

  auto currentWindowStart =
      std::chrono::time_point_cast<std::chrono::seconds>(startTime);

  TimeWindowStats currentWindow;
  currentWindow.windowStart = currentWindowStart;
  currentWindow.totalCount = 0;

  for (const auto &entry : entries_) {
    if (entry.timestamp >= currentWindowStart + windowSize) {
      if (currentWindow.totalCount > 0) {
        distribution.push_back(currentWindow);
      }
      // Move to the next window
      currentWindowStart += windowSize;
      while (entry.timestamp >= currentWindowStart + windowSize) {
        // Add empty windows
        distribution.push_back({currentWindowStart, {}, 0});
        currentWindowStart += windowSize;
      }
      currentWindow.windowStart = currentWindowStart;
      currentWindow.counts.clear();
      currentWindow.totalCount = 0;
    }
    currentWindow.counts[entry.level]++;
    currentWindow.totalCount++;
  }

  if (currentWindow.totalCount > 0) {
    distribution.push_back(currentWindow);
  }

  return distribution;
}

// New API Extensions for Iteration 1 - Multi-file Merge

void LogAnalyzer::merge(const LogAnalyzer &other) {
  // Append entries from the other analyzer
  entries_.insert(entries_.end(), other.entries_.begin(), other.entries_.end());

  // Sort all entries by timestamp
  std::sort(entries_.begin(), entries_.end(),
            [](const LogEntry &a, const LogEntry &b) {
              return a.timestamp < b.timestamp;
            });

  // Update level counts
  for (auto const &[level, count] : other.levelCounts) {
    levelCounts[level] += count;
  }
}

// --- Placeholder for Complex Operations ---
// Static utility to merge multiple sorted log views into a new, sorted view.
// This requires implementing a custom iterator that merges multiple
// LogFileViews. This is a complex C++20 ranges feature and requires significant
// implementation.
LogFileView LogAnalyzer::merge_sorted(std::span<const LogFileView> sources) {
  throw std::logic_error("LogAnalyzer::merge_sorted is a complex operation and "
                         "requires a full implementation.");
  // A proper implementation would create a temporary file or a dynamic merge
  // view. For now, returning an empty/invalid view. static LogFileView
  // empty_view("dummy_path", nullptr); // Requires dummy parser return
  // empty_view;
}
