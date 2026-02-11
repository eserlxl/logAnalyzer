## Project Structure

Understanding the project's layout can help you navigate the codebase, contribute, or find specific functionalities.

```bash
logAnalyzer/
├── _deps/                   # External dependencies (e.g., CLI11, nlohmann/json) managed by CMake's FetchContent.
├── build/                   # Build directory. Contains all compiled artifacts, executables, and libraries.
├── cmake/                   # Helper CMake scripts for build configuration and settings.
├── docs/                    # Project documentation files (Markdown).
├── examples/                # Example usage scripts or code snippets.
├── include/                 # Public header files for the library and application.
│   ├── analyzer/            # Core analysis engine, including log parsing, streaming, and processing logic.
│   ├── config/              # Configuration loading, CLI parsing, and settings management.
│   ├── core/                # Core data structures, error handling, and fundamental types (e.g., LogEntry).
│   ├── export/              # Logic for exporting data to different formats (JSON, CSV, text).
│   ├── filter/              # Filtering engine, including expression parsing and condition evaluation.
│   ├── stats/               # Statistical analysis tools and data collectors.
│   └── utils/               # Common utility functions (time, string manipulation, versioning).
├── src/                     # Source code files implementing the functionality declared in the headers.
│   ├── analyzer/            # Implementation of the analysis engine.
│   ├── config/              # Implementation of configuration and CLI.
│   ├── core/                # Implementation of core data structures.
│   ├── export/              # Implementation of data exporters.
│   ├── filter/              # Implementation of the filtering engine.
│   ├── stats/               # Implementation of statistical collectors.
│   └── utils/               # Implementation of utility functions.
├── tests/                   # Unit and integration tests for the codebase.
├── .clang-format            # Style guide for code formatting with clang-format.
├── .gitignore               # Files and directories ignored by Git.
├── CMakeLists.txt           # Primary CMake build script for the project.
├── CODE_OF_CONDUCT.md       # Guidelines for community behavior and contributions.
├── CONTRIBUTING.md          # Guide for developers who want to contribute to the project.
├── LICENSE                  # Project license information (GPLv3).
└── README.md                # Project overview, quick start, and main documentation entry point.
```
