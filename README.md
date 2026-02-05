# logAnalyzer

A high-performance C++ command-line utility for advanced log analysis, filtering, and statistical insights.

[![Build Status](https://github.com/eserlxl/logAnalyzer/actions/workflows/cmake.yml/badge.svg)](https://github.com/eserlxl/logAnalyzer/actions)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Code style: clang-format](https://img.shields.io/badge/code%20style-clang--format-blue.svg)](https://clang.llvm.org/docs/ClangFormat.html)
[![Doxygen Documentation](https://img.shields.io/badge/docs-Doxygen-blue.svg)](https://eserlxl.github.io/logAnalyzer/)

## Overview

`logAnalyzer` is a high-performance command-line utility built in C++ that enables detailed analysis, filtering, and extraction of insights from large log files. It is designed for efficiency, handling massive datasets with minimal memory footprint by processing logs as streams.

## Why logAnalyzer?

In a world of ever-growing log files, traditional tools like `grep`, `awk`, and `sed` can become cumbersome and slow. `logAnalyzer` was built to address these challenges by providing:

-   **Performance**: A C++ core that processes large volumes of data quickly.
-   **Structured Filtering**: Go beyond simple text matching with filters for log levels, timestamps, and structured data.
-   **Ease of Use**: A single, powerful CLI that combines the functionality of multiple tools.
-   **Low Memory Usage**: Stream processing for analyzing files that are too large to fit in memory.

## Table of Contents

-   [Features](#features)
-   [Getting Started](#getting-started)
-   [Usage](#usage)
-   [Command-Line Interface (CLI)](#command-line-interface-cli)
-   [Configuration](#configuration)
-   [Project Structure](#project-structure)
-   [Running Tests](#running-tests)
-   [Developer Tools](#developer-tools)
-   [Contributing](#contributing)
-   [Code of Conduct](#code-of-conduct)
-   [License](#license)

## Features

| Feature                      | Description                                                                          |
| ---------------------------- | ------------------------------------------------------------------------------------ |
| **Memory-Efficient Processing** | Handles very large files with minimal memory usage using the `--stream` mode.        |
| **Multi-File Support**       | Parses and analyzes multiple log files at once.                                      |
| **Structured Field Parsing** | Automatically parses log messages into key-value pairs using custom delimiters.      |
| **Advanced Filtering**       | Filter by keywords, regular expressions, log levels, and time ranges.                |
| **Complex Filtering Expressions** | Build sophisticated filter logic using parenthesized, nested AND/OR conditions.      |
| **Live Tailing**             | Monitor log files for new entries in real-time (`tail -f` like behavior).            |
| **Flexible Export**          | Save results in Text, JSON, or CSV formats.                                          |
| **Statistical Analysis**     | Generate statistics on your log data, such as entry rates and top messages.          |

## Getting Started

### 1. Prerequisites

-   **Compiler**: A C++23 compatible compiler (e.g., GCC 13+, Clang 16+).
-   **Build System**: CMake 3.20 or higher.
-   **Version Control**: Git for cloning the repository.

### 2. Clone the Repository

First, clone the repository and navigate into the project directory:

```bash
git clone https://github.com/eserlxl/logAnalyzer.git
cd logAnalyzer
```

### 3. Build the Project

Next, use CMake to configure and build the project. We recommend an out-of-source build.

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

The compiled `logAnalyzer` executable will be available in the `build/bin` directory.

#### Build Options

You can customize the build with the following CMake options:

-   `-DBUILD_TESTING=ON/OFF`: Enable or disable the compilation of tests (default: `ON`).
-   `-DLOGANALYZER_BUILD_SHARED=ON/OFF`: Build `logAnalyzer` as a shared library (default: `OFF`).
-   `-DLOGANALYZER_USE_SANITIZER=Address/Undefined/None`: Enable sanitizers for debugging (default: `None`).

To use an option, add it to the `cmake` command:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
```

### 4. (Optional) Install `logAnalyzer`

To install the `logAnalyzer` executable to a system-wide location (e.g., `/usr/local/bin`), run the following command from the `build` directory. This allows you to run `logAnalyzer` from any directory.

```bash
# Use sudo for system-wide installation
sudo cmake --install . --prefix /usr/local
```

For a local installation, you can specify a different prefix. This is useful if you don't have administrative privileges.

```bash
# Install to a 'dist' directory inside the project folder
cmake --install . --prefix ../dist
```

## Usage

After building, you can run `logAnalyzer` in two ways:

1.  **From the build directory**:
    ```bash
    ./build/bin/logAnalyzer [options] <log_file(s)>
    ```

2.  **As an installed command** (if you completed the installation step):
    ```bash
    logAnalyzer [options] <log_file(s)>
    ```

**Note**: The following examples assume `logAnalyzer` is in your `PATH` (i.e., installed). If not, replace `logAnalyzer` with the path to the executable (e.g., `./build/bin/logAnalyzer`).

### Example 1: Basic Filtering

```bash
# Find all errors containing the word "database" in a specific log file
logAnalyzer /var/log/app.log --level ERROR --keyword "database"

# Find all entries in two different log files, excluding those containing "DEBUG"
logAnalyzer app.log kern.log --exclude-keyword "DEBUG"
```

### Example 2: Advanced Filtering and Output

```bash
# Find entries that are either warnings or errors, and contain "timeout" OR "refused"
logAnalyzer access.log --level WARNING --level ERROR --keyword "timeout" --keyword "refused" --logic OR

# Export errors between two dates to a pretty-printed JSON file
logAnalyzer system.log --level ERROR --start "2023-11-01 00:00:00" --end "2023-11-02 00:00:00" --format json --pretty --output errors.json
```

### Example 3: Complex Expression

```bash
# Use a complex expression to find database errors or any message containing "timeout"
logAnalyzer app.log --expression '(level=ERROR and msg contains "database") or msg contains "timeout"'
```

### Example 4: Stream a Large File

```bash
# Process a large log file without loading it all into memory, saving errors to a file
logAnalyzer large_log.log --stream --level ERROR --output filtered_errors.txt
```

### Example 5: Process Logs from Standard Input

`logAnalyzer` supports reading from `stdin`, making it easy to integrate into pipelines.

```bash
# Pipe logs from another command and filter for errors
cat /var/log/syslog | logAnalyzer --stdin --level ERROR

# Tail a file and filter for a keyword. Use '-' as the filename to signify stdin.
tail -f /var/log/app.log | logAnalyzer - --keyword "error"
```

### Example 6: Statistical Analysis

```bash
# Get the top 5 most common error messages from a log file
logAnalyzer system.log --level ERROR --stats top_messages:5
```

## Command-Line Interface (CLI)

`logAnalyzer` provides a rich command-line interface for ad-hoc analysis.

Run `logAnalyzer --help` for a full list of commands.

### General Options

| Option                 | Shorthand | Description                                                                | Default   |
| ---------------------- | --------- | -------------------------------------------------------------------------- | --------- |
| `--help`               | `-h`      | Shows the help message.                                                    |           |
| `--config FILE`        |           | Load configuration from a JSON file.                                       |           |
| `--pattern REGEX`      |           | Custom regex for parsing log lines (overrides config).                     |           |
| `--output FILE`        |           | Redirect output to a file.                                                 | `(stdout)`|
| `--color OPT`          |           | Controls colorized output (`always`, `auto`, `never`).                     | `auto`    |
| `--stream`             |           | Enable stream mode for large files (low memory usage).                     | `false`   |
| `--on-parse-error OPT` |           | Action on parse errors (`ignore`, `warn`, `throw`).                        | `warn`    |
| `--stdin`              |           | Read log entries from standard input. Also activated by using `-` as a filename. | `false`   |

### Filtering and Sorting

| Option                   | Description                                                                                                                                                             | Default |
| ------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------- |
| `--keyword TEXT`         | Keyword/phrase to filter for. Multiple uses are combined with `--logic`.                                                                                                |         |
| `--exclude-keyword TEXT` | Keyword/phrase to exclude. Can be used multiple times.                                                                                                                  |         |
| `--regex PATTERN`        | Regex pattern to filter for. Multiple uses are combined with `--logic`.                                                                                                 |         |
| `--exclude-regex PATTERN`| Regex pattern to exclude. Can be used multiple times.                                                                                                                   |         |
| `--logic [AND\|OR]`      | Logic for combining multiple `--keyword` or `--regex` rules.                                                                                                            | `AND`   |
| `--case-sensitive`       | Makes keyword filtering case-sensitive.                                                                                                                                 | `false` |
| `--level LEVEL`          | Log level to include (e.g., `ERROR`). Can be used multiple times.                                                                                                       |         |
| `--min-level LEVEL`      | Minimum log level to include (e.g., `WARNING`).                                                                                                                         |         |
| `--map-level KEY=LEVEL`  | Map custom log levels (e.g., `TRC=TRACE`).                                                                                                                              |         |
| `--start TIME`           | Filter logs after a given timestamp. Accepts absolute time (e.g., `"2023-10-27 10:00:00"`), relative time (e.g., `"1h ago"`, `"yesterday"`), Unix timestamps, or ISO 8601. |         |
| `--end TIME`             | Filter logs before a given timestamp. Accepts the same formats as `--start`.                                                                                            |         |
| `--duration DURATION`    | Duration for time filtering (e.g., '30m', '1h'). Must be used with `--start` or `--end`. Accepts `s` (seconds), `m` (minutes), `h` (hours), or `d` (days).                 |         |
| `--expression "EXPR"`    | Complex filter expression using nested logic (e.g., `(level=ERROR and msg contains "db") or msg contains "timeout"`).                                                   |         |
| `--sort-by [time\|level\|msg]` | Field to sort results by.                                                                                                                                             | `time`  |
| `--order [asc\|desc]`    | Sort order.                                                                                                                                                             | `asc`   |

### Output Formatting

| Option                  | Description                                                                                                                             | Default                         |
| ----------------------- | --------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------- |
| `--format [text\|json\|csv]`  | Sets the output format.                                                                                                                 | `text`                          |
| `--text-format TEXT`    | Custom format string for `text` output. Available: `{timestamp}`, `{level}`, `{message}`, `{lineNumber}`, `{fileName}`, `{elapsedTime}`. | `{timestamp} {level}: {message}`|
| `--csv-sep CHAR`        | Separator character for `csv` output.                                                                                                   | `,`                             |
| `--csv-fields "FIELDS"` | Comma-separated fields for `csv` output (e.g., `timestamp,level,message`).                                                              |                                 |
| `--json-fields "FIELDS"`| Comma-separated fields for `json` output (e.g., `timestamp,level,message`).                                                             |                                 |
| `--pretty`              | Pretty-print `json` output.                                                                                                             | `false`                         |
| `--include-summary`     | Include a summary section in `json` output.                                                                                             | `false`                         |

### Statistics

| Option             | Description                                                                                                       | Default |
| ------------------ | ----------------------------------------------------------------------------------------------------------------- | ------- |
| `--stats NAME`     | Enable a statistic collector. Available: `unique_messages`, `top_messages[:N]`, `entry_rate`. Can be used multiple times. |         |
| `--top-n N`        | Sets 'N' for `top_messages` if not specified directly (e.g., `top_messages:10`).                                     | 10      |
| `--stats-window SEC` | Shows log frequency distribution over a time window in seconds.                                                   |         |
| `--find-gaps MS`   | Finds time gaps in logs longer than the specified milliseconds.                                                   |         |

### Tailing (Live Mode)

Monitor files for new lines, similar to `tail -f`. Not compatible with `--stdin`.

| Option             | Description                                          | Default |
| ------------------ | ---------------------------------------------------- | ------- |
| `--tail`           | Enable tail mode to watch files for new entries.     | `false` |
| `--tail-interval MS` | Polling interval in milliseconds for tail mode.      | 1000    |

## Configuration

`logAnalyzer` can be configured using a JSON file for persistent setups. Use the `--config` option to load a file. Command-line arguments override settings from the configuration file.

An example `config.json`:

```json
{
  "lineParsePattern": "^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) ([A-Z]+): (.*)$",
  "fieldMappings": [
    { "field": "timestamp", "groupIndex": 1 },
    { "field": "level", "groupIndex": 2 },
    { "field": "message", "groupIndex": 3 }
  ],
  "customLogLevelMappings": {
    "INFO": "INFO",
    "WARN": "WARNING",
    "ERROR": "ERROR"
  },
  "logEntryStartPattern": "^\d{4}-\d{2}-\d{2}",
  "caseSensitiveParsing": false,
  "filterRules": [
    { "field": "level", "operator": "EQ", "value": "ERROR" }
  ],
  // `filterRules` support complex conditions based on log entry fields.
  // Consult the Doxygen documentation for a full list of supported fields and operators.
  "exportSettings": {
    "format": "json",
    "fieldsToExport": ["timestamp", "level", "message"],
    "outputFile": "analysis_results.json"
  },
  "statisticConfigs": [
    { "type": "UNIQUE_MESSAGES", "topN": 10 }
  ]
}
```

### Advanced Configuration Features

#### Environment Variable Expansion

You can use environment variables in your configuration file using the syntax `${VAR}` or `$VAR`. They will be expanded when the file is loaded.

**Example:**
```json
{
  "exportSettings": {
    "outputFile": "${HOME}/analysis_results.json"
  }
}
```

#### Configuration Includes

You can split your configuration into multiple files using the `includes` key. This allows you to share common settings across different configurations. The paths can be relative to the main configuration file or absolute.

**Example `config.json`:**
```json
{
  "includes": [
    "common_filters.json",
    "output_settings.json"
  ],
  "lineParsePattern": "..."
}
```

**Note:** Properties in the main file override those in included files. Later includes override earlier ones if there are conflicts.

## Project Structure

```
logAnalyzer/
├── cmake/                   # CMake modules and scripts
├── docs/                    # Documentation files
├── include/                 # Public header files
│   ├── analyzer/
│   ├── config/
│   ├── core/
│   ├── export/
│   ├── filter/
│   ├── stats/
│   └── utils/
├── src/                     # Source code
│   ├── analyzer/
│   ├── config/
│   ├── core/
│   ├── export/
│   ├── filter/
│   ├── stats/
│   └── utils/
├── tests/                   # Unit and integration tests
├── tools/                   # Helper scripts for development
├── .gitignore
├── CMakeLists.txt           # Root CMake file
└── README.md                # This file
```

## Running Tests

To run the test suite, execute `ctest` from the `build` directory:

```bash
cd build
ctest --verbose
```

## Developer Tools

The following commands can be run from the `build` directory.

-   **Generate Documentation**:
    ```bash
    cmake --build . --target doc
    ```

-   **Format Code**:
    ```bash
    cmake --build . --target format
    ```

## Contributing

Please see our [Contributing Guidelines](CONTRIBUTING.md).

## Code of Conduct

Please read our [Code of Conduct](CODE_OF_CONDUCT.md).

## License

This project is licensed under the GNU GENERAL PUBLIC LICENSE Version 3.