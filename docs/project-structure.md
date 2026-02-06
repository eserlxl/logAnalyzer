## Project Structure

Understanding the project's layout can help you navigate the codebase, contribute, or find specific functionalities.

```bash
logAnalyzer/
├── _deps/                   # External dependencies (CLI11, nlohmann/json, GoogleTest) managed by CMake.
├── bin/                     # Compiled LogAnalyzer executable and other binaries.
├── build/                   # CMake build artifacts and temporary files.
├── cmake/                   # Custom CMake modules and scripts.
├── docs/                    # Doxygen configuration and generated documentation.
├── examples/                # Example log files and configuration examples.
├── include/                 # Public header files.
│   ├── analyzer/            # Core analyzer logic (LogReader, LogWriter, AnalyzerCore).
│   ├── config/              # Configuration management and CLI parsing.
│   ├── core/                # Core types and interfaces (LogParser, LogTypes).
│   ├── export/              # Export logic (Exporter).
│   ├── filter/              # Filtering logic and conditions.
│   ├── stats/               # Statistical analysis collectors.
│   └── utils/               # Utility functions (Time, String, IP).
├── lib/                     # Compiled libraries (static/shared).
├── src/                     # Source code implementing headers.
│   ├── analyzer/            
│   ├── config/              
│   ├── core/                
│   ├── export/              
│   ├── filter/              
│   ├── stats/               
│   └── utils/               
├── tests/                   # Unit and integration tests.
├── tools/                   # Development scripts and utilities.
├── .gitignore               # Files/directories ignored by Git.
├── CMakeLists.txt           # Primary CMake build script.
├── CODE_OF_CONDUCT.md       # Guidelines for community behavior.
├── CONTRIBUTING.md          # Contribution guidelines.
├── Doxyfile                 # Doxygen main configuration.
├── LICENSE                  # Project license information.
└── README.md                # Project overview and documentation.
```
