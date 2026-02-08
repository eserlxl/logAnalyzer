# LogAnalyzer

> **Unleash the Power of Your Logs: A High-Performance C++ Utility for Advanced Log Analysis.**

`logAnalyzer` is a blazing fast, command-line utility for parsing, filtering, and extracting insights from massive log files. Built with C++23, it leverages stream processing to handle datasets larger than available memory.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg?style=for-the-badge)](https://www.gnu.org/licenses/gpl-3.0)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg?style=for-the-badge)](https://en.cppreference.com/w/cpp/23)
[![Build Status](https://img.shields.io/badge/Build%20Status-passing-brightgreen?style=for-the-badge)](https://github.com/eserlxl/logAnalyzer)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-blue.svg?style=for-the-badge)](https://cmake.org)
[![Project Status: Active](https://img.shields.io/badge/Status-Active-brightgreen.svg?style=for-the-badge)](https://github.com/eserlxl/logAnalyzer)

---
## 📚 Table of Contents
- [🤔 Why logAnalyzer?](#-why-loganalyzer)
- [✨ Key Features](#-key-features)
- [🚀 Getting Started](#-getting-started)
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

`logAnalyzer` offers a rich set of features for efficient log analysis, including high-performance processing, advanced filtering capabilities (e.g., field-based, regex, and logical operators), built-in analytics, multi-format support, flexible I/O, and a C++ API.

For a comprehensive overview of all capabilities, see the [**full feature list**](docs/features.md).

## 🚀 Getting Started

Follow these steps to get `logAnalyzer` running on your system.

### Prerequisites

-   **C++ Compiler**: C++23 compatible (GCC 13+ or Clang 16+).
-   **Build System**: CMake (3.14+).
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

    The executable will be located at `build/logAnalyzer`.

3.  **Install (Optional):**
    To install `logAnalyzer` to your system path:
    ```bash
    sudo cmake --install build
    ```

4.  **Verify Installation:**
    ```bash
    logAnalyzer --version
    ```

For detailed, platform-specific instructions, refer to the [**Installation Guide**](docs/installation.md) and the [**Build Guide**](docs/build.md) for advanced configurations.

## ⚡ Quick Start

`logAnalyzer` is a versatile tool. Here’s a quick overview of its command-line interface.

### Basic Syntax

```bash
logAnalyzer [input-file] [options]
```

### Example

- **Analyze a specific log file and filter for errors:**
  ```bash
  logAnalyzer /var/log/syslog --level ERROR
  ```

For a deep dive into all functionalities and more detailed examples, check out our [**Usage Examples**](docs/usage-examples.md) and [**CLI Reference**](docs/cli-reference.md).

## ⚙️ Configuration

`logAnalyzer` supports extensive configuration via command-line arguments or a JSON configuration file. Command-line arguments always override settings from a configuration file.

For full details on all configuration options and merging strategies, see the [**Configuration Guide**](docs/configuration.md).

## 📚 Documentation

For more in-depth information, explore the documentation in the [`docs/`](./docs) directory.

### User Documentation
- [**Why logAnalyzer?**](docs/why-loganalyzer.md)
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
