# LogAnalyzer

[![Build Status](https://github.com/your-organization/logAnalyzer/actions/workflows/ci.yml/badge.svg)](https://github.com/your-organization/logAnalyzer/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)

A powerful and memory-efficient C++ tool designed to analyze, filter, and extract insights from large log files.

## Table of Contents

-   [Features](#features)
-   [Configuration](#configuration)
-   [Project Structure](#project-structure)
-   [Prerequisites](#prerequisites)
-   [Getting Started](#getting-started)
-   [Build Options](#build-options)
-   [Installation](#installation)
-   [Running Tests](#running-tests)
-   [Usage Examples](#usage-examples)
-   [Developer Tools](#developer-tools)
-   [Contributing](#contributing)
-   [Versioning](#versioning)
-   [Code of Conduct](#code-of-conduct)
-   [License](#license)

## Features

-   **Memory-Efficient Processing**: Utilizes a lazy, iterator-based approach to handle very large files with minimal memory usage.
-   **Multi-File Support**: Parses multiple log files and can merge sorted sources efficiently.
-   **Pluggable Architecture**:
    -   **Custom Parsers**: Define your own log parsing logic.
    -   **Custom Analyzers**: Create custom analysis routines.
-   **Advanced Filtering**: Build complex filter expressions with `AND`/`OR`/`NOT` logic.
-   **Asynchronous Processing**: Load and analyze files asynchronously with cancellation support.
-   **Live Tail Mode**: Monitor new log entries in real-time.
-   **Flexible Export**: Save results in Text, JSON, CSV, YAML, or XML formats.
-   **Contextual Viewing**: Display surrounding lines for filtered entries.

## Configuration

`LogAnalyzer` can be extensively configured using a JSON configuration file. This allows for persistent and complex setups for parsing, filtering, and exporting log data.

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
  "lineParsePattern": "^\\\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}" ([A-Z]+): (.*)$",
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
  "logEntryStartPattern": "^\\\\d{4}-\\d{2}-\\d{2}",
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
├── include/                 # Public header files for the logAnalyzer library, including CMake-generated configs (e.g., LogAnalyzerConfig.h.in)
├── src/                     # Source files for the logAnalyzer library and main executable
├── tests/                   # Unit and integration tests
├── .gitignore               # Files ignored by Git
├── CODE_OF_CONDUCT.md       # Project's Code of Conduct
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

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/your-organization/logAnalyzer.git
    cd logAnalyzer
    ```

2.  **Build the project:**
    ```bash
    mkdir build
    cd build
    cmake ..
    cmake --build .
    ```

3.  **Run a basic analysis:**
    ```bash
    ./bin/logAnalyzer ../sample.log
    ```

## Build Options

The following CMake options can be used to customize the build process:

-   `-DBUILD_TESTING=ON/OFF`: Toggles the compilation of unit tests (Default: `ON`).
-   `-DLOGANALYZER_BUILD_SHARED=ON/OFF`: Build the `logAnalyzer` as a shared library (Default: `OFF`).
-   `-DLOGANALYZER_USE_SANITIZER=Address/Undefined/None`: Enables various sanitizers (e.g., AddressSanitizer, UndefinedBehaviorSanitizer) to detect runtime errors (Default: `None`).

## Installation

After building, you can install `logAnalyzer` to your system:

```bash
cd build
cmake --install . --prefix /usr/local
```
This will install the executable to `/usr/local/bin` and libraries/headers to appropriate subdirectories within `/usr/local`.

## Running Tests

To execute the unit and integration tests:

```bash
cd build
ctest --verbose
```

## Usage Examples

### Basic Analysis

```bash
# Summary of a single log file
./bin/logAnalyzer sample.log

# Analyze multiple files
./bin/logAnalyzer log1.log log2.log
```

### Filtering

```bash
# Filter by level and keyword
./bin/logAnalyzer sample.log --level ERROR,WARNING --keyword "database"

# Regular expression filter
./bin/logAnalyzer sample.log --regex "Connection (timed out|refused)"
```

### Exporting Results

```bash
# Export to JSON
./bin/logAnalyzer sample.log --format json --output results.json
```

### Using a Configuration File

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
    API documentation will be generated in `build/docs/html` (or similar, depending on your build configuration) and can be viewed by opening `index.html` in your web browser.

-   **Format Code**:
    ```bash
    cmake --build . --target format
    ```
    This command applies consistent code formatting using `clang-format` (if available) across the codebase.

## Contributing

We welcome contributions! Please see our [Contributing Guidelines](CONTRIBUTING.md) for more details on how to get involved, report issues, and propose changes.

## Versioning

We use [SemVer](http://semver.org/) for versioning. For the versions available, see the [tags on this repository](https://github.com/your-organization/logAnalyzer/tags).

## Code of Conduct

Please read our [Code of Conduct](CODE_OF_CONDUCT.md) to understand the standards of behavior we expect from our community members and contributors.

## License

This project is licensed under the MIT License.
