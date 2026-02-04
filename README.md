# LogAnalyzer

[![Build Status](https://github.com/YOUR_ORGANIZATION/logAnalyzer/actions/workflows/ci.yml/badge.svg)](https://github.com/YOUR_ORGANIZATION/logAnalyzer/actions/workflows/ci.yml) <!-- IMPORTANT: Update 'YOUR_ORGANIZATION' and the workflow path to your actual GitHub Actions CI/CD build status link. -->
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)

## Overview

`logAnalyzer` is a high-performance command-line utility built in C++ that enables detailed analysis, filtering, and extraction of insights from large log files. It's designed for efficiency, handling massive datasets with minimal memory footprint by processing logs as streams. Whether you need real-time monitoring, complex filtering, or statistical summaries, `logAnalyzer` provides the tools to manage your log data effectively.

## Table of Contents

-   [Overview](#overview)
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
-   [Versioning](#versioning)
-   [Code of Conduct](#code-of-conduct)
-   [License](#license)

## Features

-   **Memory-Efficient Processing**: Handles very large files with minimal memory usage by processing them as streams.
-   **Multi-File Support**: Parses and analyzes multiple log files.
-   **Powerful Command-Line Interface**: A rich set of command-line options to control filtering, formatting, and analysis without needing a configuration file.
-   **Advanced Filtering**: Build complex filter expressions with `AND`/`OR`/`NOT` logic, time ranges, log levels, keywords, and regular expressions.
-   **Live Stream Mode**: Monitor new log entries from files in real-time, similar to `tail -f`.
-   **Flexible Export**: Save results in Text, JSON, or CSV formats.
-   **Statistical Analysis**: Generate statistics on your log data, such as entry rates and most frequent messages.

## Command-Line Interface (CLI)

`logAnalyzer` provides a rich command-line interface for ad-hoc analysis. While a JSON file is ideal for complex, persistent configurations, the CLI is perfect for quick filtering and exploration.

Run `./bin/logAnalyzer --help` for a full list of commands.

### General Options

| Option | Shorthand | Description | Default |
| --- | --- | --- | --- |
| `--help` | `-h` | Shows the help message. | |
| `--config FILE` | `-c` | Load configuration from a JSON file. | |
| `--output FILE` | `-o` | Redirect output to a file. | (stdout) |
| `--color OPT` | | Controls colorized output (`always`, `auto`, `never`). | `auto` |
| `--stream` | | Enable stream mode to process entries as they arrive, like `tail -f`. | `false` |
| `--parser-errors OPT`| | Action on parse errors (`skip`, `warn`, `fail`). | `warn` |

### Filtering and Sorting

| Option | Shorthand | Description | Default |
| --- | --- | --- | --- |
| `--filter TEXT` | `-f` | Keyword/phrase to filter for. Multiple uses are combined with `--logic`. | |
| `--exclude TEXT` | `-e` | Keyword/phrase to exclude. Can be used multiple times. | |
| `--filter-regex TEXT`| | Regex pattern to filter for. Multiple uses are combined with `--logic`. | |
| `--exclude-regex TEXT`| | Regex pattern to exclude. Can be used multiple times. | |
| `--logic [AND\|OR]`| | Logic for combining `--filter` and `--filter-regex` rules. | `AND` |
| `--case-sensitive` | `-s` | Makes keyword filtering case-sensitive. | `false` |
| `--level LEVEL` | `-l` | Log level to include (e.g., `ERROR`). Can be used multiple times. | |
| `--min-level LEVEL` | | Minimum log level to include (e.g., `WARNING`). | |
| `--start-time TIME` | | Filter logs after a given timestamp (e.g., "2023-10-27 10:00:00"). | |
| `--end-time TIME` | | Filter logs before a given timestamp. | |
| `--sort-by [timestamp\|level\|message]` | | Field to sort results by. | `timestamp` |
| `--sort-order [asc\|desc]` | | Sort order. | `asc` |

### Output Formatting

| Option | Description | Default |
| --- | --- | --- |
| `--format [text\|json\|csv]` | Sets the output format. | `text` |
| `--output-format TEXT` | Custom format string for `text` output (e.g., `"{timestamp} [{level}] {message}"`). | `"{timestamp} [{level}] {message}"` |
| `--csv-separator CHAR` | Separator character for `csv` output. | `,` |
| `--pretty` | Pretty-print `json` output. | `false` |
| `--summary` | Include a summary section in `json` output. | `false` |

### Statistics

| Option | Description | Default |
| --- | --- | --- |
| `--stats NAME` | Enable a statistic collector. Can be used multiple times. Available collectors: `unique_messages`, `top_messages:N`, `entry_rate`, `level_distribution`. | |
| `--top-messages-count N`| Sets the 'N' for the `top_messages` collector if not specified in `--stats`. | 10 |

## Configuration

`LogAnalyzer` can be extensively configured using a JSON configuration file. This allows for persistent and complex setups for parsing, filtering, and exporting log data, and is ideal for settings that are used repeatedly. For ad-hoc analysis, the [Command-Line Interface (CLI)](#command-line-interface-cli) is often more convenient.

Key configurable aspects include:

-   **Parsing Settings**:
    -   **Log Line Pattern**: Define the regular expression to parse individual log lines.
    -   **Field Mappings**: Map regex capture groups to meaningful log entry fields (e.g., `timestamp`, `level`, `message`).
    -   **Custom Log Level Mappings**: Define custom string-to-level mappings (e.g., `"DEBUG": "Debug"`).
    -   **Multi-line Log Entry Pattern**: Specify a regex to identify the start of new log entries, enabling the grouping of multi-line logs.
    -   **Case-Sensitive Parsing**: Control case sensitivity for parsing operations.
-   **Filtering Settings**:
    -   **Filter Rules**: Define basic filtering criteria based on fields, operators, and values.
    -   **Advanced Filter Expressions**: Construct complex nested `AND`/`OR`/`NOT` logic for precise log filtering.
-   **Export Settings**:
    -   **Export Format**: Choose the output format (e.g., JSON, CSV, Text).
    -   **Fields to Export**: Specify which log entry fields should be included in the output.
    -   **Output Destination**: Define the output file or stream.
-   **Statistical Analysis Settings**:
    -   Configure various statistics to be gathered, such as unique message counts, frequency distributions, etc.

An example configuration file (`config.json`) might look like this:

```json
{
  "lineParsePattern": "^(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}) ([A-Z]+): (.*)$",
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
  "logEntryStartPattern": "^\\d{4}-\d{2}-\d{2}",
  "caseSensitiveParsing": false,
  "filterRules": [
    { "field": "level", "operator": "EQ", "value": "ERROR" }
  ],
  "rootFilterExpression": {
    "operator": "AND",
    "operands": [
      {
        "type": "RULE",
        "rule": { "field": "level", "operator": "EQ", "value": "ERROR" }
      },
      {
        "type": "EXPRESSION",
        "expression": {
          "operator": "OR",
          "operands": [
            { "type": "RULE", "rule": { "field": "message", "operator": "CONTAINS", "value": "database" } },
            { "type": "RULE", "rule": { "field": "message", "operator": "CONTAINS", "value": "network" } }
          ]
        }
      }
    ]
  },
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

A high-level overview of the project's directory structure:

```
.
├── CMake/                   # CMake modules and scripts
├── docs/                    # Doxygen configuration (e.g., Doxyfile.in) and documentation resources
├── include/                 # Public header files defining the core interfaces: CLI configuration, error handling, data structures (LogTypes), filter logic, log parsing, statistics, and export mechanisms. Also contains CMake-generated configurations.
├── src/                     # Source files implementing the core logic for the LogAnalyzer: CLI configuration parsing, exporter, filter, main application logic, log parsing, and statistics calculation. Includes the `main.cpp` entry point.
├── tests/                   # Unit and integration tests for various components like Filter, LogAnalyzer, LogAnalyzerConfig, LogParser, and Statistics. Contains `CMakeLists.txt` for test setup and sample log files for testing.
├── .gitignore               # Files ignored by Git
├── CODE_OF_CONDUCT.MD       # Project's Code of Conduct
├── CONTRIBUTING.md          # Guidelines for contributing to the project
└── README.md                # This README file
```

## Prerequisites

-   **Compiler**: C++23 compatible compiler (e.g., GCC 13+, Clang 16+)
-   **Build System**: CMake 3.20 or higher
-   **Dependencies**: (Automatically handled via FetchContent)
    -   [CLI11](https://github.com/CLIUtils/CLI11) for command-line argument parsing.
    -   [nlohmann/json](https://github.com/nlohmann/json) for JSON handling.
    -   [GoogleTest](https://github.com/google/googletest) for unit testing.

## Getting Started

To get a local copy up and running, follow these simple steps.

1.  **Check Prerequisites**: Ensure you have a C++23 compatible compiler (e.g., GCC 13+, Clang 16+) and CMake 3.20+ installed on your system.

2.  **Clone the repository:**
    ```bash
    git clone https://github.com/YOUR_ORGANIZATION/logAnalyzer.git
    cd logAnalyzer
    ```

3.  **Build the project:**
    ```bash
    mkdir build
    cd build
    cmake ..
    cmake --build .
    ```

4.  **Run the executable (Optional)**: After a successful build, the `logAnalyzer` executable will be located in the `build/bin/` directory.
    ```bash
    ./bin/logAnalyzer /path/to/your/log/file.log
    # Example for basic analysis (assuming 'sample.log' exists in the parent directory):
    ./bin/logAnalyzer ../tests/test_append_1.log
    ```

## Build Options

The following CMake options can be used to customize the build process:

-   `-DBUILD_TESTING=ON/OFF`: Toggles the compilation of unit tests (Default: `ON`).
-   `-DLOGANALYZER_BUILD_SHARED=ON/OFF`: Build `logAnalyzer` as a shared library (Default: `OFF`).
-   `-DLOGANALYZER_USE_SANITIZER=Address/Undefined/None`: Enables various sanitizers (e.g., AddressSanitizer, UndefinedBehaviorSanitizer) to detect runtime errors (Default: `None`).

To use these options, pass them during the CMake configuration step:
```bash
cmake -DBUILD_TESTING=OFF -DLOGANALYZER_USE_SANITIZER=Address ..
cmake --build .
```

## Installation

After building, you can optionally install `logAnalyzer` to your system.

```bash
cd build
cmake --install . --prefix /usr/local
```
By default, this will install the `logAnalyzer` executable to `/usr/local/bin`, and any associated libraries and header files to ` /usr/local/lib` and `/usr/local/include` respectively. You can change the installation root by specifying a different path for `--prefix`. For example, `--prefix ~/.local` would install it into your home directory's local binaries.

## Usage

`logAnalyzer` is a command-line tool that processes log files based on specified filters, output formats, and statistical requirements. You can define your analysis criteria directly via command-line arguments or by providing a comprehensive JSON configuration file.

For detailed examples of common tasks, refer to the [Usage Examples](#usage-examples) section. For a complete list of all available command-line options, execute:

```bash
./bin/logAnalyzer --help
```

## Running Tests

To execute the unit and integration tests:

```bash
cd build
ctest --verbose
```

## Usage Examples

This section shows a few common use cases. For a full list of flags, see the [Command-Line Interface (CLI)](#command-line-interface-cli) section above or run `--help`.

### Basic Filtering

```bash
# Find all errors containing "database" in a specific log file
./bin/logAnalyzer /var/log/app.log --level ERROR --filter "database"

# Find all entries EXCEPT those containing "DEBUG" from multiple files
./bin/logAnalyzer app.log kern.log --exclude "DEBUG"
```

### Advanced Filtering and Output

```bash
# Find entries that are either warnings or errors, and contain "timeout" OR "refused"
./bin/logAnalyzer access.log --level WARNING --level ERROR --filter "timeout" --filter "refused" --logic OR

# Export errors between two dates to a pretty-printed JSON file
./bin/logAnalyzer system.log --level ERROR --start-time "2023-11-01" --end-time "2023-11-02" --format json --pretty -o errors.json
```

### Real-time Monitoring

```bash
# Tail a log file in real-time for critical errors
./bin/logAnalyzer /var/log/live.log --stream --min-level CRITICAL
```

### Statistical Analysis

```bash
# Get the top 5 most common error messages from a log
./bin/logAnalyzer system.log --level ERROR --stats top_messages:5
```

```bash
# Get the distribution of log levels
./bin/logAnalyzer app.log --stats level_distribution
```


### Using a Configuration File

For complex or repeated tasks, you can use a JSON configuration file.

```bash
# Analyze log files using settings from a JSON configuration file
./bin/logAnalyzer sample.log --config config.json
```

For a complete list of available command-line options and their descriptions, run:
```bash
./bin/logAnalyzer --help
```

## Developer Tools

-   **Generate Documentation**:
    ```bash
    cmake --build . --target doc
    ```
    API documentation will be generated by Doxygen in the `build/docs/html` directory. Open `build/docs/html/index.html` in your web browser to view it.

-   **Format Code**:
    ```bash
    cmake --build . --target format
    ```
    This command applies consistent code formatting using `clang-format` (if available) across the codebase.

## Contributing

We welcome contributions! Please see our [Contributing Guidelines](CONTRIBUTING.md) for more details on how to get involved, report issues, and propose changes.

## Versioning

We use [SemVer](http://semver.org/) for versioning. For the versions available, see the [tags on this repository](https://github.com/YOUR_ORGANIZATION/logAnalyzer/tags).

## Code of Conduct

Please read our [Code of Conduct](CODE_OF_CONDUCT.md) to understand the standards of behavior we expect from our community members and contributors.

## License

This project is licensed under the MIT License.
