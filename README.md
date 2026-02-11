# logAnalyzer

> **Unleash the Power of Your Logs: A High-Performance C++ Utility for Advanced Log Analysis.**

`logAnalyzer` is a blazing fast, command-line utility for parsing, filtering, and extracting insights from massive log files. Built with C++23, it leverages stream processing to handle datasets larger than available memory.

[![Project Status: Active](https://img.shields.io/badge/Status-Active-brightgreen.svg?style=for-the-badge)](https://github.com/eserlxl/logAnalyzer)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/eserlxl/logAnalyzer?style=for-the-badge)](https://github.com/eserlxl/logAnalyzer/releases)
[![CI Status](https://github.com/eserlxl/logAnalyzer/actions/workflows/ci.yml/badge.svg?style=for-the-badge)](https://github.com/eserlxl/logAnalyzer/actions/workflows/ci.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg?style=for-the-badge)](https://www.gnu.org/licenses/gpl-3.0)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg?style=for-the-badge)](https://en.cppreference.com/w/cpp/23)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-blue.svg?style=for-the-badge)](https://cmake.org)


---
## 📚 Table of Contents
- [✨ Key Features](#-key-features)
- [🚀 Getting Started](#-getting-started)
- [🛠️ Build from Source](#️-build-from-source)
- [💡 Usage](#-usage)
- [⚡ Quick Start](#-quick-start)
- [⚙️ Configuration](#️-configuration)
- [📚 Documentation](#-documentation)
- [🏗 Project Structure](#-project-structure)
- [🤝 Contributing & Support](#-contributing--support)
- [📜 Changelog](#-changelog)
- [📄 License](#-license)

---

## ✨ Key Features

`logAnalyzer` provides a robust set of features designed for efficient log analysis:

*   **High-Performance Stream Processing**: Built with C++23, it processes massive log files without consuming excessive memory.
*   **Advanced Filtering**: Use field-based queries, regular expressions, and complex logical operators (`AND`, `OR`, `NOT`) to pinpoint the exact log entries you need.
*   **Flexible I/O**: Reads from files and `stdin`. Exports to JSON, CSV, or custom text formats.
*   **Built-in Analytics**: Generate statistics like frequency counts and value distributions directly from your logs.
*   **Extensible C++ API**: Integrate `logAnalyzer`'s core functionalities into your own C++ applications.

For a comprehensive overview of all capabilities, see the [**full feature list**](docs/features.md).

## 🚀 Getting Started

Get up and running with `logAnalyzer` in just a few steps.

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

3.  **Run your first analysis:**
    Pipe `dmesg` output to `logAnalyzer` and filter for entries containing "error".
    ```bash
    dmesg | ./build/bin/logAnalyzer --level error
    ```

## 🛠️ Build from Source

For detailed, platform-specific instructions, prerequisites, and advanced build configurations, refer to the [**Build Guide**](docs/build.md).

The executable will be located at `build/bin/logAnalyzer`. To install it system-wide (optional):

```bash
sudo cmake --install build
```

Verify the installation by checking the version:
```bash
logAnalyzer --version
```

## 💡 Usage

The basic syntax for `logAnalyzer` is:

```bash
logAnalyzer [options] [log_file]
```

-   `[options]`: Flags to control filtering, output format, and other behaviors.
-   `[log_file]`: The path to the log file to analyze. If omitted, `logAnalyzer` reads from `stdin`.

For a complete list of all command-line arguments, see the [**Command Line Reference**](docs/cli-reference.md).

## ⚡ Quick Start

Here are some common examples to get you started.

### Filter by Log Level
Analyze a syslog file and show only `ERROR` level entries.
```bash
logAnalyzer /var/log/syslog --level ERROR
```

### Advanced Filtering with Expressions
Find database errors OR any message containing "timeout".
```bash
logAnalyzer app.log --expression '(level=ERROR and msg contains "database") or msg contains "timeout"'
```

### Time-based Filtering
Show all warnings from the last 30 minutes.
```bash
logAnalyzer system.log --level WARN --start "30m ago"
```

### Export to JSON
Find errors from the last 2 hours and export them to a pretty-printed JSON file.
```bash
logAnalyzer app.log --start "2h ago" --level ERROR --format json --pretty --output errors.json
```

For more examples, see the [**Usage Examples**](docs/usage-examples.md) guide.

## ⚙️ Configuration

`logAnalyzer` can be configured via command-line arguments or a JSON configuration file. Command-line arguments always override settings from a configuration file.

For full details on all options, see the [**Configuration Guide**](docs/configuration.md).

## 📚 Documentation

For more in-depth information, explore the complete documentation.

### User Documentation
- [**Why logAnalyzer?**](docs/why-loganalyzer.md)
- [**Features Overview**](docs/features.md)
- [**Installation Guide**](docs/installation.md)
- [**Quick Start Guide**](docs/quick-start.md)
- [**Command Line Reference**](docs/cli-reference.md)
- [**Configuration Guide**](docs/configuration.md)
- [**Usage Examples**](docs/usage-examples.md)

### Developer Documentation
- [**Build Guide**](docs/build.md)
- [**API Reference**](docs/api-reference.md)
- [**Project Structure**](docs/project-structure.md)

## 🤝 Contributing & Support

We welcome contributions! Please see our [**Contributing Guide**](CONTRIBUTING.md) for details on how to get started, report bugs, or request features. All contributors are expected to adhere to our [**Code of Conduct**](CODE_OF_CONDUCT.md).

## 📜 Changelog

All notable changes are documented in the [`CHANGELOG.md`](CHANGELOG.md) file.

## 📄 License

This project is licensed under the GNU General Public License Version 3. See the [LICENSE](LICENSE) file for details.