# LogAnalyzer

> **Unleash the Power of Your Logs: A High-Performance C++ Utility for Advanced Log Analysis.**

`logAnalyzer` is a blazing fast, command-line utility for parsing, filtering, and extracting insights from massive log files. Built with C++23, it leverages stream processing to handle datasets larger than available memory.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg?style=for-the-badge)](https://www.gnu.org/licenses/gpl-3.0)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg?style=for-the-badge)](https://en.cppreference.com/w/cpp/23)
[![Build Status](https://img.shields.io/github/actions/workflow/status/eserlxl/logAnalyzer/cmake-ci.yml?branch=main&style=for-the-badge)](https://github.com/eserlxl/logAnalyzer/actions/workflows/cmake-ci.yml)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-blue.svg?style=for-the-badge)](https://cmake.org)
[![Project Status: Active](https://img.shields.io/badge/Status-Active-brightgreen.svg?style=for-the-badge)](https://github.com/eserlxl/logAnalyzer)

---
## 📚 Table of Contents
- [🤔 Why logAnalyzer?](#-why-loganalyzer)
- [✨ Key Features](#-key-features)
- [🚀 Getting Started](#-getting-started)
- [⚡ Basic Usage](#-basic-usage)
- [⚙️ Configuration](#️-configuration)
- [📚 Documentation](#-documentation)
- [🏗 Project Structure](#-project-structure)
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

- **⚡ High-Performance C++ Core**: Processes massive log files as streams with minimal memory usage.
- **🔍 Advanced Filtering**: Build complex queries with `AND`/`OR`/`NOT`, rich operators, and nested JSON field support.
- **📊 Built-in Analytics**: Generate statistics like entry rates and top messages on the fly.
- **🛠 Multi-Format Support**: Natively handles JSON and custom text patterns.
- **📤 Flexible I/O**: Read from files or `stdin` and export to Text, CSV, or JSON.
- **🔄 Live Monitoring**: Tail files in real-time with live filtering.

For a comprehensive overview of all capabilities, see the [**full feature list**](docs/features.md).

## 🚀 Getting Started

Follow these steps to get `logAnalyzer` running on your system.

### Prerequisites

-   **C++ Compiler**: C++23 compatible (GCC 13+ or Clang 16+).
-   **Build System**: CMake (3.16+).
-   **Version Control**: Git.

### Installation

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/eserlxl/logAnalyzer.git
    cd logAnalyzer
    ```

2.  **Build the project:**
    ```bash
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ```

    The executable will be located at `build/bin/logAnalyzer`.

3.  **Install (Optional):**
    To install `logAnalyzer` to your system path:
    ```bash
    sudo cmake --install build
    ```

4.  **Verify Installation:**
    ```bash
    logAnalyzer --version
    ```

For detailed, platform-specific instructions, refer to the [**Installation Guide**](docs/installation.md).

### Running Tests

To ensure everything is working correctly:

```bash
cd build && ctest
```

For more comprehensive build instructions, including platform-specific details and advanced configurations, please see the [**Build Guide**](docs/build.md).

## ⚡ Basic Usage

`logAnalyzer` is a versatile tool. Here’s a quick overview of its command-line interface.

### Basic Syntax

```bash
logAnalyzer [input-file] [options]
```

### Examples

- **Process a log file from `stdin`:**
  ```bash
  echo "INFO 2023-10-27 12:30:00 This is a test log message." | logAnalyzer
  ```

- **Analyze a specific log file:**
  ```bash
  logAnalyzer /var/log/syslog
  ```

- **Use a complex expression to find database errors OR any message containing "timeout":**
  ```bash
  logAnalyzer app.log --expression '(level=ERROR and msg contains "database") or msg contains "timeout"'
  ```

- **Generate statistics on the top 5 most common error messages:**
  ```bash
  logAnalyzer system.log --level ERROR --stats "type=TOP_MESSAGES,top_n=5"
  ```

- **Export errors from the last 2 hours to a JSON file:**
  ```bash
  logAnalyzer app.log --start "2h ago" --level ERROR --format json --pretty --output errors.json
  ```

For a deep dive into all functionalities, check out our [**Usage Examples**](docs/usage-examples.md) and [**CLI Reference**](docs/cli-reference.md).

## ⚙️ Configuration

`logAnalyzer` supports extensive configuration via command-line arguments or a JSON configuration file.

-   **Command Line**: Overrides config file settings.
-   **Config File**: Use `--config path/to/config.json` for persistent settings.

Example `config.json` snippet:
```json
{
  "filterRules": [
    { "field": "level", "operator": "EQUALS", "value": "ERROR" }
  ],
  "exportSettings": { "format": "json", "prettyPrint": true }
}
```

For full details on configuration options, see the [**Configuration Guide**](docs/configuration.md).

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
- [**API Reference**](docs/api-reference.md)
- [**Contributing Guide**](CONTRIBUTING.md)

## 🏗 Project Structure

An overview of the project's directory and code structure is available in the [**Project Structure Guide**](docs/project-structure.md).

## 💬 Support & Community

Have a question, found a bug, or have a feature request? We'd love to hear from you!

-   **Bugs & Feature Requests**: Please open an issue on our [GitHub Issues page](https://github.com/eserlxl/logAnalyzer/issues).
-   **Questions**: Feel free to start a discussion on our [GitHub Discussions page](https://github.com/eserlxl/logAnalyzer/discussions).

## 🤝 Contributing

We welcome contributions! If you'd like to help improve `logAnalyzer`, please see our [**Contributing Guide**](CONTRIBUTING.md) for details on how to get started.

To ensure a welcoming and inclusive community, please review and adhere to our [**Code of Conduct**](CODE_OF_CONDUCT.md).

## 📜 Changelog

All notable changes are documented in the [`CHANGELOG.md`](CHANGELOG.md) file.

## 📄 License

This project is licensed under the GPL-3.0 License. See the [LICENSE](LICENSE) file for details.
