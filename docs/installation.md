# Installation Guide

This guide provides detailed instructions for building and installing `logAnalyzer` from source.

## Prerequisites

Before you begin, ensure you have the following dependencies installed on your system:

-   **A C++23 Compatible Compiler**: `logAnalyzer` uses features from the C++23 standard.
    -   GCC 13 or later.
    -   Clang 16 or later.
-   **CMake**: A modern build system generator.
    -   Version 3.14 or later is required.
-   **Git**: The version control system used to clone the repository.

### Installing Dependencies

#### On Debian/Ubuntu

```bash
sudo apt-get update
sudo apt-get install -y build-essential g++-13 cmake git
```

#### On macOS

Using [Homebrew](https://brew.sh/):

```bash
brew install gcc cmake git
```
*Note: On macOS, you may need to set the compiler explicitly when running CMake, as the default Clang version provided by Xcode may not be up-to-date. You can do this by setting the `CC` and `CXX` environment variables, for example: `CC=/usr/local/bin/gcc-13 CXX=/usr/local/bin/g++-13 cmake ...`*

#### On Windows

We recommend using the Windows Subsystem for Linux (WSL) with a distribution like Ubuntu. Once you have WSL set up, you can follow the Debian/Ubuntu instructions above.

## Building from Source

### 1. Clone the Repository

First, clone the `logAnalyzer` repository from GitHub:

```bash
git clone https://github.com/eserlxl/logAnalyzer.git
cd logAnalyzer
```

### 2. Configure the Build

Next, use CMake to generate the build files. It's best practice to create a separate build directory.

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
```

-   `-B build`: Specifies that the build files should be generated in a directory named `build`.
-   `-S .`: Specifies that the source directory is the current directory.
-   `-DCMAKE_BUILD_TYPE=Release`: Configures the build for release, which enables optimizations for the best performance.

### 3. Compile the Project

Now, compile the project using the `cmake --build` command:

```bash
cmake --build build --parallel
```

-   `--build build`: Tells CMake to build the project in the `build` directory.
-   `--parallel`: (Optional) Uses all available CPU cores to speed up the compilation process.

The compiled `logAnalyzer` executable will be located in the `build/bin` directory.

### 4. Run the Executable

You can run `logAnalyzer` directly from the build directory:

```bash
./build/bin/logAnalyzer --version
```

### 5. Install the Executable (Optional)

If you wish to install `logAnalyzer` system-wide, you can use the `cmake --install` command. This will typically copy the executable to `/usr/local/bin`.

```bash
sudo cmake --install build
```

Once installed, you can run `logAnalyzer` from any location:

```bash
logAnalyzer --version
```

## Updating `logAnalyzer`

To update `logAnalyzer` to the latest version, navigate to your cloned repository and run the following commands:

```bash
git pull
cmake --build build --parallel
# If you installed it system-wide:
sudo cmake --install build
```
