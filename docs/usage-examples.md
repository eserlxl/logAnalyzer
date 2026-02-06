## Usage Examples

After building, you can run `logAnalyzer` in two ways:

1.  **From the build directory**:
    ```bash
    ./build/bin/logAnalyzer [options] <log_file(s)>
    ```
2.  **As an installed command**:
    ```bash
    logAnalyzer [options] <log_file(s)>
    ```

**Note**: The following examples assume `logAnalyzer` is in your `PATH`.

### Quick Start Examples (from README)

Here are some basic examples to get you started quickly:

- **Display help:**
  ```bash
  logAnalyzer --help
  ```

- **Process a log file from `stdin`:**
  ```bash
  echo "INFO 2023-10-27 12:30:00 This is a test log message." | logAnalyzer
  ```

- **Analyze a specific log file:**
  ```bash
  logAnalyzer /var/log/syslog
  ```

- **Use a complex expression to find database errors OR any message containing "timeout":**
  ```bash
  logAnalyzer app.log --expression '(level=ERROR and msg contains "database") or msg contains "timeout"'
  ```

- **Generate statistics on the top 5 most common error messages:**
  ```bash
  logAnalyzer system.log --level ERROR --stats "type=TOP_MESSAGES,top_n=5"
  ```

- **Export errors from the last 2 hours to a JSON file:**
  ```bash
  logAnalyzer app.log --start "2h ago" --level ERROR --format json --pretty --output errors.json
  ```

### Example 1: Basic Filtering

```bash
# Find all errors containing the word "database" in a specific log file
logAnalyzer /var/log/app.log --level ERROR --keyword "database"

# Find all entries in two different log files, excluding those containing "DEBUG"
logAnalyzer app.log kern.log --exclude-keyword "DEBUG"
```

### Example 2: Advanced Filtering and Output

```bash
# Find entries that are either warnings or errors, and contain "timeout" OR "refused"
logAnalyzer access.log --level WARNING --level ERROR --keyword "timeout" --keyword "refused" --logic OR

# Export errors between two dates to a pretty-printed JSON file
logAnalyzer system.log --level ERROR --start "2023-11-01 00:00:00" --end "2023-11-02 00:00:00" --format json --pretty --output errors.json
```

### Example 3: Complex Expression

```bash
# Use a complex expression to find database errors or any message containing "timeout"
logAnalyzer app.log --expression '(level=ERROR and msg contains "database") or msg contains "timeout"'
```

```bash
# Use a complex expression to find entries that are not at the DEBUG level and contain "session"
logAnalyzer app.log --expression 'not level=DEBUG and msg contains "session"'
```

### Example 4: Stream a Large File

```bash
# Process a large log file without loading it all into memory, saving errors to a file
logAnalyzer large_log.log --stream --level ERROR --output filtered_errors.txt
```

**Note:** When using `--stream`, features that require full log data (like sorting or certain statistics) are not available.

### Example 5: Process Logs from Standard Input

```bash
# Pipe logs from another command and filter for errors
cat /var/log/syslog | logAnalyzer - --level ERROR

# Tail a file and filter for a keyword
tail -f /var/log/app.log | logAnalyzer --stdin --keyword "error"
```

### Example 6: Statistical Analysis

```bash
# Get the top 5 most common error messages from a log file (using legacy syntax)
logAnalyzer system.log --level ERROR --stats top_messages:5

# Get the top 10 messages using the new, more flexible syntax
logAnalyzer system.log --stats "type=TOP_MESSAGES,top_n=10"
```

### Example 7: Custom Export

```bash
# Export specific fields to a CSV, with a custom header for the timestamp field
logAnalyzer application.log --format csv --csv-fields "timestamp as Time, level, message" --output report.csv
```


### Example 8: Export to XML

```bash
# Export filtered errors to an XML file
logAnalyzer production.log --level ERROR --format xml --output errors.xml

# Export all messages from a specific day to a pretty-printed XML file
logAnalyzer server.log --start "today" --format xml --pretty --output today_logs.xml
```

### Advanced Expression Examples

The `--expression` flag provides access to a powerful filtering engine. Here are some examples of advanced usage:

- **Check for presence of a field:**
  ```bash
  # Find all logs that have a 'user.id' field in their custom JSON data
  logAnalyzer app.json.log --expression 'custom.user.id is present'
  ```

- **Filter using a set of values:**
  ```bash
  # Find logs where the request status is one of several error codes
  logAnalyzer api.log --expression 'status in ["500", "502", "503"]'
  ```

- **Compare Semantic Versions:**
  ```bash
  # Find logs from application versions older than 2.1.0
  logAnalyzer deployment.log --expression 'version < "2.1.0"'
  ```

- **Filter by IP Address ranges:**
  ```bash
  # Find logs from a specific internal IP subnet
  logAnalyzer firewall.log --expression 'src_ip >= "192.168.1.1" and src_ip <= "192.168.1.255"'
  ```

- **Case-insensitive search:**
  ```bash
  # Find all "error" or "failure" messages, ignoring case
  logAnalyzer app.log --expression 'msg contains_i "error" or msg contains_i "failure"'
  ```

- **Using Negation:**
  ```bash
  # Find all logs that are not from the 'healthcheck' module and do not contain 'noise'
  logAnalyzer app.log --expression 'not (module = "healthcheck" or msg contains "noise")'
  ```

### Time-based Filtering

`logAnalyzer` offers flexible options for filtering log entries based on their timestamps using the `--start` and `--end` flags.

You can specify timestamps in several formats:
*   **Absolute Time**: `"YYYY-MM-DD HH:MM:SS"` (e.g., `"2023-11-20 14:30:00"`)
*   **Relative Time**: Keywords like `yesterday`, `today`, or offsets like `"1h ago"`, `"30m ago"`, `"2h ago"`, or `3d` (days).
*   **ISO 8601 Format**: `YYYY-MM-DDTHH:MM:SSZ` or `YYYY-MM-DDTHH:MM:SS+HH:MM`.
*   **Unix Timestamp**: Seconds since the Unix epoch.

#### Using `--duration`

The `--duration` flag can be combined with `--start` or `--end` to specify a time window. It accepts durations like `10s` (seconds), `5m` (minutes), `2h` (hours), or `3d` (days).

#### Examples

```bash
# Get logs from the last 2 hours
logAnalyzer app.log --start "2h ago"

# Get logs from yesterday
logAnalyzer app.log --start "yesterday" --end "today"

# Get logs for a 30-minute window starting at a specific time
logAnalyzer app.log --start "2023-11-20 10:00:00" --duration "30m"

# Get logs from a specific day (using ISO 8601 date)
logAnalyzer app.log --start "2023-11-20T00:00:00Z" --end "2023-11-21T00:00:00Z"

# Filter by specific time range
logAnalyzer app.log --start "2023-10-27 10:00:00" --end "2023-10-27 11:00:00"
```

### Combining Filters

Combine multiple filter criteria to narrow down your search results.

```bash
# Combine filters (e.g., critical errors with specific message content):
logAnalyzer server.log --level CRITICAL --expression 'msg contains "failed to connect"'
```
