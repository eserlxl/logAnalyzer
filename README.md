# LogAnalyzer

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()

A powerful and memory-efficient C++ tool designed to analyze, filter, and extract insights from large log files.

## Features

- **Memory-Efficient Processing**: Utilizes a lazy, iterator-based approach to handle very large files with minimal memory usage. Operations like sorting or global statistics that require the full dataset will buffer entries in memory.
- **Multi-File Support**: Parses multiple log files and can merge sorted sources efficiently.
- **Pluggable Architecture**:
    - **Custom Parsers**: Define your own log parsing logic by implementing the `ILogParser` interface.
    - **Custom Analyzers**: Create custom analysis routines with the `ILogAnalyzer` interface.
- **Advanced Filtering**: Build complex filter expressions with `AND`/`OR`/`NOT` logic. Support for:
    - Log levels (e.g., `ERROR`, `WARNING`), or minimum level (e.g., `level >= WARNING`).
    - Keywords (case-sensitive or insensitive).
    - Regular expression patterns.
    - Time ranges (start/end) with support for absolute (`YYYY-MM-DD HH:MM:SS`), relative (`1h ago`), and Unix timestamps.
    - Source file and function name filtering.
- **Asynchronous Processing**: Load and analyze files asynchronously with cancellation support.
- **Live Tail Mode**: Monitor new log entries in real-time.
- **Customizable Parsing**:
    - Map custom log level strings (e.g., `FATAL=ERROR`).
    - Custom timestamp formats using `strftime` patterns.
    - Configurable log line patterns via regular expressions.
- **Statistical Analysis**:
    - Unique message counts and top N frequent messages.
    - Log frequency distribution over time windows.
    - Average log entry rate and time gap identification.
    - Percentile analysis (e.g., p99) and log burst detection.
- **Flexible Export**: Save results in Text, JSON, CSV, YAML, or XML formats.
- **Contextual Viewing**: Display surrounding lines (N before, M after) for filtered entries.

## Prerequisites

- **Compiler**: C++23 compatible compiler (e.g., GCC 13+, Clang 16+)
- **Build System**: CMake 3.20 or higher
- **Dependencies**: (Automatically handled via FetchContent)
    - [CLI11](https://github.com/CLIUtils/CLI11)
    - [nlohmann/json](https://github.com/nlohmann/json)
    - [GoogleTest](https://github.com/google/googletest) (for testing)

## Building the Project

```bash
mkdir build
cd build
cmake ..
make
```

### Build Options

- `-DBUILD_TESTING=ON/OFF`: Build unit tests (Default: ON).
- `-DLOGANALYZER_BUILD_SHARED=ON/OFF`: Build as a shared library (Default: OFF).
- `-DLOGANALYZER_USE_SANITIZER=Address/Undefined/None`: Enable sanitizers (Default: None).

## Running Tests

```bash
cd build
ctest --verbose
```

## Usage Examples

### Basic Analysis
```bash
# Summary of a single log file
./logAnalyzer logs/app.log

# Analyze multiple files
./logAnalyzer log1.log log2.log

# Save summary to file
./logAnalyzer app.log --output summary.txt
```

### Filtering
```bash
# Filter by level and keyword
./logAnalyzer app.log --level ERROR,WARNING --keyword "database"

# Filter by time range
./logAnalyzer app.log --start "2023-10-27 10:00:00" --end "1h ago"

# Regular expression filter
./logAnalyzer app.log --regex "Connection (timed out|refused)"

# Contextual view (2 lines before, 1 after)
./logAnalyzer app.log --level CRITICAL --context 2,1
```

### Sorting and Statistics
```bash
# Sort by message descending
./logAnalyzer app.log --sort-by msg --order desc

# Top 5 frequent messages
./logAnalyzer app.log --top-messages 5

# Log frequency distribution in 5-minute windows
./logAnalyzer app.log --stats-window 5m

# Show average entry rate
./logAnalyzer app.log --rate
```

### Exporting Results
```bash
# Export to JSON (pretty-printed)
./logAnalyzer app.log --format json --pretty --output results.json

# Export to CSV with custom separator
./logAnalyzer app.log --format csv --output results.csv
```

### Advanced Configuration
```bash
# Custom log pattern and field mapping
./logAnalyzer app.log --log-pattern "^\\[(.*?)\\] ([A-Z]+): (.*)$" --pattern-fields "timestamp,level,message"

# Map custom levels
./logAnalyzer app.log --map-level FATAL=ERROR --level ERROR
```

## Developer Tools

If Doxygen and Clang-Format are installed, you can use:

```bash
# Generate API documentation
make doc

# Format source code
make format
```

## Example Log Format
The default parser expects:
```
[2023-10-27 10:00:00.123] INFO: Application started
[2023-10-27 10:01:00.456] DEBUG: Initializing components
```
But can be easily configured for almost any format using `--log-pattern`.
