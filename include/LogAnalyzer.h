#ifndef LOG_ANALYZER_H
#define LOG_ANALYZER_H

#include "LogTypes.h"  // Include LogTypes first to ensure LogEntry is defined
#include "LogParser.h" // Then include LogParser, which uses ParseResult and ILogParser
#include "Filter.h"    // Include the new Filter header
#include "Statistics.h"
#include "Exporter.h"

#include <atomic>
#include <chrono>
#include <expected>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <ranges>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <mutex>

// FilterCriteria structure expansion
struct FilterCriteria {
  std::vector<LogLevel> levels;
  std::string keyword;
  std::string regexPattern;
  bool keywordCaseSensitive = false;

  std::optional<std::chrono::system_clock::time_point> startTime;
  std::optional<std::chrono::system_clock::time_point> endTime;
};

// Sorting Functionality enums
enum class SortBy { TIMESTAMP, LEVEL, MESSAGE };

// A view over a log file that provides iterators for lazy parsing.
class LogFileView {
private:
    struct ManagedFile {
        std::string filePath;
        bool isTemporary;

        ManagedFile(std::string path, bool temporary)
            : filePath(std::move(path)), isTemporary(temporary) {}

        ~ManagedFile() {
            if (isTemporary && !filePath.empty()) {
                std::remove(filePath.c_str());
            }
        }
    };

public:
    LogFileView(const std::string &filePath, std::unique_ptr<ILogParser> parser,
                bool isTemporary = false)
        : file_(std::make_shared<ManagedFile>(filePath, isTemporary)),
          parser_(std::move(parser)) {}

    LogFileView(LogFileView &&) noexcept = default;
    LogFileView &operator=(LogFileView &&) noexcept = default;
    LogFileView(const LogFileView &) = delete;
    LogFileView &operator=(const LogFileView &) = delete;

    class LogEntryIterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = std::expected<LogEntry, LogParseError>;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type *;
        using reference = const value_type &;

        LogEntryIterator() : isAtEnd_(true) {}

        LogEntryIterator(std::shared_ptr<ManagedFile> file,
                         const ILogParser *parser)
            : file_(std::move(file)),
              fileStream_(std::make_unique<std::ifstream>()),
              parser_(parser->clone()), lineNumber_(0) {
            if (!file_ || file_->filePath.empty()) {
                isAtEnd_ = true;
                currentValue_ = std::unexpected(LogParseError{
                    ParseError::FILE_OPEN_FAILED, "No file specified.", 0});
                return;
            }
            fileStream_->open(file_->filePath);
            if (!fileStream_->is_open()) {
                isAtEnd_ = true;
                currentValue_ =
                    std::unexpected(LogParseError{ParseError::FILE_OPEN_FAILED,
                                                  "File could not be opened.", 0});
                return;
            }
            readNextLine();
        }

        LogEntryIterator(const LogEntryIterator &) = delete;
        LogEntryIterator &operator=(const LogEntryIterator &) = delete;
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

        LogEntryIterator operator++(int) {
            LogEntryIterator temp = std::move(*this);
            ++(*this);
            return temp;
        }

        bool operator==(const LogEntryIterator &other) const {
            if (isAtEnd_ && other.isAtEnd_)
                return true;
            if (isAtEnd_ != other.isAtEnd_)
                return false;
            // Compare file paths and line numbers for equality.
            // If both files are valid, compare paths. If one is null, they are not equal unless both are null and at end.
            if (file_ && other.file_) {
                return (file_->filePath == other.file_->filePath && lineNumber_ == other.lineNumber_);
            }
            // If one is null and the other is not, they are not equal unless both are at end state.
            // If both are null, it implies an invalid iterator state, which should ideally not happen if file_ is checked.
            // However, for robustness, if both are null, consider them equal only if both are at the end.
            return false; // Should not reach here if file_ is properly managed
        }

        bool operator!=(const LogEntryIterator &other) const {
            return !(*this == other);
        }

