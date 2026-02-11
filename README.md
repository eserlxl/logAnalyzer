# LogAnalyzer

> **Unleash the Power of Your Logs: A High-Performance C++ Utility for Advanced Log Analysis.**

`logAnalyzer` is a blazing fast, command-line utility for parsing, filtering, and extracting insights from massive log files. Built with C++23, it leverages stream processing to handle datasets larger than available memory.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg?style=for-the-badge)](https://www.gnu.org/licenses/gpl-3.0)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg?style=for-the-badge)](https://en.cppreference.com/w/cpp/23)
[![CI Status](https://github.com/eserlxl/logAnalyzer/actions/workflows/ci.yml/badge.svg?style=for-the-badge)](https://github.com/eserlxl/logAnalyzer/actions/workflows/ci.yml)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-blue.svg?style=for-the-badge)](https://cmake.org)
[![Project Status: Active](https://img.shields.io/badge/Status-Active-brightgreen.svg?style=for-the-badge)](https://github.com/eserlxl/logAnalyzer)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/eserlxl/logAnalyzer?style=for-the-badge)](https://github.com/eserlxl/logAnalyzer/releases)

---
## 📚 Table of Contents
- [🤔 Why logAnalyzer?](#why-loganalyzer)
- [✨ Key Features](#key-features)
- [🚀 Installation](#-installation)
- [⚡ Quick Start](#-quick-start)
- [⚙️ Configuration](#️-configuration)
- [📚 Documentation](#-documentation)
- [🏗 Project Structure](#-project-structure)
- [💬 Support & Community](#-support--community)
- [🤝 Contributing](#-contributing)
- [📜 Changelog](#-changelog)
- [📄 License](#-license)

---
## 🤔 Why logAnalyzer?

Modern applications generate gigabytes of logs daily. While tools like `grep`, `awk`, or `less` are powerful, they often fall short when dealing with the scale and complexity of today's logging formats. `logAnalyzer` was built to solve these problems. It treats your logs as a structured data source, allowing you to query them with power and flexibility, right from your terminal. Its stream-based architecture ensures it can handle files of any size with minimal memory usage.

For a comprehensive explanation, see [**docs/why-loganalyzer.md**](docs/why-loganalyzer.md).

## ✨ Key Features

`logAnalyzer` provides a robust set of features designed for efficient log analysis:

*   **High Performance**: Built with C++23, it leverages stream processing for efficient handling of massive log files, even those larger than available memory.
*   **Advanced Filtering**: Powerful filtering capabilities including field-based queries, regular expressions, and logical operators to pinpoint relevant log entries.
*   **Built-in Analytics**: Extract statistics and insights directly from your logs.
*   **Multi-format Support**: Adapts to various log formats.
*   **Flexible I/O**: Process logs from files, `stdin`, and output to various formats like JSON, CSV, or plain text.
*   **Extensible C++ API**: Integrate `logAnalyzer`'s core functionalities into your own C++ applications, leveraging modern C++23 patterns such as `std::generator`, `std::expected`, and `std::span`.

For a comprehensive overview of all capabilities, see the [**full feature list**](docs/features.md).

## 🚀 Installation

To get `logAnalyzer` up and running on your system, follow these basic steps:

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
    The executable will be located at `build/logAnalyzer`.

3.  **Verify Installation:**
    ```bash
    ./build/logAnalyzer --version
    ```
    (If installed system-wide: `logAnalyzer --version`)

For detailed, platform-specific instructions, prerequisites, advanced build configurations, and optional system-wide installation, refer to the [**Installation Guide**](docs/installation.md) and the [**Build Guide**](docs/build.md).

## ⚡ Quick Start

Dive right in! Here are some common ways to use `logAnalyzer`:

### Filter for specific log levels
```bash
# Analyze a syslog file and filter only for ERROR level entries
logAnalyzer /var/log/syslog --level ERROR
```

### Advanced filtering with expressions
```bash
# Find database errors OR any message containing "timeout"
logAnalyzer app.log --expression '(level=ERROR and msg contains "database") or msg contains "timeout"'
```

### Export to JSON
```bash
# Find errors from the last 2 hours and export to a pretty JSON file
logAnalyzer app.log --start "2h ago" --level ERROR --format json --pretty --output errors.json
```

### Statistical Analysis
```bash
# Generate a report of the top 5 most frequent error messages
logAnalyzer system.log --level ERROR --stats "type=TOP_MESSAGES,top_n=5"
```

For a more detailed overview of basic usage, syntax, and more example commands, see our [**Quick Start Guide**](docs/quick-start.md) and [**Usage Examples**](docs/usage-examples.md).

## ⚙️ Configuration

`logAnalyzer` supports extensive configuration via command-line arguments or a JSON configuration file. Command-line arguments always override settings from a configuration file.

For full details on all configuration options and merging strategies, see the [**Configuration Guide**](docs/configuration.md).

## 📚 Documentation

For more in-depth information, explore the documentation in the [`docs/`](./docs) directory.

### User Documentation
- [**Why logAnalyzer?**](docs/why-loganalyzer.md)
- [**Features Overview**](docs/features.md)
- [**Quick Start Guide**](docs/quick-start.md)
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

This project is licensed under the GNU General Public License Version 3. See the [LICENSE](LICENSE) file for details.