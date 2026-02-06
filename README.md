# logAnalyzer

> **Unleash the Power of Your Logs: A High-Performance C++ Utility for Advanced Log Analysis.**

`logAnalyzer` is a blazing fast, command-line utility for parsing, filtering, and extracting insights from massive log files. Built with C++23, it leverages stream processing to handle datasets larger than available memory.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
![Platform: Linux | macOS | Windows](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-blue)
[![Project Status: Active](https://img.shields.io/badge/Status-Active-brightgreen.svg)](https://github.com/eserlxl/logAnalyzer)

---
## 📚 Table of Contents
- [🤔 Why logAnalyzer?](#-why-loganalyzer)
- [✨ Key Features](#-key-features)
- [🚀 Getting Started](#-getting-started)
- [📚 Documentation](#-documentation)
- [📜 Changelog](#-changelog)
- [🤝 Contributing](#-contributing)


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
- **🔍 Advanced Filtering**:
    - Filter by **Log Level** (ERROR, WARN, INFO).
    - Filter by **Time Range** (Absolute or Relative).
    - **Complex Expressions**: `(level=ERROR AND msg contains "timeout") OR duration > 500ms`.
- **📊 Statistical Analysis**: Generate instant reports on entry rates, top error messages, and more.
- **📂 Multi-File & Sorting**: Analyze multiple files at once and sort results by any field.
- **🛠 Structured Support**: Native parsing for JSON logs and customizable text patterns.
- **⚙️ Configurable**: Use JSON configuration files for persistent, complex setups.
- **📤 Flexible Export**: Output to Text, CSV, JSON, or XML.
- **🔄 Live Monitoring**: Tail files in real-time with filtering applied (`--tail`).

## 🚀 Getting Started

To get `logAnalyzer` up and running, follow these simple steps. For detailed instructions, refer to the [**Installation Guide**](docs/installation.md).

### Installation

1.  **Prerequisites**: Ensure you have a C++23 compatible compiler (e.g., GCC 13+, Clang 16+), CMake (3.16 or higher), and Git installed.
    For more detailed installation steps, including platform-specific instructions, see [**docs/installation.md**](docs/installation.md).

2.  **Clone the repository:**
    ```bash
    git clone https://github.com/eserlxl/logAnalyzer.git
    cd logAnalyzer
    ```

3.  **Build the project:**
    ```bash
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ```
    This command prepares the build and compiles the source code into an executable. For more build options and details, see [**docs/build.md**](docs/build.md).

### Quick Start

Once built, you can immediately start analyzing your logs.

1.  **Run your first analysis:**
    Execute the compiled binary, pointing it to a log file. Let's find all "ERROR" level messages in your system's log.
    ```bash
    ./build/bin/logAnalyzer /var/log/syslog --level ERROR
    ```

2.  **Filter by time:**
    Narrow down the search to a specific timeframe.
    ```bash
    ./build/bin/logAnalyzer app.log --after "2023-10-27 10:00:00" --before "2023-10-27 11:00:00"
    ```

For more advanced usage scenarios and a complete list of commands, check out our [**Usage Examples**](docs/usage-examples.md) and the comprehensive [**Command Line Reference**](docs/cli-reference.md).


## 📚 Documentation

All documentation is located in the [`docs/`](./docs) directory.

### Usage
- [**Features Overview**](docs/features.md): A detailed look at what `logAnalyzer` can do.
- [**Installation Guide**](docs/installation.md): How to install `logAnalyzer` on your system.
- [**Command Line Reference**](docs/cli-reference.md): A complete guide to all flags and arguments.
- [**Configuration Guide**](docs/configuration.md): How to use JSON for advanced setups.
- [**Usage Examples**](docs/usage-examples.md): Practical examples for common scenarios.

### Development
- [**Build Guide**](docs/build.md): Instructions for compiling from the source.
- [**Project Structure**](docs/project-structure.md): An overview of the codebase organization.

### Community
- [**Contributing Guide**](CONTRIBUTING.md): How to contribute to the project.
- [**Code of Conduct**](CODE_OF_CONDUCT.md): Our community standards.
- [**License**](LICENSE): The project's license.

## 📜 Changelog

All notable changes to this project are documented in the [`CHANGELOG.md`](CHANGELOG.md) file.

## 🤝 Contributing

Contributions are welcome! Please see the [Contributing Guide](CONTRIBUTING.md) for details on how to get started, report bugs, and suggest features.

## 📄 License

This project is licensed under the GPL-3.0 License. See the [LICENSE](LICENSE) file for details.