    private:
        void readNextLine() {
            std::string line;
            if (std::getline(*fileStream_, line)) {
                lineNumber_++;
                ParseResult result = parser_->parseLine(line, lineNumber_);
                if (result.success && result.entry) {
                    currentValue_ = std::move(*result.entry);
                } else {
                    currentValue_ = std::unexpected(
                        LogParseError{ParseError::PARTIAL_FAILURE,
                                      result.errorMessage, lineNumber_});
                }
            } else {
                isAtEnd_ = true;
            }
        }

        std::shared_ptr<ManagedFile> file_;
        std::unique_ptr<std::ifstream> fileStream_;
        std::unique_ptr<ILogParser> parser_;
        value_type currentValue_;
        bool isAtEnd_ = false;
        size_t lineNumber_ = 0;
    };

    inline LogEntryIterator begin() const {
        return LogEntryIterator(file_, parser_.get());
    }
    inline LogEntryIterator end() const { return LogEntryIterator(); }

private:
    std::shared_ptr<ManagedFile> file_;
    std::unique_ptr<ILogParser> parser_;
};

class ILogAnalyzer {
public:
  virtual ~ILogAnalyzer() = default;
  virtual void processEntry(const LogEntry &entry) = 0;
  virtual void finalize() = 0;
};

struct AsyncControl {
  std::jthread worker;
  std::shared_ptr<std::atomic<float>> progress;
  std::future<AnalysisReport> reportFuture;
};

enum class SortOrder { ASCENDING, DESCENDING };

class LogAnalyzer {
public:
  LogAnalyzer();
  ~LogAnalyzer() = default;

  void open(const std::string &filePath,
       std::unique_ptr<ILogParser> parser = nullptr);
  void setFilter(std::shared_ptr<IFilter> filter);
  void addFilter(std::shared_ptr<IFilter> filter);
  void clearFilters();
  const LogFileView &getView() const;

  [[nodiscard]]
  AsyncControl &loadAsyncCancellable( // Renamed from load_async_cancellable
      const std::string &filePath,
      const std::string &pattern = "");

  std::future<AnalysisReport> loadAsync(const std::string &filePath, // Renamed from load_async
                                         const std::string &pattern = "");

  void runAnalysis(ILogAnalyzer &analyzer, const IFilter *filter = nullptr);
  static LogFileView merge_sorted(std::span<LogFileView> sources);

  const std::vector<LogEntry> &getEntries() const;
  std::vector<LogEntry>
  getFilteredEntries(const FilterCriteria &criteria) const;

  [[deprecated("Use static merge_sorted instead.")]]
  void merge(const LogAnalyzer &other);

  void clear();
  std::span<const LogEntry> entriesView() const; // Renamed from entries_view
  void setCustomLogLevelMapping(std::string_view levelString,
                                LogLevel mappedLevel);
  LogLevel resolveLogLevel(const std::string &levelStr) const;
  void exportAsCsv(std::ostream &out,
                   const FilterCriteria &filter = FilterCriteria{}, char delimiter = ',') const;
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
  void analyzeStream(const std::vector<std::string> &filePaths,
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
  getFrequencyDistributionOptimized(std::chrono::seconds windowSize) const; // This will be removed or refactored
  AnalysisReport getAnalysisReport() const { return lastReport; }
  std::vector<LogEntry> materializeEntries() const;

  std::string formatEntry(const LogEntry &entry, std::string_view format, bool useColor) const;

  static LogLevel stringToLogLevel(const std::string &levelStr);
  static std::string logLevelToString(LogLevel level);

private:
  std::unique_ptr<ILogParser> defaultParser_;
  std::map<LogLevel, int> levelCounts;
  std::map<std::string, LogLevel, std::less<>> customLevelMappings;
  
  mutable std::mutex mutex_; // Moved to be before data members it protects
  
  std::unique_ptr<LogFileView> logSourceView_; // Renamed from log_source_view_
  std::shared_ptr<IFilter> activeFilter_; // Renamed from active_filter_
  std::unique_ptr<AsyncControl> asyncControl_; // Renamed from async_control_

  mutable AnalysisReport lastReport;
  mutable std::vector<LogEntry> entries_;
  
  const ILogParser *getCurrentParser() const;
  const IFilter *getActiveFilter() const;
  
};
#endif // LOG_ANALYZER_H
