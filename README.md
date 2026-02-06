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
-   [Command-Line Interface (CLI)](#command-line-interface-cli)
-   [Configuration](#configuration)
-   [Project Structure](#project-structure)
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

See [docs/features.md](docs/features.md) for a comprehensive list of features.

## Building from Source

See [docs/build.md](docs/build.md) for detailed build instructions.


## Installation

See [docs/installation.md](docs/installation.md) for detailed installation instructions.

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
