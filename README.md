# LogAnalyzer

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)

A powerful and memory-efficient C++ tool designed to analyze, filter, and extract insights from large log files.

## Table of Contents

-   [Features](#features)
-   [Getting Started](#getting-started)
-   [Prerequisites](#prerequisites)
-   [Building the Project](#building-the-project)
-   [Build Options](#build-options)
-   [Running Tests](#running-tests)
-   [Usage Examples](#usage-examples)
    -   [Command Line Interface (CLI)](#command-line-interface-cli)
    -   [Basic Analysis](#basic-analysis)
    -   [Filtering](#filtering)
    -   [Sorting and Statistics](#sorting-and-statistics)
    -   [Exporting Results](#exporting-results)
    -   [Advanced Configuration](#advanced-configuration)
-   [Developer Tools](#developer-tools)
-   [Example Log Format](#example-log-format)
-   [Contributing](#contributing)
-   [License](#license)

## Features

-   **Memory-Efficient Processing**: Utilizes a lazy, iterator-based approach to handle very large files with minimal memory usage. Operations like sorting or global statistics that require the full dataset will buffer entries in memory.
-   **Multi-File Support**: Parses multiple log files and can merge sorted sources efficiently.
-   **Pluggable Architecture**:
    -   **Custom Parsers**: Define your own log parsing logic by implementing the `ILogParser` interface.
    -   **Custom Analyzers**: Create custom analysis routines with the `ILogAnalyzer` interface.
-   **Advanced Filtering**: Build complex filter expressions with `AND`/`OR`/`NOT` logic. Support for:
    -   Log levels (e.g., `ERROR`, `WARNING`), or minimum level (e.g., `level >= WARNING`).
    -   Keywords (case-sensitive or insensitive).
    -   Regular expression patterns.
    -   Time ranges (start/end) with support for absolute (`YYYY-MM-DD HH:MM:SS`), relative (`1h ago`), and Unix timestamps.
    -   Source file and function name filtering.
-   **Asynchronous Processing**: Load and analyze files asynchronously with cancellation support.
-   **Live Tail Mode**: Monitor new log entries in real-time.
-   **Customizable Parsing**:
    -   Map custom log level strings (e.g., `FATAL=ERROR`).
    -   Custom timestamp formats using `strftime` patterns.
    -   Configurable log line patterns via regular expressions.
-   **Statistical Analysis**:
    -   Unique message counts and top N frequent messages.
    -   Log frequency distribution over time windows.
    -   Average log entry rate and time gap identification.
    -   Percentile analysis (e.g., p99) and log burst detection.
-   **Flexible Export**: Save results in Text, JSON, CSV, YAML, or XML formats.
-   **Contextual Viewing**: Display surrounding lines (N before, M after) for filtered entries.

## Getting Started

Follow these steps to quickly build and run `logAnalyzer` on your system.

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/your-username/logAnalyzer.git # Replace with actual repo URL
    cd logAnalyzer
    ```
2.  **Build the project:**
    ```bash
    mkdir build
    cd build
    cmake ..
    make
    ```
3.  **Run a basic analysis:**
    ```bash
    ./logAnalyzer ../logs/app.log # Assuming you have a log file named app.log in a 'logs' directory
    ```
    (You might need to create a sample `app.log` or adjust the path to an existing log file.)

## Prerequisites

-   **Compiler**: C++23 compatible compiler (e.g., GCC 13+, Clang 16+)
-   **Build System**: CMake 3.20 or higher
-   **Dependencies**: (Automatically handled via FetchContent during CMake configuration)
    -   [CLI11](https://github.com/CLIUtils/CLI11): A header-only library for command line parsing.
    -   [nlohmann/json](https://github.com/nlohmann/json): A header-only C++ JSON library.
    -   [GoogleTest](https://github.com/google/googletest): For comprehensive unit and integration testing.

## Building the Project

To build the `logAnalyzer` executable and its associated libraries:

```bash
mkdir build
cd build
cmake ..
make
```
This will compile the project and place the executable in `build/bin/logAnalyzer`.

### Build Options

You can customize the build process using these CMake options:

-   `-DBUILD_TESTING=ON/OFF`: Toggles the compilation of unit tests (Default: `ON`).
-   `-DLOGANALYZER_BUILD_SHARED=ON/OFF`: Determines whether to build `logAnalyzer` as a shared library (Default: `OFF`).
-   `-DLOGANALYZER_USE_SANITIZER=Address/Undefined/None`: Enables various sanitizers for debugging and performance analysis (Default: `None`). Options include `Address` (AddressSanitizer) and `Undefined` (UndefinedBehaviorSanitizer).

## Running Tests

After building the project, you can execute the test suite:

```bash
cd build
ctest --verbose
```
This command runs all configured tests and provides detailed output.

## Usage Examples

### Command Line Interface (CLI)

The `logAnalyzer` tool is primarily operated via command-line arguments. For a complete list of available commands and options, use the `--help` flag:

```bash
./logAnalyzer --help
```

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

# Exclude entries with a specific keyword
./logAnalyzer app.log --exclude-keyword "debug_message"

# Filter by time range
./logAnalyzer app.log --start "2023-10-27 10:00:00" --end "1h ago"

# Regular expression filter
./logAnalyzer app.log --regex "Connection (timed out|refused)"

# Exclude entries matching a regular expression
./logAnalyzer app.log --exclude-regex "InternalError \d{3}"

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

If Doxygen and Clang-Format are installed on your system, you can use these convenient targets:

```bash
# Generate API documentation (HTML, LaTeX, etc.)
make doc

# Format source code according to project guidelines
make format
```

## Example Log Format

The default parser expects log entries in the following format:

```
[2023-10-27 10:00:00.123] INFO: Application started
[2023-10-27 10:01:00.456] DEBUG: Initializing components
```
However, `logAnalyzer` is highly configurable and can parse almost any log format using the `--log-pattern` option, allowing you to define custom regular expressions for your log lines.

## Contributing

We welcome contributions to `logAnalyzer`! If you have suggestions, bug reports, or want to contribute code, please feel free to:

-   Open an issue on the [GitHub repository](https://github.com/your-username/logAnalyzer/issues).
-   Submit a pull request.

Please ensure your code adheres to the project's coding style and includes appropriate tests.

## License

This project is licensed under the MIT License - see the [LICENSE](https://opensource.org/licenses/MIT) file for details.
