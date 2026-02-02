#ifndef LOG_ANALYZER_H
#define LOG_ANALYZER_H

#include <atomic>     // For std::atomic
#include <chrono>     // For std::chrono::system_clock::time_point
#include <expected>   // For std::expected (C++23)
#include <fstream>    // For std::ifstream
#include <functional> // For std::function
#include <future>
#include <iomanip> // For timestamp formatting
#include <iostream>
#include <map>
#include <optional>   // For std::optional
#include <ranges>     // For std::ranges (C++20)
#include <regex>      // For regexPattern in FilterCriteria
#include <span>       // For std::span (C++20)
#include <stop_token> // For std::stop_token
#include <string>
#include <string_view> // For std::string_view (C++17, but emphasized for C++20/23 use)
#include <thread> // For std::jthread
#include <vector>

enum class LogLevel { INFO, WARNING, ERROR, DEBUG, UNKNOWN };

// New Enum for Parse Errors
enum class ParseError {
  SUCCESS,
  FILE_OPEN_FAILED,
  INVALID_REGEX_PATTERN,
  PARTIAL_FAILURE // Some lines failed to parse but others succeeded
};

// New type for robust error handling (Iteration 1 Feature)
struct LogParseError {
  ParseError code = ParseError::SUCCESS;
  std::string message;
  size_t lineNumber = 0;

  bool operator==(const LogParseError &other) const {
    return code == other.code && message == other.message &&
           lineNumber == other.lineNumber;
  }
};

// New struct for comprehensive analysis results (Iteration 1 Feature)
struct AnalysisReport {
  size_t linesProcessed = 0;
  size_t successfulParses = 0;
  std::vector<std::pair<size_t, std::string>>
      parseErrors; // line number -> error reason
  ParseError status = ParseError::SUCCESS;
  std::string message; // Added message field
};

// New struct for time-gap analysis (Iteration 1 Feature)
struct TimeGap {
  std::chrono::system_clock::time_point start;
  std::chrono::system_clock::time_point end;
  std::chrono::system_clock::duration duration;
};

// New struct for time-windowed statistics (Iteration 1 Feature)
struct TimeWindowStats {
  std::chrono::system_clock::time_point windowStart;
  std::map<LogLevel, int> counts;
  int totalCount;
};

// LogEntry structure enhancement
struct LogEntry {
  size_t id; // New: Unique identifier for each log entry
  std::chrono::system_clock::time_point timestamp;
  LogLevel level;
  std::string message;

  bool operator==(const LogEntry &other) const {
    return id == other.id && timestamp == other.timestamp &&
           level == other.level && message == other.message;
  }
};

// Interface for pluggable log parsers
class ILogParser {
public:
  virtual ~ILogParser() = default;
  virtual std::optional<LogEntry> parseLine(std::string_view line,
                                            size_t lineNumber) const = 0;
  virtual std::unique_ptr<ILogParser> clone() const = 0;
  virtual std::string getLineFilterRegex() const { return ".*"; }
};

// Default implementation of ILogParser using regex
class DefaultLogParser : public ILogParser {
public:
  // Modified constructor to accept custom mappings
  DefaultLogParser(
      std::string pattern = "",
      const std::map<std::string, LogLevel, std::less<>> &mappings = {});
  std::optional<LogEntry> parseLine(std::string_view line,
                                    size_t lineNumber) const override;
  std::unique_ptr<ILogParser> clone() const override;

private:
  std::regex logPattern;
  std::string patternString; // Store pattern string to allow cloning
  std::map<std::string, LogLevel, std::less<>>
      customLevelMappings; // Use this copy
};

// FilterCriteria structure expansion
struct FilterCriteria {
  std::vector<LogLevel> levels;        // Multiple levels (empty means all)
  std::optional<LogLevel> minLogLevel; // Filter for this level and above
  std::string
      keyword; // Case-insensitive substring match (kept for compatibility)
  std::string regexPattern; // Optional: full regex pattern for message content.
                            // If provided, overrides 'keyword'.
  bool keywordCaseSensitive = false; // For 'keyword' field if used.

  std::optional<std::chrono::system_clock::time_point>
      startTime; // Optional start timestamp
  std::optional<std::chrono::system_clock::time_point>
      endTime; // Optional end timestamp
};

// Sorting Functionality enums
enum class SortBy { TIMESTAMP, LEVEL, MESSAGE };

