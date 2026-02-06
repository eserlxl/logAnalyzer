# Installation Guide

This guide covers the requirements and steps to build and install `logAnalyzer` on various platforms.

## Prerequisites

`logAnalyzer` requires a C++23 compatible compiler and CMake 3.20+.

### Platform-Specific Setup

#### 🐧 Linux (Debian/Ubuntu)

```bash
sudo apt-get update
# GCC 13 is required for C++23 support
sudo apt-get install -y g++-13 cmake git
```

#### 🍎 macOS

Using [Homebrew](https://brew.sh/):

```bash
brew install gcc cmake git
```

#### 🏔 Arch Linux

```bash
sudo pacman -Syu gcc cmake git
```

#### 🪟 Windows

1.  Install [Visual Studio 2022](https://visualstudio.microsoft.com/) with the "Desktop development with C++" workload.
2.  Install [CMake](https://cmake.org/download/).
3.  Install [Git](https://git-scm.com/download/win).

## Building from Source

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/eserlxl/logAnalyzer.git
    cd logAnalyzer
    ```

2.  **Configure and Build:**
    ```bash
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ```

    The executable will be located at `build/bin/logAnalyzer`.

## System-Wide Installation

To install the `logAnalyzer` executable to your system path so it can be run from any directory:

### Unix/Linux/macOS

Run the following command from the project root after building:

```bash
sudo cmake --install build
```

By default, this installs to `/usr/local/bin`. You can specify a different prefix:

```bash
cmake --install build --prefix /home/user/.local
```

### Windows

You may need to run your terminal as Administrator.

```powershell
cmake --install build
```

Alternatively, you can manually add the `build\bin` directory to your System `PATH` environment variable.

## Verification

After installation, verify that `logAnalyzer` is accessible:

```bash
logAnalyzer --version
```
