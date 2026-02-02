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

// --- Iterator-Based Processing & Ranges (The Core Change) ---

class LogEntryIterator {
public:
  using iterator_category = std::input_iterator_tag;
  using value_type = std::expected<LogEntry, LogParseError>;
  using difference_type = std::ptrdiff_t;
  using pointer = const value_type *;
  using reference = const value_type &;

  LogEntryIterator() : isAtEnd_(true) {} // Default constructor for end iterator
  LogEntryIterator(const LogEntryIterator &) = default; // Make copyable
  LogEntryIterator &
  operator=(const LogEntryIterator &) = default;              // Make copyable
  LogEntryIterator(LogEntryIterator &&) = default;            // Allow move
  LogEntryIterator &operator=(LogEntryIterator &&) = default; // Allow move

  // Constructor for begin() iterator
  LogEntryIterator(std::shared_ptr<std::ifstream> stream,
                   const ILogParser *parser)
      : fileStream_(std::move(stream)), parser_(parser), lineNumber_(0) {
    if (!fileStream_ || !fileStream_->is_open()) {
      isAtEnd_ = true;
      currentValue_ = std::unexpected(
          LogParseError{ParseError::FILE_OPEN_FAILED,
                        "File stream not open for iterator.", 0});
      return;
    }
    readNextLine(); // Read the first entry
  }

  reference operator*() const { return currentValue_; }
  pointer operator->() const { return &currentValue_; }
  LogEntryIterator &operator++() {
    if (!isAtEnd_) {
      readNextLine();
    }
    return *this;
  }
  LogEntryIterator operator++(int) { // Post-increment
    LogEntryIterator temp = *this;
    ++(*this);
    return temp;
  }

  bool operator==(const LogEntryIterator &other) const {
    if (isAtEnd_ && other.isAtEnd_)
      return true; // Both are end iterators
    if (isAtEnd_ != other.isAtEnd_)
      return false; // One is end, other is not
    // Compare stream positions and shared_ptr identity (if relevant)
    return fileStream_ == other.fileStream_ &&
           fileStream_->tellg() == other.fileStream_->tellg();
  }
  bool operator!=(const LogEntryIterator &other) const {
    return !(*this == other);
  }

private:
  void readNextLine() {
    currentLine_.clear(); // Clear previous line content
    // Store current stream position before reading
    // std::streampos currentPos = fileStream_->tellg(); // Not strictly needed
    // here, just read

    if (std::getline(*fileStream_, currentLine_)) {
      lineNumber_++;
      if (parser_) {
        if (auto parsedEntry = parser_->parseLine(currentLine_, lineNumber_)) {
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
      // No need to close the shared stream here. It will be closed when all
      // shared_ptr instances are destroyed.
    }
  }

  std::shared_ptr<std::ifstream> fileStream_; // Use shared_ptr for ifstream
  const ILogParser *parser_ =
      nullptr; // Raw pointer, ownership is with LogFileView
  std::string currentLine_;
  value_type currentValue_; // Stores the current LogEntry or error
  bool isAtEnd_ = false;
  size_t lineNumber_ = 0;
};

// A view over a log file that provides iterators for lazy parsing.
class LogFileView {
public:
  LogFileView(const std::string &filePath, std::unique_ptr<ILogParser> parser)
      : filePath_(filePath),
        parser_(std::move(
            parser)) // sharedFileStream_ is default-initialized (nullptr)
  {
    sharedFileStream_ = std::make_shared<std::ifstream>(); // Initialize here
    sharedFileStream_->open(filePath_);
    if (!sharedFileStream_->is_open()) {
      throw std::runtime_error("File could not be opened.");
    }
    // Error handling for file opening can be added here if necessary.
    // The iterator's constructor checks for !is_open() and reports an error.
  }

  inline LogEntryIterator begin() const {
    // Return an iterator initialized with the shared stream and parser
    return LogEntryIterator(sharedFileStream_, parser_.get());
  }

  inline LogEntryIterator end() const {
    // The end iterator is default-constructed
    return LogEntryIterator();
  }

private:
  std::string filePath_;
  std::unique_ptr<ILogParser> parser_;
  std::shared_ptr<std::ifstream> sharedFileStream_; // Managed by LogFileView

  friend class LogEntryIterator; // Grant friendship to access private members
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
  std::string
  formatTimestamp(std::chrono::system_clock::time_point tp,
                  std::string_view format = "%Y-%m-%d %H:%M:%S") const;
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
  std::map<LogLevel, int> levelCounts;
  std::map<std::string, LogLevel, std::less<>> customLevelMappings;
  std::unique_ptr<LogFileView> log_source_view_;
  std::shared_ptr<IFilter> active_filter_;
  std::unique_ptr<AsyncControl> async_control_;
  std::unique_ptr<ILogParser> defaultParser_;
  const ILogParser *getCurrentParser() const;
  const IFilter *getActiveFilter() const;
  AnalysisReport lastReport;
  mutable std::vector<LogEntry> entries_;
};
#endif // LOG_ANALYZER_H
