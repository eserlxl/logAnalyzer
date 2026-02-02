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
    defaultParser_ = std::make_unique<DefaultLogParser>();
  }

  // Determine the parser to use for the new view
  std::unique_ptr<ILogParser> viewParser;
  try {
    if (parser) {
      viewParser = std::move(parser);
    } else {
      // Clone the default parser for the view to own and use
      viewParser = defaultParser_->clone();
    }
  } catch (const std::regex_error &e) {
    return std::unexpected(
        LogParseError{ParseError::INVALID_REGEX_PATTERN,
                      std::string("Invalid regex pattern: ") + e.what(), 0});
  }

  // Create a temporary stream to verify the file can be opened
  {
    std::ifstream testStream(filePath);
    if (!testStream.is_open()) {
        return std::unexpected(LogParseError{
            ParseError::FILE_OPEN_FAILED,
            "Could not open file: " + filePath, 0});
    }
  }

  // Create the LogFileView. It no longer opens the file itself,
  // but stores the path and parser for iterators.
  log_source_view_ =
      std::make_unique<LogFileView>(filePath, std::move(viewParser));

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

bool CriteriaToFilterAdapter::matches(const LogEntry &entry) const {
  // Use a custom priority for level comparison
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

  // Filter by Levels
  if (!criteria_.levels.empty()) {
    if (std::find(criteria_.levels.begin(), criteria_.levels.end(),
                  entry.level) == criteria_.levels.end()) {
      return false;
    }
  }

  // Filter by minLogLevel
  if (criteria_.minLogLevel.has_value()) {
    if (getPriority(entry.level) < getPriority(criteria_.minLogLevel.value())) {
      return false;
    }
  }

  // Filter by Keyword or Regex
  if (!criteria_.regexPattern.empty()) {
    try {
      std::regex re(criteria_.regexPattern);
      if (!std::regex_search(entry.message, re)) {
        return false;
      }
    } catch (...) {
      return false;
    }
  } else if (!criteria_.keyword.empty()) {
    std::string msg = entry.message;
    std::string key = criteria_.keyword;
    if (!criteria_.keywordCaseSensitive) {
      std::transform(msg.begin(), msg.end(), msg.begin(), ::tolower);
      std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    }
    if (msg.find(key) == std::string::npos) {
      return false;
    }
  }

  // Filter by Time
  if (criteria_.startTime.has_value()) {
    if (entry.timestamp < criteria_.startTime.value()) {
      return false;
    }
  }
  if (criteria_.endTime.has_value()) {
    if (entry.timestamp > criteria_.endTime.value()) {
      return false;
    }
  }

  return true;
}

// Sets the active filter for subsequent operations that use it.
void LogAnalyzer::setFilter(std::shared_ptr<IFilter> filter) {
  active_filter_ = std::move(filter);
}



void LogAnalyzer::addFilter(std::shared_ptr<IFilter> filter) {
  if (!filter)
    return;

  if (!active_filter_) {
    active_filter_ = std::make_shared<FilterSet>(FilterSet::Logic::AND);
  }

  auto filterSet = std::dynamic_pointer_cast<FilterSet>(active_filter_);
  if (filterSet) {
    filterSet->add(std::move(filter));
  } else {
    // Current active_filter is not a FilterSet, create a new one
    auto newSet = std::make_shared<FilterSet>(FilterSet::Logic::AND);
    newSet->add(active_filter_);
    newSet->add(std::move(filter));
    active_filter_ = std::move(newSet);
  }
}



