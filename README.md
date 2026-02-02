# LogAnalyzer

A simple C++ tool to analyze log files.

## Features

- Parses **multiple** log files with a flexible format, including support for milliseconds in timestamps: `[timestamp] LEVEL: message`.
- Supports custom regex patterns for parsing log lines, allowing analysis of diverse log formats.
- **Maps custom log level strings to standard levels (e.g., `FATAL=ERROR`).**
- Generates a summary of log levels (INFO, WARNING, ERROR, DEBUG, UNKNOWN).
- Ability to filter logs by:
    - Specific log levels.
    - Message keyword (case-sensitive or insensitive).
    - Regular expression patterns in messages.
    - Time range (start and end timestamps).
- Ability to sort filtered logs by timestamp, level, or message, in ascending or descending order.
- Ability to save analysis results to an output file in various formats (text, JSON, **CSV**).
- **Customizable text output format.**
- Provides statistical analysis including:
    - Counts of unique messages.
    - Top N most frequent messages (default is 10).
    - **Log frequency distribution over a specified time window.**
    - **Average log entry rate (entries/second).**
    - **Identification of time gaps between log entries longer than a specified duration.**
- Supports JSON output, with options for pretty-printing and including a summary.
- **Streaming mode for processing very large files with low memory usage (incompatible with sorting and global statistics).**


## Building the project

```bash
mkdir build
cd build
cmake ..
make
```

## Running the project

```bash
# Get a summary of a single log file
./logAnalyzer path/to/your/logfile.log

# Analyze multiple log files at once
./logAnalyzer path/to/file1.log path/to/file2.log

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

# Show counts of unique messages
./logAnalyzer path/to/your/logfile.log --unique-messages

# Show the top 5 most frequent log messages
./logAnalyzer path/to/your/logfile.log --top-messages 5

# Show the top 10 (default) most frequent messages
./logAnalyzer path/to/your/logfile.log --top-messages

# Export filtered logs to a pretty-printed JSON file, including a summary
./logAnalyzer path/to/your/logfile.log --level WARNING --format json --pretty --include-summary --output filtered_warnings.json

# Export filtered logs to a CSV file
./logAnalyzer path/to/your/logfile.log --level ERROR --format csv --output errors.csv

# Use a custom text format for the output
./logAnalyzer path/to/your/logfile.log --level INFO --text-format "[{level}] {message}"

# Map a custom log level 'FATAL' to the standard 'ERROR' level for parsing
./logAnalyzer path/to/your/logfile.log --map-level FATAL=ERROR --level ERROR

# Show log frequency distribution in 60-second windows
./logAnalyzer path/to/your/logfile.log --stats-window 60

# Find time gaps in logs longer than 500 milliseconds
./logAnalyzer path/to/your/logfile.log --find-gaps 500

# Show the average log entry rate
./logAnalyzer path/to/your/logfile.log --rate

# Process a very large log file in streaming mode (filtering is supported, sorting is not)
./logAnalyzer path/to/large_logfile.log --stream --level ERROR --keyword "critical"

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
