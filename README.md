# logAnalyzer

**Unleash the Power of Your Logs: A High-Performance C++ Utility for Advanced Log Analysis.**

A high-performance C++ command-line utility for advanced log analysis, filtering, and statistical insights.

[![Build Status](https://github.com/eserlxl/logAnalyzer/actions/workflows/cmake.yml/badge.svg?branch=main)](https://github.com/eserlxl/logAnalyzer/actions)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
![CMake](https://img.shields.io/badge/cmake-3.20%2B-blue.svg)
[![Code style: clang-format](https://img.shields.io/badge/code%20style-clang--format-blue.svg)](https://clang.llvm.org/docs/ClangFormat.html)
[![Doxygen Documentation](https://img.shields.io/badge/docs-Doxygen-blue.svg)](https://eserlxl.github.io/logAnalyzer/)
[![codecov](https://codecov.io/gh/eserlxl/logAnalyzer/branch/main/graph/badge.svg)](https://codecov.io/gh/eserlxl/logAnalyzer)

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
    git clone https://github.com/eserlxl/logAnalyzer.git
    cd logAnalyzer
    ```

2.  **Build from Source**:
    Ensure you have a C++23 compatible compiler (e.g., GCC 13+, Clang 16+) and CMake 3.20+ installed.
    ```bash
    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build . # For faster compilation, append -j<num_jobs> (e.g., -j$(nproc) on Linux/macOS, or omit on Windows).
    ```

3.  **Run a Basic Analysis**:
    After building, the executable will be in the `build/bin` directory. You can run it directly:
    ```bash
    ./build/bin/LogAnalyzer /var/log/syslog --level ERROR
    # Note: The actual executable name might be 'logAnalyzer' (lowercase 'l') depending on your build environment or OS.
    # Please verify the exact name in 'build/bin/' if the above command fails.
    ```
    *(Replace `/var/log/syslog` with a path to one of your log files.)*

    For easier access, consider installing it system-wide or adding `build/bin` to your system's `PATH`. See the [Installation](#installation) section for details.

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
| **Complex Filter Expressions** | (Coming Soon) Build sophisticated filter logic using parenthesized, nested `AND`/`OR`/`NOT` conditions.                     |
| **Flexible Export**          | Save results in Text, JSON, or CSV formats with customizable and aliasable output fields.                 |
| **Statistical Analysis**     | Generate statistics on log data, such as entry rates, top messages, log level counts, and unique value counts for any field. |

## Building from Source

See [docs/build.md](docs/build.md) for detailed build instructions.


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

See [docs/usage-examples.md](docs/usage-examples.md) for detailed usage instructions and examples.

## Command-Line Interface (CLI)

See [docs/cli-reference.md](docs/cli-reference.md) for a comprehensive list of command-line options and their descriptions.

## Configuration

See [docs/configuration.md](docs/configuration.md) for details on how to use JSON configuration files for complex setups.

## Project Structure

See [docs/project-structure.md](docs/project-structure.md) for an overview of the project layout.

## Contributing

We welcome contributions! Please see our [Contributing Guidelines](CONTRIBUTING.md) for information on how to build the project, run tests, and submit your changes.

## Code of Conduct

Please read our [Code of Conduct](CODE_OF_CONDUCT.md).

## License

This project is licensed under the terms of the [GPL-3.0 license](LICENSE).
