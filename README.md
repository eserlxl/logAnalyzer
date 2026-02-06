# logAnalyzer

> **Unleash the Power of Your Logs: A High-Performance C++ Utility for Advanced Log Analysis.**

`logAnalyzer` is a blazing fast, command-line utility for parsing, filtering, and extracting insights from massive log files. Built with C++23, it leverages stream processing to handle datasets larger than available memory.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
![Platform: Linux | macOS | Windows](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-blue)

---

## 🚀 Overview

Modern applications generate gigabytes of logs daily. Traditional tools like `grep` or `awk` struggle with:
- **Complex filtering** (e.g., "ERRORs between 10 AM and 11 AM").
- **Structured data** (e.g., parsing JSON logs).
- **Performance** on multi-GB files.

`logAnalyzer` solves this with a **stream-based architecture** and a powerful **filtering engine**, allowing you to query logs like a database from your terminal.

## ✨ Key Features

- **⚡ High Performance**: Written in C++23. Processes logs as streams with minimal memory footprint.
- **🔍 Advanced Filtering**:
    - Filter by **Log Level** (ERROR, WARN, INFO).
    - Filter by **Time Range** (Absolute or Relative).
    - **Complex Expressions**: `(level=ERROR AND msg contains "timeout") OR duration > 500ms`.
- **📊 Statistical Analysis**: Generate instant reports on entry rates, top error messages, and more.
- **🛠 Structured Support**: Native parsing for JSON logs and customizable text patterns.
- **📤 Flexible Export**: Output to Text, CSV, JSON, or XML.
- **🔄 Live Monitoring**: Tail files in real-time with filtering applied (`--tail`).

## 🚀 Getting Started

### Prerequisites

Before you can build and run `logAnalyzer`, ensure you have the following installed:

*   **Git**: For cloning the repository.
*   **CMake**: Version 3.15 or higher, for managing the build process.
*   **C++23 Compatible Compiler**: Such as GCC (13 or newer), Clang (16 or newer), or MSVC (Visual Studio 2022 v17.8 or newer).

### Build from Source

Follow these steps to clone the repository and build `logAnalyzer`:

```bash
git clone https://github.com/eserlxl/logAnalyzer.git
cd logAnalyzer
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```
For more detailed installation instructions, including platform-specific notes and dependency management, please refer to the [Installation Guide](docs/installation.md).

### Quick Usage Examples

Here are a few common use cases to get you started:

**Basic Filter:** Get all ERROR logs from a file.
```bash
./bin/logAnalyzer /var/log/syslog --level ERROR
```

**Time Range & Keyword:** Find "database" errors from the last hour.
```bash
./bin/logAnalyzer /var/log/app.log --keyword "database" --start "1 hour ago"
```

**Statistical Insight:** See the top occurring log messages.
```bash
./bin/logAnalyzer /var/log/app.log --stats top_messages:5
```
For a comprehensive list of commands and advanced filtering options, consult the [Command Line Reference](docs/cli-reference.md) and [Usage Examples](docs/usage-examples.md).

## 📚 Documentation

Detailed documentation is available in the `docs/` directory:

- [**Installation Guide**](docs/installation.md)
- [**Command Line Reference**](docs/cli-reference.md)
- [**Usage Examples**](docs/usage-examples.md)
- [**Configuration Guide**](docs/configuration.md)
- [**Project Structure**](docs/project-structure.md)

## 🤝 Contributing

We welcome contributions from the community! Whether it's reporting bugs, suggesting new features, or submitting code, your help is invaluable. Please see our [CONTRIBUTING.md](CONTRIBUTING.md) guide for detailed instructions on how to get started.

## 📄 License

This project is licensed under the [GPL-3.0 License](LICENSE).