// A view over a log file that provides iterators for lazy parsing.
class LogFileView {
public:
  LogFileView(const std::string &filePath, std::unique_ptr<ILogParser> parser,
              bool isTemporary = false)
      : filePath_(filePath), parser_(std::move(parser)),
        isTemporary_(isTemporary) {}

  ~LogFileView() {
    if (isTemporary_ && !filePath_.empty()) {
      std::remove(filePath_.c_str());
    }
  }

  // Support move semantics
  LogFileView(LogFileView &&other) noexcept
      : filePath_(std::move(other.filePath_)),
        parser_(std::move(other.parser_)), isTemporary_(other.isTemporary_) {
    other.isTemporary_ = false; // Prevent deletion by the moved-from object
  }

  LogFileView &operator=(LogFileView &&other) noexcept {
    if (this != &other) {
      if (isTemporary_ && !filePath_.empty()) {
        std::remove(filePath_.c_str());
      }
      filePath_ = std::move(other.filePath_);
      parser_ = std::move(other.parser_);
      isTemporary_ = other.isTemporary_;
      other.isTemporary_ = false;
    }
    return *this;
  }

  // Delete copy constructor and assignment
  LogFileView(const LogFileView &) = delete;
  LogFileView &operator=(const LogFileView &) = delete;

  // LogEntryIterator now needs the file path and a clone of the parser
  class LogEntryIterator {
  public:
    using iterator_category = std::input_iterator_tag;
    using value_type = std::expected<LogEntry, LogParseError>;
    using difference_type = std::ptrdiff_t;
    using pointer = const value_type *;
    using reference = const value_type &;

    LogEntryIterator() : isAtEnd_(true) {} // Default constructor for end iterator

    // Constructor for begin() iterator
    LogEntryIterator(const std::string &filePath, const ILogParser *parser)
        : fileStream_(std::make_unique<std::ifstream>()),
          parser_(parser->clone()), // Iterator owns its own cloned parser
          lineNumber_(0) {
      fileStream_->open(filePath);
      if (!fileStream_->is_open()) {
        isAtEnd_ = true;
        currentValue_ = std::unexpected(
            LogParseError{ParseError::FILE_OPEN_FAILED,
                          "File could not be opened by iterator.", 0});
        return;
      }
      readNextLine(); // Read the first entry
    }

    // Explicitly delete copy constructor and assignment operator to prevent
    // accidental copying of unique_ptr and associated stream state.
    LogEntryIterator(const LogEntryIterator &) = delete;
    LogEntryIterator &operator=(const LogEntryIterator &) = delete;

    // Allow move semantics
    LogEntryIterator(LogEntryIterator &&) = default;
    LogEntryIterator &operator=(LogEntryIterator &&) = default;

    reference operator*() const { return currentValue_; }
    pointer operator->() const { return &currentValue_; }
    LogEntryIterator &operator++() {
      if (!isAtEnd_) {
        readNextLine();
      }
      return *this;
    }
    LogEntryIterator operator++(int) { // Post-increment
      LogEntryIterator temp = std::move(*this); // Use move constructor
      ++(*this);
      return temp;
    }

    bool operator==(const LogEntryIterator &other) const {
      if (isAtEnd_ && other.isAtEnd_)
        return true; // Both are end iterators
      if (isAtEnd_ != other.isAtEnd_)
        return false; // One is end, other is not
      // Compare current values if both are not at end.
      // This might not be strictly necessary for input iterators,
      // but helps with robust comparisons.
      return (currentValue_ == other.currentValue_) &&
             (lineNumber_ == other.lineNumber_);
    }
    bool operator!=(const LogEntryIterator &other) const {
      return !(*this == other);
    }

  private:
    void readNextLine() {
      currentLine_.clear(); // Clear previous line content
      if (std::getline(*fileStream_, currentLine_)) {
        lineNumber_++;
        if (parser_) {
          if (auto parsedEntry =
                  parser_->parseLine(currentLine_, lineNumber_)) {
            currentValue_ = parsedEntry.value();
          } else {
            currentValue_ = std::unexpected(LogParseError{
                ParseError::PARTIAL_FAILURE,
                "Failed to parse log line: " + currentLine_, lineNumber_});
          }
        } else {
          currentValue_ = std::unexpected(LogParseError{
              ParseError::PARTIAL_FAILURE,
              "No parser available for log line: " + currentLine_, lineNumber_});
        }
      } else {
        isAtEnd_ = true;
        // The unique_ptr will close the file when it goes out of scope.
      }
    }

