# Installation

To install the `logAnalyzer` executable to a system-wide location (e.g., `/usr/local/bin`), run the following command from your `build` directory. This allows you to run `logAnalyzer` from any terminal.

```bash
# Use sudo for system-wide installation
sudo cmake --install . --prefix /usr/local
```

For a local installation (if you don't have admin privileges), you can specify a different prefix:

```bash
# Install to a 'dist' directory inside the project folder
cmake --install . --prefix ../dist
```

Alternatively, you can add the `build/bin` directory to your system's `PATH` or copy the `logAnalyzer` executable to a directory already in your `PATH`.

```bash
# Add to PATH for the current session (from within the build directory)
export PATH=$(pwd)/bin:$PATH

# Or copy the executable to a user-local bin directory (ensure ~/.local/bin is in your PATH)
cp ./bin/logAnalyzer ~/.local/bin/
```
