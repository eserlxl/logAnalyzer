## Building from Source

This section guides you through setting up `LogAnalyzer` from its source code.

### Prerequisites

-   **C++ Compiler**: A compiler with C++23 support (e.g., GCC 13+, Clang 16+).
-   **Build System**: CMake (version 3.14 or higher).
-   **Version Control**: Git for cloning the repository.

#### Installing Dependencies

**On Debian/Ubuntu:**

```bash
sudo apt-get update
sudo apt-get install -y g++-13 cmake git
```

**On macOS (using Homebrew):**

```bash
brew install gcc cmake git
```

**On Arch Linux:**

```bash
sudo pacman -Syu gcc cmake git
```

**On Windows:**

Ensure you have [Visual Studio 2022](https://visualstudio.microsoft.com/) with the "Desktop development with C++" workload installed, along with [CMake](https://cmake.org/download/) and [Git](https://git-scm.com/download/win). You can use the Developer PowerShell for VS to run the build commands.

### Dependencies

`LogAnalyzer` leverages several excellent open-source libraries, which CMake will automatically fetch during the build process:

-   [**CLI11**](https://github.com/CLIUtils/CLI11): A header-only library for robust command-line argument parsing.
-   [**nlohmann/json**](https://github.com/nlohmann/json): A header-only JSON library for C++.
-   [**GoogleTest**](https://github.com/google/googletest): A Google testing and mocking framework for C++ (used for tests).

### Clone the Repository

First, clone the repository and navigate into the project directory:

```bash
git clone https://github.com/eserlxl/logAnalyzer.git
cd logAnalyzer
```

### Build

Next, use CMake to configure and build the project. We recommend an out-of-source build.

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The compiled `logAnalyzer` executable will be available at `build/logAnalyzer`.

#### Build Options

You can customize the build with the following CMake options:

| Option                       | Description                                                     | Default    |
| :--------------------------- | :-------------------------------------------------------------- | :--------- |
| `-DBUILD_TESTING=ON/OFF`     | Enable or disable the compilation of tests.                     | `ON`       |

To use an option, add it to the `cmake` command:

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
```

---

## Running Tests

To ensure everything is working correctly after building:

```bash
ctest --test-dir build --output-on-failure
```

For more comprehensive test instructions, including advanced testing configurations, please see this guide's full documentation.
