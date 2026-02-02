# LogAnalyzer

A simple C++ tool to analyze log files.

## Features
- Parses log files with format: `[timestamp] LEVEL: message`
- Generates a summary of log levels (INFO, WARNING, ERROR, CRITICAL, DEBUG, TRACE, FATAL).
- Ability to filter logs by level.
- Ability to filter logs by message keyword.
- Ability to save analysis summary to an output file.

## Building the project

```bash
mkdir build
cd build
cmake ..
make
```

## Running the project

```bash
./logAnalyzer path/to/your/logfile.log
./logAnalyzer path/to/your/logfile.log --output analysis_summary.txt
./logAnalyzer path/to/your/logfile.log --level ERROR
./logAnalyzer path/to/your/logfile.log --keyword "failed"
./logAnalyzer path/to/your/logfile.log --level WARNING --keyword "memory" --output filtered_warnings.txt
./logAnalyzer path/to/your/logfile.log --level FATAL
./logAnalyzer path/to/your/logfile.log --level CRITICAL
```

## Example Log Format
```
[2023-10-27 10:00:00] INFO: Application started
[2023-10-27 10:01:00] DEBUG: Initializing components
[2023-10-27 10:02:00] WARNING: Low memory
[2023-10-27 10:05:00] ERROR: Connection failed
```
