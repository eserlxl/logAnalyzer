# LogAnalyzer

A high-performance C++ command-line utility for advanced log analysis, filtering, and statistical insights.

[![Build Status](https://github.com/Eser KUBALI/logAnalyzer/actions/workflows/cmake.yml/badge.svg?branch=main)](https://github.com/Eser KUBALI/logAnalyzer/actions)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
![CMake](https://img.shields.io/badge/cmake-3.20%2B-blue.svg)
[![Code style: clang-format](https://img.shields.io/badge/code%20style-clang--format-blue.svg)](https://clang.llvm.org/docs/ClangFormat.html)
[![Doxygen Documentation](https://img.shields.io/badge/docs-Doxygen-blue.svg)](https://Eser KUBALI.github.io/logAnalyzer/)
[![codecov](https://codecov.io/gh/Eser KUBALI/logAnalyzer/branch/main/graph/badge.svg)](https://codecov.io/gh/Eser KUBALI/logAnalyzer)

## Project Status

`LogAnalyzer` is under active development. We are continuously adding new features, improving performance, and refining the user experience. While it is stable for general use, expect potential API changes in major releases as the project evolves. For production use, we recommend using a tagged release for stability.

## Overview

`LogAnalyzer` is a high-performance, command-line utility built in C++ for detailed analysis, filtering, and extraction of insights from large log files. It is designed for efficiency, handling massive datasets with a minimal memory footprint by processing logs as streams.

## Why LogAnalyzer?

In a world of ever-growing log files, traditional tools like `grep`, `awk`, and `sed` can become cumbersome and slow. `LogAnalyzer` addresses these challenges by providing:

-   **Performance**: A C++ core that processes large volumes of data quickly.
-   **Structured Filtering**: Go beyond simple text matching with filters for log levels, timestamps, and structured data.
-   **Ease of Use**: A single, powerful CLI that combines the functionality of multiple tools.
-   **Low Memory Usage**: Stream processing for analyzing files that are too large to fit in memory.

## Table of Contents

-   [Quick Start](#quick-start)
-   [Features](#features)
-   [Building from Source](#building-from-source)
-   [Installation](#installation)
-   [Usage](#usage)
-   [Time-based Filtering](#time-based-filtering)
-   [Command-Line Interface (CLI)](#command-line-interface-cli)
-   [Configuration](#configuration)
-   [Project Structure](#project-structure)
-   [Running Tests](#running-tests)
-   [Developer Tools](#developer-tools)
-   [Contributing](#contributing)
-   [Code of Conduct](#code-of-conduct)
-   [License](#license)

## Quick Start

Get `LogAnalyzer` up and running on your system with these simple steps.

1.  **Clone the Repository**:
    ```bash
    git clone https://github.com/Eser KUBALI/logAnalyzer.git
    cd logAnalyzer
    ```

2.  **Build from Source**:
    Ensure you have a C++23 compatible compiler (e.g., GCC 13+, Clang 16+) and CMake 3.20+ installed.
    ```bash
    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    # Use -j to specify the number of parallel jobs, e.g., -j4
    cmake --build . -- -j$(nproc) 
    ```

3.  **Run a Basic Analysis**:
    After building, the executable will be in the `build/bin` directory.
    ```bash
    ./bin/LogAnalyzer /var/log/syslog --level ERROR
    ```
    *(Replace `/var/log/syslog` with a path to one of your log files.)*

For detailed build instructions, installation options, and more usage examples, please refer to the respective sections below.

## Features

| Feature                      | Description                                                                                             |
| ---------------------------- | ------------------------------------------------------------------------------------------------------- |
| **Memory-Efficient Processing** | Handles massive files with minimal memory usage using the `--stream` mode.                              |
| **Multi-File Support**       | Parses and analyzes multiple log files in a single run.                                                 |
| **Sorting**                  | Sort results by timestamp, log level, message, or other fields in ascending or descending order.        |
| **Structured Field Parsing** | Automatically parses log messages into fields, including structured data, using custom patterns and intelligent detection.                         |
| **Keyword & Regex Filtering**| Filter by log level, keywords, glob patterns (anchored), and case-sensitive/insensitive regular expressions. |
| **Field-Value Matching**     | Match field values with case-sensitive/insensitive text, regex, and glob patterns.                      |
| **Nested Field Filtering**   | Target nested fields within structured data (e.g., `user.id` in a JSON log).                            |
| **Advanced Data Types**      | Compare fields as `version` numbers (semantic versioning) or `IP addresses`.                            |
| **Numeric & Bool Filtering** | Perform numeric (`>`, `<`, `==`) or boolean (`true`, `false`) comparisons on flat and nested fields.     |
| **Set-Based Filtering**      | Check if a field's value is `in` or `not in` a specific set of values.                                   |
| **Time-based Filtering**     | Filter by absolute time range, relative time (`5m ago`), or for a specific day (`yesterday`, `2023-10-20`). |
| **Field Presence Checks**    | Filter for logs where a specific field `is present` or `is absent`.                                       |
| **Complex Filter Expressions** | Build sophisticated filter logic using parenthesized, nested `AND`/`OR`/`NOT` conditions.                     |
| **Live Tailing**             | Monitor log files for new entries in real-time (`tail -f` like behavior).                               |
| **Flexible Export**          | Save results in Text, JSON, or CSV formats with customizable and aliasable output fields.               |
| **Statistical Analysis**     | Generate statistics on log data, such as entry rates, top messages, log level counts, and unique value counts for any field. |

## Building from Source

This section guides you through setting up `LogAnalyzer` from its source code.

### Prerequisites

-   **C++ Compiler**: A compiler with C++23 support (e.g., GCC 13+, Clang 16+).
-   **Build System**: CMake (version 3.20 or higher).
-   **Version Control**: Git for cloning the repository.

#### Installing Dependencies

**On Debian/Ubuntu:**

```bash
sudo apt-get update
sudo apt-get install -y g++-13 cmake git
```

**On macOS (using Homebrew):**

```bash
brew install gcc cmake git
```

**On Arch Linux:**

```bash
sudo pacman -Syu gcc cmake git
```

**On Windows:**

Ensure you have [Visual Studio 2022](https://visualstudio.microsoft.com/) with the "Desktop development with C++" workload installed, along with [CMake](https://cmake.org/download/) and [Git](https://git-scm.com/download/win). You can use the Developer PowerShell for VS to run the build commands.

### Dependencies

`LogAnalyzer` leverages several excellent open-source libraries, which CMake will automatically fetch during the build process:

-   [**CLI11**](https://github.com/CLIUtils/CLI11): A header-only library for robust command-line argument parsing.
-   [**nlohmann/json**](https://github.com/nlohmann/json): A header-only JSON library for C++.
-   [**GoogleTest**](https://github.com/google/googletest): A Google testing and mocking framework for C++ (used for tests).

### Clone the Repository

First, clone the repository and navigate into the project directory:

```bash
git clone https://github.com/Eser KUBALI/logAnalyzer.git
cd logAnalyzer
```

### Build

Next, use CMake to configure and build the project. We recommend an out-of-source build.

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -- -j$(nproc)
```

The compiled `LogAnalyzer` executable will be available in the `build/bin` directory.

#### Build Options

You can customize the build with the following CMake options:

| Option                       | Description                                                     | Default    |
| :--------------------------- | :-------------------------------------------------------------- | :--------- |
| `-DBUILD_TESTING=ON/OFF`     | Enable or disable the compilation of tests.                     | `ON`       |
| `-DLOGANALYZER_BUILD_SHARED=ON/OFF` | Build `LogAnalyzer` as a shared library.                        | `OFF`      |
| `-DLOGANALYZER_USE_SANITIZER=...` | Enable sanitizers for debugging (`Address`, `Undefined`).       | `None`     |
| `-DENABLE_COVERAGE=ON/OFF`   | Enable code coverage instrumentation for tests.                 | `OFF`      |
| `-DENABLE_ASAN=ON/OFF`       | Enable AddressSanitizer for tests.                              | `OFF`      |
| `-DENABLE_UBSAN=ON/OFF`      | Enable UndefinedBehaviorSanitizer for tests.                    | `OFF`      |
| `-DENABLE_GMOCK=ON/OFF`      | Enable Google Mock for tests.                                   | `OFF`      |

To use an option, add it to the `cmake` command:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
```

**Note:** To build the documentation, use `cmake --build . --target doc` from the `build` directory.


## Installation

To install the `LogAnalyzer` executable to a system-wide location (e.g., `/usr/local/bin`), run the following command from your `build` directory. This allows you to run `LogAnalyzer` from any terminal.

```bash
# Use sudo for system-wide installation
sudo cmake --install . --prefix /usr/local
```

For a local installation (if you don't have admin privileges), you can specify a different prefix:

```bash
# Install to a 'dist' directory inside the project folder
cmake --install . --prefix ../dist
```

Alternatively, you can add the `build/bin` directory to your system's `PATH` or copy the `LogAnalyzer` executable to a directory already in your `PATH`.

```bash
# Add to PATH for the current session (from within the build directory)
export PATH=$(pwd)/bin:$PATH

# Or copy the executable to a user-local bin directory (ensure ~/.local/bin is in your PATH)
cp ./bin/LogAnalyzer ~/.local/bin/
```

## Usage

After building, you can run `LogAnalyzer` in two ways:

1.  **From the build directory**:
    ```bash
    ./build/bin/LogAnalyzer [options] <log_file(s)>
    ```
2.  **As an installed command**:
    ```bash
    LogAnalyzer [options] <log_file(s)>
    ```

**Note**: The following examples assume `LogAnalyzer` is in your `PATH`.

### Example 1: Basic Filtering

```bash
# Find all errors containing the word "database" in a specific log file
LogAnalyzer /var/log/app.log --level ERROR --keyword "database"

# Find all entries in two different log files, excluding those containing "DEBUG"
LogAnalyzer app.log kern.log --exclude-keyword "DEBUG"
```

### Example 2: Advanced Filtering and Output

```bash
# Find entries that are either warnings or errors, and contain "timeout" OR "refused"
LogAnalyzer access.log --level WARNING --level ERROR --keyword "timeout" --keyword "refused" --logic OR

# Export errors between two dates to a pretty-printed JSON file
LogAnalyzer system.log --level ERROR --start "2023-11-01 00:00:00" --end "2023-11-02 00:00:00" --format json --pretty --output errors.json
```

### Example 3: Complex Expression

```bash
# Use a complex expression to find database errors or any message containing "timeout"
LogAnalyzer app.log --expression '(level=ERROR and msg contains "database") or msg contains "timeout"'
```

### Example 4: Stream a Large File

```bash
# Process a large log file without loading it all into memory, saving errors to a file
LogAnalyzer large_log.log --stream --level ERROR --output filtered_errors.txt
```

### Example 5: Process Logs from Standard Input

```bash
# Pipe logs from another command and filter for errors
cat /var/log/syslog | LogAnalyzer - --level ERROR

# Tail a file and filter for a keyword
tail -f /var/log/app.log | LogAnalyzer --stdin --keyword "error"
```

### Example 6: Statistical Analysis

```bash
# Get the top 5 most common error messages from a log file (using legacy syntax)
LogAnalyzer system.log --level ERROR --stats top_messages:5

# Get the top 10 messages using the new, more flexible syntax
LogAnalyzer system.log --stats "type=TOP_MESSAGES,top_n=10"
```

### Example 7: Tailing a File

```bash
# Monitor a log file in real-time for new entries containing "critical"
LogAnalyzer /var/log/app.log --tail --keyword "critical"
```

### Example 8: Custom CSV Export

```bash
# Export specific fields to a CSV, with a custom header for the timestamp field
LogAnalyzer application.log --format csv --csv-fields "timestamp as Time, level, message" --output report.csv
```


### Time-based Filtering

`LogAnalyzer` offers flexible options for filtering log entries based on their timestamps using the `--start` and `--end` flags.

You can specify timestamps in several formats:
*   **Absolute Time**: `"YYYY-MM-DD HH:MM:SS"` (e.g., `"2023-11-20 14:30:00"`)
*   **Relative Time**: Keywords like `yesterday`, `today`, or offsets like `"1h ago"`, `"30m ago"`, `"2d ago"`.
*   **ISO 8601 Format**: `YYYY-MM-DDTHH:MM:SSZ` or `YYYY-MM-DDTHH:MM:SS+HH:MM`.
*   **Unix Timestamp**: Seconds since the Unix epoch.

#### Using `--duration`

The `--duration` flag can be combined with `--start` or `--end` to specify a time window. It accepts durations like `10s` (seconds), `5m` (minutes), `2h` (hours), or `3d` (days).

#### Examples

```bash
# Get logs from the last 2 hours
LogAnalyzer app.log --start "2h ago"

# Get logs from yesterday
LogAnalyzer app.log --start "yesterday" --end "today"

# Get logs for a 30-minute window starting at a specific time
LogAnalyzer app.log --start "2023-11-20 10:00:00" --duration "30m"

# Get logs from a specific day (using ISO 8601 date)
LogAnalyzer app.log --start "2023-11-20T00:00:00Z" --end "2023-11-21T00:00:00Z"
```

## Command-Line Interface (CLI)

`LogAnalyzer` provides a rich command-line interface for ad-hoc analysis.

Run `LogAnalyzer --help` for a full list of commands.

### General Options

| Option                 | Shorthand | Description                                                                                                                                                                             | Default    |
| :--------------------- | :-------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :--------- |
| `--help`               | `-h`      | Displays the help message and exits.                                                                                                                                                    |            |
| `--config FILE`        |           | Specifies a JSON configuration file to load. Command-line arguments will override settings defined in the file.                                                                           |            |
| `--output FILE`        |           | Redirects all output (filtered logs, statistics) to the specified file instead of standard output.                                                                                      | `(stdout)` |
| `--color OPT`          |           | Controls colorized output. Options are `always`, `auto` (default, colors if stdout is a TTY and not redirected), or `never`.                                                              | `auto`     |
| `--stream`             |           | Enables memory-efficient stream processing mode for very large files. Not all features are available in stream mode (e.g., sorting).                                                    | `false`    |
| `--stdin` |           | Reads log entries from standard input (e.g., from a pipe). This mode is automatically enabled if `-` is used as a log file path. See Example 5 for details.                                           | `false`    |


### Parsing

| Option                          | Description                                                                                                          | Default   |
| :------------------------------ | :------------------------------------------------------------------------------------------------------------------- | :-------- |
| `--pattern REGEX`               | Overrides the log line parsing regular expression defined in the configuration.                                      | (builtin) |
| `--multiline-start-pattern REGEX` | Regex to identify the start of a multi-line log entry.                                                               |           |
| `--max-multiline-buffer SIZE`   | Max buffer size for multi-line entries (e.g. 10MB, 50KB, 1048576).                                                    | `10MB`    |
| `--field-map MAPPING`           | Map regex capture group to a field (e.g., '1=timestamp:%Y-%m-%d %H:%M:%S'). Can be used multiple times.               |           |
| `--on-parse-error OPT`          | Action on parse error. Options are `skip` (ignore the line), `log` (print a warning to stderr), or `fail` (exit).     | `log`     |


### Filtering


| Option                   | Shorthand | Description                                                                                                                                                                                                                                                                                                                                                         | Default   |

| :----------------------- | :-------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | :-------- |

| `--keyword TEXT`         | `-k`      | Filters log messages containing this keyword or phrase. Can be used multiple times, combined by `--logic`.                                                                                                                                                                                                                                                          |           |

| `--exclude-keyword TEXT` |           | Excludes log messages containing this keyword or phrase. Can be used multiple times.                                                                                                                                                                                                                                                                                |           |

| `--regex PATTERN`        | `-r`      | Filters log messages matching this regular expression. Can be used multiple times, combined by `--logic`.                                                                                                                                                                                                                                                           |           |

| `--exclude-regex PATTERN`|           | Excludes log messages matching this regular expression. Can be used multiple times.                                                                                                                                                                                                                                                                                 |           |

| `--logic [AND|OR]`       |           | Specifies the logical operator for combining multiple `--keyword` or `--regex` filters.                                                                                                                                                                                                                                                                             | `AND`     |

| `--case-sensitive`       |           | Makes keyword and regex filtering case-sensitive.                                                                                                                                                                                                                                                                                                                   | `false`   |

| `--level LEVEL`          | `-l`      | Includes log entries of a specific level (e.g., `ERROR`, `INFO`). Can be used multiple times to include multiple levels.                                                                                                                                                                                                                                            |           |

| `--min-level LEVEL`      | `-m`      | Includes log entries with a level equal to or more severe than the specified level (e.g., `WARNING` will include `WARNING`, `ERROR`, `CRITICAL`).                                                                                                                                                                                                                  |           |

| `--map-level KEY=LEVEL`  |           | Maps a custom log level string found in logs (KEY) to a recognized internal level (LEVEL, e.g., `TRC=TRACE`, `WRN=WARNING`). Can be used multiple times.                                                                                                                                                                                                               |           |

| `--start TIME`           |           | Filters logs appearing after the specified timestamp. Supports absolute, relative, ISO 8601, and Unix timestamp formats.                                                                                                                                                                                                                                            |           |

| `--end TIME`             |           | Filters logs appearing before the specified timestamp. Supports the same formats as `--start`.                                                                                                                                                                                                                                                                      |           |

| `--duration DURATION`    |           | Specifies a time window when used with `--start` or `--end`. Accepts units like `s` (seconds), `m` (minutes), `h` (hours), or `d` (days).                                                                                                                                                                                                                           |           |

| `--expression "EXPR"`    | `-e`      | A powerful filter using a logical expression language. Supports fields, nested `and`/`or`/`not` logic, and rich operators like `contains_i` (case-insensitive), `in` (set), `>` (numeric), `startswith`, `is present`, and type casting (e.g., `ip(client_ip)`) for advanced filtering. Example: `(level=ERROR and msg contains_i "database") or not status_code in [200, 304]` |           |


### Sorting


| Option                 | Shorthand | Description                                                                                         | Default     |

| :--------------------- | :-------- | :-------------------------------------------------------------------------------------------------- | :---------- |

| `--sort-by FIELD`      |           | Sorts the output by a specific field. Available fields: `timestamp`, `level`, `message`, `source`, `thread_id`. | `timestamp` |

| `--sort-order ORDER`   |           | Sets the sorting order. Available orders: `ascending`, `descending`.                                | `ascending` |


### Output & Export


| Option                  | Shorthand | Description                                                                                                                             | Default                         |

| :---------------------- | :-------- | :-------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------ |

| `--format [text|json|csv]` | `-f`      | Sets the output format for filtered log entries.                                                                                        | `text`                          |

| `--text-format FORMAT_STRING` |           | Custom format string for `text` output. Placeholders: `{timestamp}`, `{level}`, `{message}`, `{lineNumber}`, `{fileName}`, `{elapsedTime}`. | `{timestamp} {level}: {message}`|

| `--csv-sep CHAR`        |           | Specifies the separator character for `csv` output.                                                                                     | `,`                             |

| `--csv-fields "FIELDS"` |           | Comma-separated list of fields to include in `csv` output (e.g., `timestamp,level,message,file`).                                       | `timestamp,level,message,file`  |

| `--json-fields "FIELDS"`|           | Comma-separated list of fields to include in `json` output. If omitted, all standard fields are included.                               | `(all)`                         |

| `--pretty`              | `-p`      | Pretty-prints `json` output with indentation for readability.                                                                           | `false`                         |

| `--include-summary`     |           | Includes a summary section (e.g., total entries) in `json` output.                                                                      | `false`                         |


### Statistics


| Option             | Description                                                                                                       | Default |

| :----------------- | :---------------------------------------------------------------------------------------------------------------- | :------ |

| `--stats NAME`     | Enables a statistic collector. Available: `unique_messages`, `top_messages[:N]`, `entry_rate`. Can be used multiple times. |         |

| `--top-n N`        | Sets the number of top items to display for statistics like `top_messages` if not specified directly (e.g., `top_messages:10`). | `10`    |

| `--stats-window SEC` | Shows log frequency distribution over a time window in seconds.                                                   |         |

| `--find-gaps MS`   | Detects and reports time gaps in logs longer than the specified milliseconds.                                     |         |


### Tailing (Live Mode)

Monitor files for new lines, similar to `tail -f`. Not compatible with `--stdin` or `--stream`.


| Option             | Description                                          | Default |

| :----------------- | :--------------------------------------------------- | :------ |

| `--tail`           | Enables tail mode to watch files for new entries in real-time. | `false` |

| `--tail-interval MS` | Polling interval in milliseconds for tail mode.      | `1000`  |

## Configuration

`LogAnalyzer` can be configured using a JSON file for complex or persistent setups. Use the `--config` option to specify a configuration file. Settings provided via command-line arguments will override the corresponding settings in the file.

The application performs robust validation on startup. If it finds any errors in the configuration file (e.g., a missing required parameter, an invalid value, or a malformed regex), it will print a descriptive error message and exit.

### Example `config.json`

Here is an example demonstrating a more advanced configuration:

```json
{
  "lineParsePattern": "^(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}.\d{3}Z) \[(\w+)\] \(tid:(\d+)\) (.*) \{ \"session\": \"([a-f0-9-]+)\" \}$",
  "fieldMappings": [
    { "field": "timestamp", "groupIndex": 1 },
    { "field": "level", "groupIndex": 2 },
    { "field": "threadId", "groupIndex": 3 },
    { "field": "message", "groupIndex": 4 },
    { "field": "customFields", "groupIndex": 5, "customFieldKey": "session" }
  ],
  "customLogLevelMappings": {
    "db_trace": "TRACE",
    "warn": "WARNING"
  },
  "filterRules": [
    { "field": "level", "operator": "EQUALS", "value": "ERROR" },
    { "field": "message", "operator": "REGEX_MATCH", "value": "database connection failed" }
  ],
  "exportSettings": {
    "format": "json",
    "outputFile": "error_report.json",
    "fieldsToExport": [
      { "field": "timestamp" },
      { "field": "level" },
      { "field": "message" },
      { "field": "customFields", "customFieldKey": "session" }
    ]
  },
  "statisticConfigs": [
    {
      "type": "TOP_MESSAGES",
      "params": { "top_n": "5" }
    },
    {
      "type": "TOP_N_FIELD_VALUES",
      "params": {
        "target_field": "customFields",
        "custom_field_key": "session",
        "top_n": "10"
      }
    },
    {
      "type": "FIELD_VALUE_COUNT",
      "params": { "target_field": "level" }
    }
  ]
}
```

### Key Configuration Sections

#### `filterRules`
An array of rule objects that define how to filter log entries. Each rule is an object with:
-   `field`: The log entry field to check (e.g., `level`, `message`, `customFields`).
-   `operator`: The comparison operator (e.g., `EQUALS`, `CONTAINS`, `REGEX_MATCH`, `GREATER_THAN`).
-   `value`: The value to compare against.
-   `customFieldKey` (optional): Required if `field` is `customFields`.

#### `statisticConfigs`
An array of objects to configure which statistics to generate. Each object has a `type` and an optional `params` object.

| Type                 | Description                                       | Required `params`                                                                                                 |

| :-------------------- | :------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------- |

| `LOG_LEVEL_COUNT`    | Counts entries for each log level.                | None                                                                                                              |

| `TOP_MESSAGES`       | Finds the most frequently occurring messages.     | `top_n`: A positive integer (e.g., `"5"`).                                                                        |

| `FIELD_VALUE_COUNT`  | Counts unique values for a given field.           | `target_field`: The field to analyze (e.g., `level`). If `customFields`, `custom_field_key` is also required.      |

| `TOP_N_FIELD_VALUES` | Finds the most frequent values for a given field. | `top_n`: A positive integer.<br>`target_field`: The field to analyze. If `customFields`, `custom_field_key` is also required. |

| `ENTRY_RATE`         | Calculates the rate of log entries per second.    | None                                                                                                              |

| `UNIQUE_MESSAGES`    | Counts the number of unique log messages.         | None                                                                                                              |


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

Understanding the project's layout can help you navigate the codebase, contribute, or find specific functionalities.

```
logAnalyzer/
├── _deps/                   # External dependencies (CLI11, nlohmann/json, GoogleTest) managed by CMake.
├── bin/                     # Compiled `LogAnalyzer` executable and other binaries.
├── build/                   # CMake build artifacts and temporary files.
├── cmake/                   # Custom CMake modules and scripts.
├── docs/                    # Doxygen configuration and generated documentation.
├── examples/                # Example log files and configuration examples.
├── include/                 # Public header files for core logic, configuration, filters, and utilities.
├── lib/                     # Compiled libraries (static/shared).
├── src/                     # Source code (.cpp files) implementing header functionalities.
├── tests/                   # Unit and integration tests.
├── tools/                   # Development scripts and utilities.
├── .gitignore               # Files/directories ignored by Git.
├── CMakeLists.txt           # Primary CMake build script.
├── CODE_OF_CONDUCT.md       # Guidelines for community behavior.
├── CONTRIBUTING.md          # Contribution guidelines.
├── Doxyfile                 # Doxygen main configuration.
├── LICENSE                  # Project license information.
└── README.md                # Project overview and documentation.
```

## Running Tests

To run the test suite, navigate to the `build` directory and execute `ctest`:

```bash
cd build
ctest --verbose
```

## Developer Tools

The following commands can be run from the `build` directory to assist with development and maintenance:

-   **Generate Documentation**:
    Generates HTML documentation using Doxygen. The output will be in `build/docs/html/`.
    ```bash
    cmake --build . --target doc
    ```

-   **Format Code**:
    Automatically formats the C++ source code using `clang-format` according to the project's style guidelines.
    ```bash
    cmake --build . --target format
    ```

## Contributing

Please see our [Contributing Guidelines](CONTRIBUTING.md).

## Code of Conduct

Please read our [Code of Conduct](CODE_OF_CONDUCT.md).

## License

This project is licensed under the terms of the [GPL-3.0 license](LICENSE).

