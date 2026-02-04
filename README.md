# LogAnalyzer

A high-performance C++ command-line utility for advanced log analysis, filtering, and statistical insights.

[![Build Status](https://github.com/your-org-or-username/logAnalyzer/actions/workflows/ci.yml/badge.svg)](https://github.com/your-org-or-username/logAnalyzer/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Doxygen Documentation](https://img.shields.io/badge/docs-Doxygen-blue.svg)](./docs/html/index.html)

## Overview

`logAnalyzer` is a high-performance command-line utility built in C++ that enables detailed analysis, filtering, and extraction of insights from large log files. It is designed for efficiency, handling massive datasets with minimal memory footprint by processing logs as streams.

## Table of Contents

-   [Features](#features)
-   [Command-Line Interface (CLI)](#command-line-interface-cli)
-   [Configuration](#configuration)
-   [Project Structure](#project-structure)
-   [Prerequisites](#prerequisites)
-   [Getting Started](#getting-started)
-   [Build Options](#build-options)
-   [Installation](#installation)
-   [Usage](#usage)
-   [Running Tests](#running-tests)
-   [Usage Examples](#usage-examples)
-   [Developer Tools](#developer-tools)
-   [Contributing](#contributing)
-   [Code of Conduct](#code-of-conduct)
-   [License](#license)

## Features

-   **Memory-Efficient Processing**: Handles very large files with minimal memory usage using the `--stream` mode.
-   **Multi-File Support**: Parses and analyzes multiple log files at once.
-   **Structured Field Parsing**: Automatically parses log messages into key-value pairs using custom delimiters.
-   **Advanced Filtering**: Filter by keywords, regular expressions, log levels, and time ranges.
-   **Flexible Export**: Save results in Text, JSON, or CSV formats.
-   **Statistical Analysis**: Generate statistics on your log data, such as entry rates and top messages.

## Command-Line Interface (CLI)

`logAnalyzer` provides a rich command-line interface for ad-hoc analysis.

Run `./bin/logAnalyzer --help` for a full list of commands.

### General Options

| Option | Shorthand | Description | Default |
| --- | --- | --- | --- |
| `--help` | `-h` | Shows the help message. | |
| `--config FILE` | | Load configuration from a JSON file. | |
| `--output FILE` | | Redirect output to a file. | (stdout) |
| `--color OPT` | | Controls colorized output (`always`, `auto`, `never`). | `auto` |
| `--stream` | | Enable stream mode to process entries without loading the entire file into memory. | `false` |
| `--on-parse-error OPT`| | Action on parse errors (`ignore`, `warn`, `throw`). | `warn` |
| `--stdin` | | Read log entries from standard input if no file paths are provided. | `false` |

### Filtering and Sorting

| Option | Description | Default |
| --- | --- | --- |
| `--keyword TEXT` | Keyword/phrase to filter for. Multiple uses are combined with `--logic`. | |
| `--exclude-keyword TEXT` | Keyword/phrase to exclude. Can be used multiple times. | |
| `--regex PATTERN` | Regex pattern to filter for. Multiple uses are combined with `--logic`. | |
| `--exclude-regex PATTERN` | Regex pattern to exclude. Can be used multiple times. | |
| `--logic [AND\|OR]` | Logic for combining multiple `--keyword` or `--regex` rules. | `AND` |
| `--case-sensitive` | Makes keyword filtering case-sensitive. | `false` |
| `--level LEVEL` | Log level to include (e.g., `ERROR`). Can be used multiple times. | |
| `--min-level LEVEL` | Minimum log level to include (e.g., `WARNING`). | |
| `--map-level KEY=LEVEL` | Map custom log levels (e.g., `TRC=TRACE`). | |
| `--start TIME` | Filter logs after a given timestamp (e.g., "2023-10-27 10:00:00"). | |
| `--end TIME` | Filter logs before a given timestamp. | |
| `--duration DURATION` | Duration for time filtering (e.g., '30m', '1h'). | |
| `--sort-by [time\|level\|msg]` | Field to sort results by. | `time` |
| `--order [asc\|desc]` | Sort order. | `asc` |

### Output Formatting

| Option | Description | Default |
| --- | --- | --- |
| `--format [text\|json\|csv]` | Sets the output format. | `text` |
| `--text-format TEXT` | Custom format string for `text` output (e.g., `"{timestamp} [{level}] {message}"`). | |
| `--csv-sep CHAR` | Separator character for `csv` output. | `,` |
| `--csv-fields LIST` | Ordered list of fields for CSV output. | |
| `--json-fields LIST` | Ordered list of fields for JSON output. | |
| `--pretty` | Pretty-print `json` output. | `false` |
| `--include-summary` | Include a summary section in `json` output. | `false` |

### Statistics

| Option | Description | Default |
| --- | --- | --- |
| `--stats NAME` | Enable a statistic collector. Available: `unique_messages`, `top_messages[:N]`, `entry_rate`. | |
| `--top-n N` | Sets the 'N' for the `top_messages` collector if not specified in `--stats`. | 10 |

## Configuration

`logAnalyzer` can be configured using a JSON configuration file for persistent setups.

An example configuration file (`config.json`) might look like this:

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

## Project Structure

```
.
├── cmake/                   # CMake modules and scripts
├── docs/                    # Documentation resources
├── include/                 # Public header files
│   ├── analyzer/            # Analyzer core logic headers
│   ├── config/              # Configuration and CLI headers
│   ├── core/                # Core types and parser headers
│   ├── export/              # Exporting headers
│   ├── filter/              # Filter logic headers
│   ├── stats/               # Statistics headers
│   └── utils/               # Utility headers
├── src/                     # Source files
│   ├── analyzer/            # Analyzer implementation
│   ├── config/              # Configuration implementation
│   ├── core/                # Core implementation
│   ├── export/              # Export implementation
│   ├── filter/              # Filter implementation
│   ├── stats/               # Statistics implementation
│   └── utils/               # Utility implementation
├── tests/                   # Unit and integration tests
├── .gitignore               # Git ignore file
└── README.md                # This file
```

## Prerequisites

-   **Compiler**: C++23 compatible compiler (e.g., GCC 13+, Clang 16+)
-   **Build System**: CMake 3.20 or higher
-   **Dependencies**: (Automatically handled via CMake `FetchContent`)
    -   [CLI11](https://github.com/CLIUtils/CLI11)
    -   [nlohmann/json](https://github.com/nlohmann/json)
    -   [GoogleTest](https://github.com/google/googletest)

## Getting Started

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/your-org-or-username/logAnalyzer.git
    cd logAnalyzer
    ```

2.  **Build the project:**
    ```bash
    mkdir build
    cd build
    cmake ..
    cmake --build .
    ```

3.  **Run the executable:**
    ```bash
    ./bin/logAnalyzer --help
    ```

## Build Options

-   `-DBUILD_TESTING=ON/OFF`: Toggles unit tests (Default: `ON`).
-   `-DLOGANALYZER_BUILD_SHARED=ON/OFF`: Build as shared library (Default: `OFF`).
-   `-DLOGANALYZER_USE_SANITIZER=Address/Undefined/None`: Enables sanitizers (Default: `None`).

Example:
```bash
cmake -DBUILD_TESTING=OFF -DLOGANALYZER_USE_SANITIZER=Address ..
```

## Installation

```bash
cd build
cmake --install . --prefix /usr/local
```

## Usage

For detailed help, run:
```bash
./bin/logAnalyzer --help
```

## Running Tests

```bash
cd build
ctest --verbose
```

## Usage Examples

### Basic Filtering

```bash
# Find all errors containing "database"
./bin/logAnalyzer /var/log/app.log --level ERROR --keyword "database"

# Find all entries EXCEPT those containing "DEBUG"
./bin/logAnalyzer app.log kern.log --exclude-keyword "DEBUG"
```

### Advanced Filtering and Output

```bash
# Find entries that are either warnings or errors, and contain "timeout" OR "refused"
./bin/logAnalyzer access.log --level WARNING --level ERROR --keyword "timeout" --keyword "refused" --logic OR

# Export errors between two dates to a pretty-printed JSON file
./bin/logAnalyzer system.log --level ERROR --start "2023-11-01 00:00:00" --end "2023-11-02 00:00:00" --format json --pretty --output errors.json
```

### Stream Mode

```bash
# Process a large log file without loading it all into memory
./bin/logAnalyzer large_log.log --stream --level ERROR --output filtered_errors.txt
```

### Statistical Analysis

```bash
# Get the top 5 most common messages
./bin/logAnalyzer system.log --level ERROR --stats top_messages:5
```

## Developer Tools

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

This project is licensed under the MIT License.
