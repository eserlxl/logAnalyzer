# LogAnalyzer

A simple C++ tool to analyze log files.

## Features
- Parses log files with a flexible format, including support for milliseconds in timestamps: `[timestamp] LEVEL: message`.
- Supports custom regex patterns for parsing log lines, allowing analysis of diverse log formats.
- Generates a summary of log levels (INFO, WARNING, ERROR, DEBUG, UNKNOWN).
- Ability to filter logs by:
    - Specific log levels.
    - Message keyword (case-sensitive or insensitive).
    - Regular expression patterns in messages.
    - Time range (start and end timestamps).
- Ability to sort filtered logs by timestamp, level, or message, in ascending or descending order.
- Ability to save analysis results to an output file in various formats (text, JSON).
- Provides statistical analysis including:
    - Counts of unique messages.
    - Top N most frequent messages.
- Supports JSON output, with options for pretty-printing and including a summary.


## Building the project

```bash
mkdir build
cd build
cmake ..
make
```

## Running the project

```bash
# Get a summary of the log file
./logAnalyzer path/to/your/logfile.log

# Save a summary to a file
./logAnalyzer path/to/your/logfile.log --output analysis_summary.txt

# Filter for specific log levels
./logAnalyzer path/to/your/logfile.log --level ERROR,WARNING

# Filter by a keyword (case-insensitive by default)
./logAnalyzer path/to/your/logfile.log --keyword "failed"

# Filter by a keyword with case-sensitivity
./logAnalyzer path/to/your/logfile.log --keyword "Failed" --case-sensitive

# Filter by a regular expression
./logAnalyzer path/to/your/logfile.log --regex "Connection timed out|refused"

# Filter by a time range
./logAnalyzer path/to/your/logfile.log --start "2023-10-27 10:00:00" --end "2023-10-27 10:05:00"

# Sort filtered results by log level (asc) and message (desc)
./logAnalyzer path/to/your/logfile.log --level ERROR --sort-by level --order asc
./logAnalyzer path/to/your/logfile.log --level ERROR --sort-by msg --order desc

# Show the top 5 most frequent log messages
./logAnalyzer path/to/your/logfile.log --top-messages 5

# Export filtered logs to a pretty-printed JSON file, including a summary
./logAnalyzer path/to/your/logfile.log --level WARNING --keyword "memory" --format json --pretty --include-summary --output filtered_warnings.json

# Complex example: Filter for errors containing 'database', sort them by time, and save to a file
./logAnalyzer path/to/your/logfile.log --level ERROR --keyword "database" --sort-by time --order desc --output db_errors.log
```

## Example Log Format
```
[2023-10-27 10:00:00.123] INFO: Application started
[2023-10-27 10:01:00.456] DEBUG: Initializing components
[2023-10-27 10:02:00.789] WARNING: Low memory
[2023-10-27 10:05:00.000] ERROR: Connection failed
```
