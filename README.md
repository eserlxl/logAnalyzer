# LogAnalyzer

[![Build Status](https://github.com/your-organization/logAnalyzer/actions/workflows/ci.yml/badge.svg)](https://github.com/your-organization/logAnalyzer/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)

A powerful and memory-efficient C++ tool designed to analyze, filter, and extract insights from large log files.

## Table of Contents

-   [Features](#features)
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
-   **Customizable Parsing**:
    -   Map custom log level strings.
    -   Custom timestamp formats.
    -   Configurable log line patterns via regular expressions.
-   **Statistical Analysis**:
    -   Unique message counts and top N frequent messages.
    -   Log frequency distribution over time.
-   **Flexible Export**: Save results in Text, JSON, CSV, YAML, or XML formats.
-   **Contextual Viewing**: Display surrounding lines for filtered entries.
-   **Multi-line Log Entry Support**: Define a regex pattern to identify the start of a new log entry.

## Prerequisites

-   **Compiler**: C++23 compatible compiler (e.g., GCC 13+, Clang 16+)
-   **Build System**: CMake 3.20 or higher
-   **Dependencies**: (Automatically handled via FetchContent)
    -   [CLI11](https://github.com/CLIUtils/CLI11)
    -   [nlohmann/json](https://github.com/nlohmann/json)
    -   [GoogleTest](https://github.com/google/googletest)

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

-   `-DBUILD_TESTING=ON/OFF`: Toggles the compilation of unit tests (Default: `ON`).
-   `-DLOGANALYZER_BUILD_SHARED=ON/OFF`: Build as a shared library (Default: `OFF`).
-   `-DLOGANALYZER_USE_SANITIZER=Address/Undefined/None`: Enables sanitizers (Default: `None`).

## Installation

```bash
cd build
cmake --install . --prefix /usr/local
```

## Running Tests

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

For a complete list of options, run:
```bash
./bin/logAnalyzer --help
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

We welcome contributions! Please see our [Contributing Guidelines](CONTRIBUTING.md) for more details.

## Versioning

We use [SemVer](http://semver.org/) for versioning. For the versions available, see the [tags on this repository](https://github.com/your-organization/logAnalyzer/tags).

## Code of Conduct

Please read our [Code of Conduct](CODE_OF_CONDUCT.md) to understand the standards of behavior we expect from our community.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE.md) file for details.
