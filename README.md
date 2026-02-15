# logAnalyzer

> **Unleash the Power of Your Logs: A High-Performance C++ Utility for Advanced Log Analysis.**

`logAnalyzer` is a blazing fast, command-line utility for parsing, filtering, and extracting insights from massive log files. Built with C++23, it leverages stream processing to handle datasets larger than available memory with ease.

[![Project Status: Active](https://img.shields.io/badge/Status-Active-brightgreen.svg?style=for-the-badge)](https://github.com/eserlxl/logAnalyzer)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/eserlxl/logAnalyzer?style=for-the-badge)](https://github.com/eserlxl/logAnalyzer/releases)
[![CI Status](https://github.com/eserlxl/logAnalyzer/actions/workflows/ci.yml/badge.svg?style=for-the-badge)](https://github.com/eserlxl/logAnalyzer/actions/workflows/ci.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg?style=for-the-badge)](https://www.gnu.org/licenses/gpl-3.0)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg?style=for-the-badge)](https://en.cppreference.com/w/cpp/23)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-blue.svg?style=for-the-badge)](https://cmake.org)

## Table of Contents

- [Why logAnalyzer?](#why-loganalyzer)
- [Features](#features)
- [Getting Started](#getting-started)
  - [Installation](#installation)
  - [Quick Start](#quick-start)
- [Usage](#usage)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License](#license)

## Why logAnalyzer?

`logAnalyzer` is designed to be a powerful, flexible, and easy-to-use tool for log analysis. Here are a few reasons why you might choose `logAnalyzer`:

-   **Performance**: Built in C++ for maximum speed, `logAnalyzer` can process large volumes of log data quickly. Its stream processing capabilities mean you're not limited by your system's RAM.
-   **Flexibility**: With advanced filtering options, multiple output formats, and a rich set of data types, `logAnalyzer` can be adapted to a wide range of log analysis tasks.
-   **Ease of Use**: A simple command-line interface, combined with powerful features like automatic type inference, makes `logAnalyzer` easy to learn and use.
-   **Extensibility**: The C++ API allows you to integrate `logAnalyzer`'s parsing and filtering capabilities directly into your own applications.

## Features

-   **High-Performance Stream Processing**: Process large log files without loading them into memory.
-   **Advanced Filtering**: Use field-based queries, regex, and logical operators.
-   **Flexible I/O**: Read from files or stdin and export to JSON, CSV, XML, or text.
-   **Built-in Analytics**: Get statistics like frequency counts and value distributions.
-   **Rich Data Types**: Filter by semantic versions, IP addresses, and booleans.
-   **Configurable**: Use command-line flags or JSON configuration files.
-   **Extensible C++ API**: Integrate `logAnalyzer` into your C++ projects.
-   **Multi-File Support**: Parse and analyze multiple log files in a single run.
-   **Sorting**: Sort results by timestamp, log level, or any other field.
-   **Structured Field Parsing**: Automatically parse log messages into fields using custom patterns.

For a full list of features, see the [Features documentation](docs/features.md).

## Getting Started

### Installation

To get started with `logAnalyzer`, you can build it from source.

#### Prerequisites

-   A C++23 compatible compiler (GCC 13+, Clang 16+).
-   CMake (version 3.14+).
-   Git.

#### Build

1.  Clone the repository:
    ```bash
    git clone https://github.com/eserlxl/logAnalyzer.git
    cd logAnalyzer
    ```

2.  Configure and build the project:
    ```bash
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ```

3.  Install the executable (optional):
    ```bash
    sudo cmake --install build
    ```

For more detailed instructions, see the [Build Guide](docs/build.md).

### Quick Start

-   **Filter by log level**:
    ```bash
    logAnalyzer /var/log/syslog --level ERROR
    ```

-   **Find entries with "database" or "timeout"**:
    ```bash
    logAnalyzer app.log --keyword "database" --keyword "timeout" --logic OR
    ```

-   **Use a complex expression**:
    ```bash
    logAnalyzer app.log --expression '(level=ERROR and msg contains "auth") or status_code >= 500'
    ```

-   **Export errors from the last 30 minutes to JSON**:
    ```bash
    logAnalyzer system.log --level ERROR --start "30m ago" --format json --pretty
    ```

For more examples, see the [Quick Start Guide](docs/quick-start.md) and [Usage Examples](docs/usage-examples.md).

## Usage

`logAnalyzer` can be run from the command line, with options to specify the input file, filtering criteria, and output format.

### Basic Filtering

Filter by a single log level:

```bash
logAnalyzer /path/to/your.log --level INFO
```

### Multiple Keywords

Search for logs containing either "error" or "warning":

```bash
logAnalyzer /path/to/your.log --keyword error --keyword warning --logic OR
```

### Time-Based Filtering

Show logs from the last 2 hours:

```bash
logAnalyzer /path/to/your.log --start "2h ago"
```

### Output Formatting

Export results to a CSV file:

```bash
logAnalyzer /path/to/your.log --level WARN --format csv --output warnings.csv
```

For a complete list of command-line options, refer to the [CLI Reference](docs/cli-reference.md).

## Documentation

For more detailed information, please refer to the following documents:

-   [**Why logAnalyzer?**](docs/why-loganalyzer.md)
-   [**Installation Guide**](docs/installation.md)
-   [**Quick Start Guide**](docs/quick-start.md)
-   [**Usage Examples**](docs/usage-examples.md)
-   [**Build Details**](docs/build.md)
-   [**CLI Reference**](docs/cli-reference.md)
-   [**Configuration Guide**](docs/configuration.md)
-   [**API Reference**](docs/api-reference.md)
-   [**Features**](docs/features.md)
-   [**Project Structure**](docs/project-structure.md)

## Contributing

Contributions are welcome. Please read our [Contributing Guide](CONTRIBUTING.md) to learn about our development process and how to set up your environment.

## License

This project is licensed under the GNU General Public License Version 3. See the [LICENSE](LICENSE) file for details.
