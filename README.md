# logAnalyzer

> **Unleash the Power of Your Logs: A High-Performance C++ Utility for Advanced Log Analysis.**

`logAnalyzer` is a blazing fast, command-line utility for parsing, filtering, and extracting insights from massive log files. Built with C++23, it leverages stream processing to handle datasets larger than available memory.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Build Status](https://github.com/eserlxl/logAnalyzer/actions/workflows/cmake-ci.yml/badge.svg)](https://github.com/eserlxl/logAnalyzer/actions/workflows/cmake-ci.yml)
![Platform: Linux | macOS | Windows](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-blue)
[![Project Status: Active](https://img.shields.io/badge/Status-Active-brightgreen.svg)](https://github.com/eserlxl/logAnalyzer)

---
## 📚 Table of Contents
- [🤔 Why logAnalyzer?](#-why-loganalyzer)
- [✨ Key Features](#-key-features)
- [🚀 Getting Started](#-getting-started)
- [🏃 Quick Start & Basic Usage](#-quick-start--basic-usage)
- [📚 Documentation](#-documentation)
- [💬 Support & Community](#-support--community)
- [🤝 Contributing](#-contributing)
- [📜 Changelog](#-changelog)
- [📄 License](#-license)


---
## 🤔 Why logAnalyzer?

Modern applications generate gigabytes of logs daily. While tools like `grep`, `awk`, or `less` are powerful, they often fall short when dealing with the scale and complexity of today's logging formats. You've likely felt the pain of:

-   **Complex Queries**: Trying to filter logs by a specific time range and multiple keywords (`grep "ERROR" | grep "2023-10-27 10:"`) is cumbersome and inefficient.
-   **Lack of Structure**: Parsing structured formats like JSON or key-value pairs requires custom, often brittle, scripting.
-   **Performance Bottlenecks**: Searching multi-gigabyte files can be slow and memory-intensive, bringing your analysis to a crawl.
-   **No Built-in Analytics**: `grep` can find lines, but it can't tell you the rate of errors per minute or the top 10 most common log messages.

`logAnalyzer` was built to solve these problems. It treats your logs as a structured data source, allowing you to query them with power and flexibility, right from your terminal. Its stream-based architecture ensures it can handle files of any size with minimal memory usage.

## ✨ Key Features

- **⚡ High Performance**: Written in C++23. Processes logs as streams with minimal memory footprint.
- **🔍 Advanced Filtering Engine**:
    - **Complex Logical Expressions**: Combine filters with nested `AND`, `OR`, and `NOT` operators, e.g., `(level=ERROR OR level=WARN) AND NOT msg contains "noise"`.
    - **Rich Set of Operators**: Use a wide array of operators: `=`, `!=`, `>`, `<`, `>=`, `<=`, `contains`, `starts_with`, `ends_with`, `regex`, `in`, `not in`, `is present`, `is null`.
    - **Case-Insensitive Matching**: Use operators like `contains_i` or `equals_i` for case-insensitive searches.
    - **Typed Comparisons**: Explicitly compare fields as `string`, `int`, `double`, `bool`, `ip_address`, `version`, or `datetime`.
    - **Auto-Typed Values**: The engine automatically detects types like numbers, booleans, IPs, and versions for simpler queries.
    - **JSON & Nested Fields**: Filter on nested fields within JSON logs, e.g., `custom.user.id = 123`.
- **📊 Statistical Analysis**: Generate instant reports on entry rates, top error messages, and more.
- **📂 Multi-File & Sorting**: Analyze multiple files at once and sort results by any field.
- **🛠 Structured Support**: Native parsing for JSON logs and customizable text patterns, with the ability to apply different patterns to different files.
- **⚙️ Configurable**: Use JSON configuration files for persistent, complex setups.
- **📤 Flexible Export**: Output to Text, CSV, JSON, or XML.
- **🔄 Live Monitoring**: Tail files in real-time with filtering applied (`--tail`).

## 🚀 Getting Started

Follow these steps to get `logAnalyzer` running on your system.

### Prerequisites

- C++23 compatible compiler (GCC 13+, Clang 16+)
- CMake (3.16+)
- Git

For detailed, platform-specific instructions, please refer to the [**Installation Guide**](docs/installation.md).

### Quick Start (Build & Run)

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/eserlxl/logAnalyzer.git
    cd logAnalyzer
    ```

2.  **Build the project:**
    ```bash
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ```

3.  **Run the application:**
    ```bash
    ./build/bin/logAnalyzer --help
    ```

4.  **Run Tests (Optional):**
    ```bash
    cd build
    ctest
    cd ..
    ```
For more comprehensive build instructions, including platform-specific details and advanced configurations, please see the [**Build Guide**](docs/build.md).

## 🏃 Quick Start & Basic Usage

`logAnalyzer` is a versatile tool. Here’s a quick overview of its command-line interface.

### Basic Syntax

```bash
./build/bin/logAnalyzer [input-file] [options]
```

### Examples

- **Process a log file from `stdin`:**
  ```bash
  echo "INFO 2023-10-27 12:30:00 This is a test log message." | ./build/bin/logAnalyzer
  ```

- **Analyze a specific log file:**
  ```bash
  ./build/bin/logAnalyzer /var/log/syslog
  ```

- **Use a complex expression to find database errors OR any message containing "timeout":**
  ```bash
  ./build/bin/logAnalyzer app.log --expression '(level=ERROR and msg contains "database") or msg contains "timeout"'
  ```

- **Generate statistics on the top 5 most common error messages:**
  ```bash
  ./build/bin/logAnalyzer system.log --level ERROR --stats "type=TOP_MESSAGES,top_n=5"
  ```

- **Export errors from the last 2 hours to a JSON file:**
  ```bash
  ./build/bin/logAnalyzer app.log --start "2h ago" --level ERROR --format json --pretty --output errors.json
  ```

For a deep dive into all functionalities, including advanced filtering and configuration, check out our [**Usage Examples**](docs/usage-examples.md) and [**CLI Reference**](docs/cli-reference.md).

## 📚 Documentation

For more in-depth information, explore the documentation in the [`docs/`](./docs) directory.

### User Documentation
- [**Features Overview**](docs/features.md)
- [**Installation Guide**](docs/installation.md)
- [**Command Line Reference**](docs/cli-reference.md)
- [**Configuration Guide**](docs/configuration.md)
- [**Usage Examples**](docs/usage-examples.md)

### Developer Documentation
- [**Build Guide**](docs/build.md)
- [**Project Structure**](docs/project-structure.md)
- [**API Reference**](docs/api-reference.md)
- [**Contributing Guide**](CONTRIBUTING.md)
- [**Code of Conduct**](CODE_OF_CONDUCT.md)

## 💬 Support & Community

Have a question, found a bug, or have a feature request? We'd love to hear from you!

-   **Bugs & Feature Requests**: Please open an issue on our [GitHub Issues page](https://github.com/eserlxl/logAnalyzer/issues).
-   **Questions**: Feel free to start a discussion on our [GitHub Discussions page](https://github.com/eserlxl/logAnalyzer/discussions).

## 🤝 Contributing

We welcome contributions! If you'd like to help improve `logAnalyzer`, please see our [**Contributing Guide**](CONTRIBUTING.md) for details on how to get started.

## 📜 Changelog

All notable changes are documented in the [`CHANGELOG.md`](CHANGELOG.md) file.

## 📄 License

This project is licensed under the GPL-3.0 License. See the [LICENSE](LICENSE) file for details.

## 🤝 Code of Conduct

To ensure a welcoming and inclusive community, please review and adhere to our [**Code of Conduct**](CODE_OF_CONDUCT.md).
