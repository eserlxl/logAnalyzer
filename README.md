# logAnalyzer

A C++ log analysis tool designed for parsing, filtering, statistically analyzing, and exporting log data in various formats. It supports flexible configuration, diverse log parsing strategies (e.g., JSON, regex), advanced filtering capabilities, and multiple output options (CSV, JSON, Text, XML).

## Features

*   **Flexible Log Parsing**: Supports parsing logs using defined JSON structures or regular expressions.
*   **Advanced Filtering**: Filter log entries based on complex conditions and expressions.
*   **Statistical Analysis**: Perform various statistical computations on parsed log data.
*   **Multiple Export Formats**: Output analyzed data to CSV, JSON, Text, or XML.
*   **Command-Line Configuration**: Configure behavior via command-line interface.

## Building the Project

This project uses CMake for its build system.

### Prerequisites

*   A C++ compiler (e.g., GCC, Clang, MSVC)
*   CMake (version 3.15 or higher recommended)

### Build Steps

1.  **Clone the repository (if applicable):**
    ```bash
    git clone <repository_url>
    cd logAnalyzer
    ```
2.  **Create a build directory:**
    ```bash
    mkdir build
    cd build
    ```
3.  **Configure the project with CMake:**
    ```bash
    cmake ..
    ```
    (On Windows with Visual Studio, you might need to specify a generator, e.g., `cmake .. -G "Visual Studio 17 2022"`)
4.  **Build the project:**
    ```bash
    cmake --build . --config Release
    ```
    (Or simply `make` on Unix-like systems after `cmake ..`)

## Running the Analyzer

After a successful build, the executable `logAnalyzer` will be located in your build directory (e.g., `build/logAnalyzer` or `build/Release/logAnalyzer` on Windows).

You can run it from the build directory:

```bash
./logAnalyzer --help
```

For detailed usage, refer to the command-line help output.

## License

This project is licensed under the GNU General Public License v3.0. See the [LICENSE](LICENSE) file for details.