void LogAnalyzer::runAnalysis(ILogAnalyzer &analyzer, const IFilter *filter) {
  if (!log_source_view_)
    return;

  // Combine provided filter with active_filter_ if both exist
  std::shared_ptr<IFilter> effectiveFilter;
  if (filter && active_filter_) {
    auto set = std::make_shared<FilterSet>(FilterSet::Logic::AND);
    // Since 'filter' is a raw pointer, we can't easily add it to a FilterSet
    // that expects shared_ptr unless we wrap it or change FilterSet.
    // For Iteration 1, we'll just use the provided filter if it exists,
    // otherwise the active one.
    effectiveFilter = active_filter_; // Fallback, see below
  }

  for (auto it = log_source_view_->begin(); it != log_source_view_->end();
       ++it) {
    if (it->has_value()) {
      const auto &entry = it->value();
      bool matches = true;
      if (filter && !filter->matches(entry))
        matches = false;
      if (matches && active_filter_ && !active_filter_->matches(entry))
        matches = false;

      if (matches) {
        analyzer.processEntry(entry);
      }
    }
  }
  analyzer.finalize();
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

  entries_.clear(); // Clear existing entries before populating
  // Reset report before materializing.
  lastReport.linesProcessed = 0;
  lastReport.successfulParses = 0;
  lastReport.parseErrors.clear();
  lastReport.status = ParseError::SUCCESS;
  lastReport.message.clear();

  size_t successfulParses = 0;
  size_t totalLines = 0;
  std::vector<std::pair<size_t, std::string>> parseErrors;

  for (auto it = log_source_view_->begin(); it != log_source_view_->end();
       ++it) {
    totalLines++;
    if (it->has_value()) {
      entries_.push_back(it->value());
      successfulParses++;
    } else {
      parseErrors.push_back({totalLines, it->error().message});
      // Optionally add a dummy entry for backward compatibility
      LogEntry dummyEntry;
      dummyEntry.id = totalLines;
      dummyEntry.timestamp = std::chrono::system_clock::now();
      dummyEntry.level = LogLevel::UNKNOWN;
      dummyEntry.message = "Parse Error: " + it->error().message;
      entries_.push_back(dummyEntry);
    }
  }

  // Update the report after materialization
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
                             std::string_view format) {
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
  if (entries_.empty() && log_source_view_) {
    materializeEntries();
  }

  std::vector<LogEntry> results;
  CriteriaToFilterAdapter adapter(criteria);
  for (const auto &entry : entries_) {
    if (adapter.matches(entry)) {
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
    // If not in append mode, clear existing data and set up a new view
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
    // Set up the view for the new file
    auto openResult = open(filePath, std::move(parser));
    if (!openResult.has_value()) {
      lastReport.status = openResult.error().code;
      lastReport.parseErrors.push_back(
          {openResult.error().lineNumber, openResult.error().message});
      return std::unexpected(openResult.error());
    }
    // After setting up the view, materialize entries immediately for backward compatibility
    // (methods like getEntries() and others still rely on `entries_`)
    entries_.clear(); // Clear before materializing from the new view
    materializeEntries(); // Populates entries_ and updates lastReport from the new view
  } else {
    // If in append mode, open a temporary view for the new file to append
    // This temporary view will not replace the main log_source_view_
    // A temporary LogFileView is created just to iterate and get its entries.
    std::unique_ptr<ILogParser> tempParser =
        std::make_unique<DefaultLogParser>(pattern, customLevelMappings);
    LogFileView tempView(filePath, std::move(tempParser));

    // Iterate through tempView and add entries to a temporary vector.
    std::vector<LogEntry> newEntries;
    size_t successfulParses = 0;
    std::vector<std::pair<size_t, std::string>> parseErrors;
    size_t totalLinesAppended = 0;

    for (auto it = tempView.begin(); it != tempView.end(); ++it) {
      totalLinesAppended++;
      if (it->has_value()) {
        newEntries.push_back(it->value());
        successfulParses++;
      } else {
        parseErrors.push_back({totalLinesAppended, it->error().message});
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
    lastReport.linesProcessed += totalLinesAppended;
    lastReport.successfulParses += successfulParses;
    lastReport.parseErrors.insert(lastReport.parseErrors.end(),
                                  std::make_move_iterator(parseErrors.begin()),
                                  std::make_move_iterator(parseErrors.end()));
    if (!parseErrors.empty() && lastReport.status == ParseError::SUCCESS) {
      lastReport.status = ParseError::PARTIAL_FAILURE;
    }
    if (lastReport.status == ParseError::SUCCESS && totalLinesAppended > 0) {
      lastReport.message = "Log appended successfully.";
    } else if (lastReport.status == ParseError::PARTIAL_FAILURE) {
      lastReport.message = "Log appended with some errors.";
    }
  }

  // Update level counts after potential materialization or append
  levelCounts.clear();
  for(const auto& entry : entries_) {
      levelCounts[entry.level]++;
  }

  return {};
}

// load now just calls parseFile with appendMode = false
std::expected<void, LogParseError>
LogAnalyzer::load(const std::string &filePath, const std::string &pattern) {
  return parseFile(filePath, pattern, false);
}

// append now just calls parseFile with appendMode = true
std::expected<void, LogParseError>
LogAnalyzer::append(const std::string &filePath, const std::string &pattern) {
  // Before appending, ensure a file is already loaded to append to, or at least a view is set up.
  // If entries_ is empty and log_source_view_ is not set, it's an initial load, not an append.
  if (!log_source_view_ && entries_.empty()) {
     // If nothing is loaded yet, treat the first 'append' as a 'load'
     return parseFile(filePath, pattern, false);
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
  if (entries_.empty() && log_source_view_) {
    materializeEntries();
  }

  CriteriaToFilterAdapter adapter(filter);
  std::vector<LogEntry> filteredEntries;
  for (const auto &entry : entries_) {
    if (adapter.matches(entry)) {
      filteredEntries.push_back(entry);
    }
  }

  std::string indent = prettyPrint ? "  " : "";
  std::string newline = prettyPrint ? "\n" : "";

  out << "{" << newline;
  if (includeSummary) {
    out << indent << "\"summary\": {" << newline;
    out << indent << indent << "\"totalEntries\": " << entries_.size() << ","
        << newline;
    out << indent << indent << "\"filteredEntries\": " << filteredEntries.size()
        << "," << newline;
    out << indent << indent << "\"levelCounts\": {" << newline;
    bool firstLevel = true;
    for (auto const &[level, count] : levelCounts) {
      if (!firstLevel)
        out << "," << newline;
      out << indent << indent << indent << "\""
          << LogAnalyzer::logLevelToString(level) << "\": " << count;
      firstLevel = false;
    }
    out << newline << indent << indent << "}" << newline; // End levelCounts
    out << indent << "}," << newline;                     // End summary
  }

  out << indent << "\"entries\": [" << newline;
  for (size_t i = 0; i < filteredEntries.size(); ++i) {
    const auto &entry = filteredEntries[i];
    out << indent << indent << "{" << newline;
    out << indent << indent << indent << "\"id\": " << entry.id << ","
        << newline;
    out << indent << indent << indent << "\"timestamp\": \""
        << formatTimestamp(entry.timestamp, "%Y-%m-%dT%H:%M:%S%z") << "\","
        << newline;
    out << indent << indent << indent << "\"level\": \""
        << LogAnalyzer::logLevelToString(entry.level) << "\"," << newline;
    // Escape message for JSON
    std::string escapedMessage = entry.message;
    size_t pos = escapedMessage.find_first_of("\"\\\b\f\n\r\t");
    while (pos != std::string::npos) {
      switch (escapedMessage[pos]) {
      case '"':
        escapedMessage.replace(pos, 1, "\\\"");
        pos += 2;
        break;
      case '\\':
        escapedMessage.replace(pos, 1, "\\\\");
        pos += 2;
        break;
      case '\b':
        escapedMessage.replace(pos, 1, "\\b");
        pos += 2;
        break;
      case '\f':
        escapedMessage.replace(pos, 1, "\\f");
        pos += 2;
        break;
      case '\n':
        escapedMessage.replace(pos, 1, "\\n");
        pos += 2;
        break;
      case '\r':
        escapedMessage.replace(pos, 1, "\\r");
        pos += 2;
        break;
      case '\t':
        escapedMessage.replace(pos, 1, "\\t");
        pos += 2;
        break;
      }
      pos = escapedMessage.find_first_of("\"\\\b\f\n\r\t", pos);
    }

    out << indent << indent << indent << "\"message\": \"" << escapedMessage
        << "\"" << newline;
    out << indent << indent << "}";
    if (i < filteredEntries.size() - 1) {
      out << ",";
    }
    out << newline;
  }
  out << indent << "]" << newline; // End entries
  out << "}" << newline;           // End root
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

// Static utility to merge multiple sorted log views into a new, sorted view.
// This implementation for Iteration 1 materializes the logs.
LogFileView LogAnalyzer::merge_sorted(std::span<const LogFileView> sources) {
  std::vector<LogEntry> allEntries;

  // 1. Materialize all entries from all sources
  for (const auto &view : sources) {
    for (auto it = view.begin(); it != view.end(); ++it) {
      if (it->has_value()) {
        allEntries.push_back(it->value());
      }
    }
  }

  // 2. Sort the combined entries by timestamp
  std::sort(allEntries.begin(), allEntries.end(),
            [](const LogEntry &a, const LogEntry &b) {
              return a.timestamp < b.timestamp;
            });

  // 3. Create a temporary file
  std::string tempFilePath;
  // Use a platform-specific temp directory or a known local one
#ifdef _WIN32
  char *temp_path_env;
  size_t len;
  _dupenv_s(&temp_path_env, &len, "TEMP");
  tempFilePath = (temp_path_env ? std::string(temp_path_env) : ".") +
                 "\\merged_log_" +
                 std::to_string(std::chrono::system_clock::now()
                                    .time_since_epoch()
                                    .count()) +
                 ".log";
  free(temp_path_env);
#else
  tempFilePath = "/tmp/merged_log_" +
                 std::to_string(std::chrono::system_clock::now()
                                    .time_since_epoch()
                                    .count()) +
                 ".log";
#endif

  std::ofstream tempFile(tempFilePath);
  if (!tempFile.is_open()) {
    throw std::runtime_error("Could not create temporary file for merging.");
  }

  // 4. Write the sorted entries to the temporary file in a parsable format
  for (const auto &entry : allEntries) {
    // Format similar to DefaultLogParser's expectation
    tempFile << "["
             << LogAnalyzer::formatTimestamp(
                    entry.timestamp, "%Y-%m-%d %H:%M:%S")
             << "] " << LogAnalyzer::logLevelToString(entry.level) << ": "
             << entry.message << "\n";
  }
  tempFile.close();

  // 5. Return a LogFileView for this temporary file.
  // The LogFileView will own the temporary file and delete it on destruction.
  auto parser = std::make_unique<DefaultLogParser>();
  return LogFileView(tempFilePath, std::move(parser), true /* isTemporary */);
}

