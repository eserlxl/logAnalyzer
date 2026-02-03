# LogAnalyzer

A simple C++ tool to analyze log files.

## Features

- **Memory-Efficient Processing**: Utilizes a lazy, iterator-based approach to handle very large files with minimal memory usage by default. Operations like sorting or global statistics that require the full dataset will buffer entries in memory.
- Parses **multiple** log files and can merge sorted sources efficiently.
- **Pluggable Architecture**:
    - **Custom Parsers**: Define your own log parsing logic by implementing the `ILogParser` interface, going beyond simple regex.
    - **Custom Analyzers**: Create custom analysis routines with the `ILogAnalyzer` interface.
- **Advanced Filtering**: Build complex filter expressions with `AND`/`OR` logic to pinpoint exact log messages. Basic filtering is still supported for:
    - Specific log levels.
    - Message keyword (case-sensitive or insensitive).
    - Regular expression patterns in messages.
    - Time range (start and end timestamps).
- **Asynchronous Processing**: Load and analyze files asynchronously with support for cancellation.
- **Maps custom log level strings to standard levels (e.g., `FATAL=ERROR`).**
- **Custom Timestamp Formats**: Specify the timestamp format of your log files using `strftime` patterns to ensure correct parsing.
- Generates a summary of log levels (INFO, WARNING, ERROR, DEBUG, UNKNOWN).
- Ability to sort filtered logs by timestamp, level, or message, in ascending or descending order.
- Ability to save analysis results to an output file in various formats (text, JSON, **CSV**).
- **Customizable text output format.**
- Provides statistical analysis (may require buffering data in memory):
    - Counts of unique messages.
    - Top N most frequent messages (default is 10).
    - **Log frequency distribution over a specified time window.**
    - **Average log entry rate (entries/second).**
    - **Identification of time gaps between log entries longer than a specified duration.**
- Supports JSON output, with options for pretty-printing and including a summary.
- Provides detailed error reports on parsing failures, including line numbers.


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

# Specify a custom timestamp format for parsing (e.g., ISO 8601 with milliseconds)
./logAnalyzer path/to/your/logfile.log --timestamp-format "%Y-%m-%d %H:%M:%S.%f"

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