    std::unique_ptr<std::ifstream> fileStream_; // Each iterator owns its ifstream
    std::unique_ptr<ILogParser> parser_; // Each iterator owns its cloned parser
    std::string currentLine_;
    value_type currentValue_; // Stores the current LogEntry or error
    bool isAtEnd_ = false;
    size_t lineNumber_ = 0;
  };

  inline LogEntryIterator begin() const {
    return LogEntryIterator(filePath_, parser_.get()); // Pass path and parser ptr
  }

  inline LogEntryIterator end() const { return LogEntryIterator(); }

private:
  std::string filePath_;
  std::unique_ptr<ILogParser> parser_;
  bool isTemporary_ = false;
};

// --- Pluggable Analysis Framework ---
class ILogAnalyzer {
public:
  virtual ~ILogAnalyzer() = default;
  virtual void processEntry(const LogEntry &entry) = 0;
  virtual void finalize() = 0;
  // Concrete analyzers will provide methods to retrieve results
};

// --- Expressive Filtering System ---

// Base interface for all filter conditions
class IFilter {
public:
  virtual ~IFilter() = default;
  virtual bool matches(const LogEntry &entry) const = 0;
};

// Composite filter for AND/OR/NOT logic
class FilterSet : public IFilter {
public:
  enum class Logic { AND, OR };

  explicit FilterSet(Logic logic = Logic::AND);

  void add(std::shared_ptr<IFilter> filter);
  void negate();

  bool matches(const LogEntry &entry) const override;

private:
  Logic logic_;
  bool negated_ = false;
  std::vector<std::shared_ptr<IFilter>> filters_;
};

// Concrete Filter Implementations
class TimeRangeFilter : public IFilter {
public:
  TimeRangeFilter(std::chrono::system_clock::time_point start,
                  std::chrono::system_clock::time_point end)
      : startTime_(start), endTime_(end) {}

  bool matches(const LogEntry &entry) const override {
    return entry.timestamp >= startTime_ && entry.timestamp < endTime_;
  }

private:
  std::chrono::system_clock::time_point startTime_;
  std::chrono::system_clock::time_point endTime_;
};

class LevelFilter : public IFilter {
public:
  LevelFilter(LogLevel level) : targetLevel_(level) {}

  bool matches(const LogEntry &entry) const override {
    return entry.level == targetLevel_;
  }

private:
  LogLevel targetLevel_;
};

class KeywordFilter : public IFilter {
private:
  std::string keyword_;
  bool caseSensitive_;

public:
  KeywordFilter(std::string keyword, bool caseSensitive = false)
      : keyword_(std::move(keyword)), caseSensitive_(caseSensitive) {}

  bool matches(const LogEntry &entry) const override {
    std::string msg = entry.message;
    std::string key = keyword_;

    if (!caseSensitive_) {
      std::transform(msg.begin(), msg.end(), msg.begin(), ::tolower);
      std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    }
    return msg.find(key) != std::string::npos;
  }
};

class RegexFilter : public IFilter {
public:
  RegexFilter(
      std::string pattern,
      std::regex_constants::syntax_option_type flags = std::regex::ECMAScript);
  bool matches(const LogEntry &entry) const override;

private:
  std::regex pattern_;
};

// Adapter to use FilterCriteria with the new IFilter interface
class CriteriaToFilterAdapter : public IFilter {
public:
  explicit CriteriaToFilterAdapter(const FilterCriteria &criteria)
      : criteria_(criteria) {}

  bool matches(const LogEntry &entry) const override;

private:
  FilterCriteria criteria_;
};

// --- Asynchronous Operation Control ---
struct AsyncControl {
  std::jthread worker;
  std::shared_ptr<std::atomic<float>> progress;
  std::future<AnalysisReport> reportFuture;
};

enum class SortOrder { ASCENDING, DESCENDING };

// Main LogAnalyzer class, refactored for Iteration 2
class LogAnalyzer {
public:
  // Constructor
  LogAnalyzer();
  ~LogAnalyzer() = default;

