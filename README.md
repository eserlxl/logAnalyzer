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
- **📂 Multi-File & Sorting**: Analyze multiple files at once and sort results by any field.
- **🛠 Structured Support**: Native parsing for JSON logs and customizable text patterns.
- **⚙️ Configurable**: Use JSON configuration files for persistent, complex setups.
- **📤 Flexible Export**: Output to Text, CSV, JSON, or XML.
- **🔄 Live Monitoring**: Tail files in real-time with filtering applied (`--tail`).

## 🚀 Getting Started

### Installation & Building

`logAnalyzer` is primarily built from source.

- **Build Instructions**: See [docs/build.md](docs/build.md) for prerequisites and step-by-step build commands.
- **Installation**: See [docs/installation.md](docs/installation.md) for installing the binary to your system path.

### Quick Start

**1. Build (if not already done):**
```bash
git clone https://github.com/eserlxl/logAnalyzer.git
cd logAnalyzer
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

**2. Run a basic analysis:**
```bash
# Navigate to project root
cd ../

# Analyze a log file for ERRORs
./build/bin/logAnalyzer /var/log/syslog --level ERROR

# Analyze logs from a specific time range
./build/bin/logAnalyzer app.log --after "2023-10-27 10:00:00" --before "2023-10-27 11:00:00"
```

**3. Use a Configuration File:**
For complex rules, use a JSON config file:
```bash
./build/bin/logAnalyzer app.log --config my_config.json
```

For more examples, see [Usage Examples](docs/usage-examples.md) and the [Command Line Reference](docs/cli-reference.md).

## 📚 Documentation

Detailed documentation is available in the `docs/` directory:

- [**Features Overview**](docs/features.md): In-depth look at capabilities.
- [**Build Guide**](docs/build.md): Compiling from source.
- [**Installation Guide**](docs/installation.md): System installation.
- [**Command Line Reference**](docs/cli-reference.md): Flags and arguments.
- [**Usage Examples**](docs/usage-examples.md): Common use cases.
- [**Configuration Guide**](docs/configuration.md): JSON configuration format.
- [**Project Structure**](docs/project-structure.md): Codebase organization.

## 🤝 Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for details on how to get started, report bugs, or suggest features.

## 📄 License

This project is licensed under the [GPL-3.0 License](LICENSE).
