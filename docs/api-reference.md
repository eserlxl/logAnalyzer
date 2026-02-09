# C++ API Reference

The `logAnalyzer` core functionality is exposed through a C++ API, allowing developers to integrate log analysis capabilities directly into their own applications.

## Core Class: `LogAnalyzer`

The `LogAnalyzer` class is the main entry point for the library. It manages log settings, parsing, filtering, statistics, and export.

### Header
```cpp
#include "analyzer/Core.h"
```

### Initialization

```cpp
// Initialize with default settings
LogAnalyzer analyzer;

// Initialize with custom settings
LogAnalyzerSettings settings;
// ... configure settings ...
LogAnalyzer analyzer(settings);
```

### Loading Logs

#### Synchronous Load
Load a log file completely into memory.

```cpp
ErrorCode::Result<AnalysisReport> load(
    const std::string& filePath, 
    CLIConfig::ParserErrorAction errorAction = CLIConfig::ParserErrorAction::LOG,
    std::optional<CancellationToken*> cancellationToken = std::nullopt,
    std::optional<ProgressCallback> progressCallback = std::nullopt
);
```

#### Asynchronous Load
Load a log file asynchronously.

```cpp
std::future<ErrorCode::Result<AnalysisReport>> loadAsync(
    const std::string& filePath, 
    CLIConfig::ParserErrorAction errorAction,
    std::shared_ptr<CancellationToken> cancellationToken = nullptr,
    std::optional<ProgressCallback> progressCallback = std::nullopt
);
```

#### Stream Processing
Process a stream without loading everything into memory (useful for large files).

```cpp
ErrorCode::Result<AnalysisReport> streamIn(
    std::istream& is, 
    const std::string& sourceIdentifier, 
    CLIConfig::ParserErrorAction errorAction,
    // ...
);
```

### Filtering

Retrieve log entries that match specific criteria.

```cpp
// Using the new FilterExpression system
ErrorCode::Result<std::vector<LogEntry>> getFilteredEntries(
    const filter::FilterExpression& expression
) const;

// Streaming filtered entries (C++23 generator)
std::generator<const LogEntry&> streamFilteredEntries(
    const filter::FilterExpression& expression
) const;
```

### Exporting

Export filtered logs to various formats.

```cpp
// Export to CSV
void exportAsCsv(
    std::ostream& out, 
    const filter::FilterExpression& expression, 
    char delimiter = ',', 
    std::string_view timestampFormat = "%Y-%m-%dT%H:%M:%S.%fZ"
) const;

// Export to JSON
void exportAsJson(
    std::ostream& out, 
    const filter::FilterExpression& expression, 
    bool prettyPrint, 
    std::string_view timestampFormat = "%Y-%m-%dT%H:%M:%S.%fZ"
) const;
```

### Statistics

Configure and retrieve statistics.

```cpp
// Add a collector
analyzer.addStatisticCollector(std::make_shared<TopMessagesCollector>(5));

// Process entries
analyzer.processEntriesForStatistics(analyzer.getEntriesView());

// Get reports
std::map<std::string, json> reports = analyzer.getAllStatisticReports();
```

## Data Structures

### `LogEntry`

Represents a single log line.

```cpp
struct LogEntry {
    std::string id;
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string message;
    std::string source; // Filename or identifier
    size_t lineNumber;
    std::map<std::string, std::string> customFields;
    // ...
};
```

### `AnalysisReport`

Contains summary information about a load operation.

```cpp
struct AnalysisReport {
    size_t totalLines;
    size_t processedLines;
    size_t skippedLines;
    size_t errorLines;
    std::chrono::milliseconds duration;
    // ...
};
```

## Error Handling

The API uses `ErrorCode::Result<T>` (aliased to `std::expected` or similar) to return results or errors.

```cpp
auto result = analyzer.load("app.log");
if (!result) {
    std::cerr << "Error: " << result.error().message << '\n';
}
```

### Error Boundary Policy

- Public operational APIs are designed to report recoverable failures via `ErrorCode::Result<T>` instead of throwing (for example: `load*`, `append`, `streamIn`, `analyzeStream`, `getFilteredEntries`, `setSettings`).
- Invalid user input (bad files, invalid regex/settings, malformed expression/config data) should surface as structured error codes/messages.
- Constructors may still throw only for unrecoverable initialization failures after fallback attempts.
- Deprecated legacy overloads returning `std::expected<void, LogParseError>` preserve their existing contract.