  // --- Core Functionality (New Iterator-Based Model) ---
  std::expected<void, LogParseError>
  open(const std::string &filePath,
       std::unique_ptr<ILogParser> parser = nullptr);
  void setFilter(std::shared_ptr<IFilter> filter);
  void addFilter(std::shared_ptr<IFilter> filter); // Adds to a FilterSet (AND)
  void clearFilters();
  const LogFileView &getView() const; // Return by const reference

  // --- Asynchronous Operations ---
  [[nodiscard]]
  AsyncControl &load_async_cancellable(
      const std::string &filePath,
      const std::string &pattern = ""); // Return by reference

  std::future<AnalysisReport> load_async(const std::string &filePath,
                                         const std::string &pattern = "");

  // --- Analysis & Customization ---
  void runAnalysis(ILogAnalyzer &analyzer, const IFilter *filter = nullptr);
  static LogFileView merge_sorted(std::span<const LogFileView> sources);

  // --- Backward Compatibility & Deprecations ---
  const std::vector<LogEntry> &getEntries() const;

  std::vector<LogEntry>
  getFilteredEntries(const FilterCriteria &criteria) const;

  [[deprecated("Use static merge_sorted instead.")]]
  void merge(const LogAnalyzer &other);

  // --- Other public methods ---
  void clear();
  std::span<const LogEntry> entries_view() const;
  void setCustomLogLevelMapping(std::string_view levelString,
                                LogLevel mappedLevel);
  LogLevel resolveLogLevel(const std::string &levelStr) const;
  void exportAsCsv(std::ostream &out,
                   const FilterCriteria &filter = FilterCriteria{}) const;
  static std::string
  formatTimestamp(std::chrono::system_clock::time_point tp,
                  std::string_view format = "%Y-%m-%d %H:%M:%S");
  std::vector<LogEntry> getSortedFilteredEntries(const FilterCriteria &criteria,
                                                 SortBy sortBy,
                                                 SortOrder sortOrder) const;
  std::map<std::string, int> getUniqueMessageCounts() const;
  std::vector<std::pair<std::string, int>> getTopMessages(int n) const;
  void printFilteredEntries(std::ostream &out, const FilterCriteria &criteria,
                            std::string_view formatString) const;
  std::optional<LogEntry> findFirst(const FilterCriteria &criteria) const;
  std::optional<LogEntry> findLast(const FilterCriteria &criteria) const;
  std::string getSummaryString() const;
  std::expected<void, LogParseError> parseFile(const std::string &filePath,
                                               const std::string &pattern,
                                               bool appendMode);
  std::expected<void, LogParseError> load(const std::string &filePath,
                                          const std::string &pattern = "");
  std::expected<void, LogParseError> append(const std::string &filePath,
                                            const std::string &pattern = "");
  AnalysisReport analyze(const std::string &filePath,
                         const std::string &pattern = "");
  void analyzeStream(const std::string &filePath,
                     std::function<bool(const LogEntry &)> entryCallback,
                     const std::string &pattern = "");
  void printSummary(std::ostream &out) const;
  void exportAsJson(std::ostream &out, const FilterCriteria &filter,
                    bool includeSummary, bool prettyPrint) const;
  std::vector<TimeGap>
  findTimeGaps(std::chrono::milliseconds minGapDuration) const;
  double getAverageEntryRate() const;
  std::vector<TimeWindowStats>
  getFrequencyDistribution(std::chrono::seconds windowSize) const;
  std::vector<TimeWindowStats>
  getFrequencyDistributionOptimized(std::chrono::seconds windowSize) const;
  AnalysisReport getAnalysisReport() const { return lastReport; }
  std::vector<LogEntry> materializeEntries() const;

  // Helper functions
  static LogLevel stringToLogLevel(const std::string &levelStr);
  static std::string logLevelToString(LogLevel level);

private:
  // Private Members
  std::unique_ptr<ILogParser> defaultParser_;
  std::map<LogLevel, int> levelCounts;
  std::map<std::string, LogLevel, std::less<>> customLevelMappings;
  std::unique_ptr<LogFileView> log_source_view_;
  std::shared_ptr<IFilter> active_filter_;
  std::unique_ptr<AsyncControl> async_control_;
  const ILogParser *getCurrentParser() const;
  const IFilter *getActiveFilter() const;
  mutable AnalysisReport lastReport;
  mutable std::vector<LogEntry> entries_;
};
#endif // LOG_ANALYZER_H
