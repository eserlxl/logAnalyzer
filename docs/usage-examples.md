## Usage Examples

After building, you can run `logAnalyzer` in two ways:

1.  **From the build directory**:
    ```bash
    ./build/logAnalyzer [options] <log_file(s)>
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

**Note:** When using `--stream`, features that require buffering all entries (like `--sort-by`) are not available. Statistics and deduplication work normally in stream mode.

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

# Compute P50/P95/P99 percentiles for the response_time field across all errors
logAnalyzer api.log --level ERROR --stats "percentile_stats:response_time"

# Compute a moving average rate of log entries per 30-second bucket
logAnalyzer app.log --stats "moving_average_rate:30"

# Stream a large file and emit a percentile report every 500 matching entries
logAnalyzer app.log --stream --stats "percentile_stats:response_time" --stats-interval 500

# Stream and emit a moving average rate report every 1000 entries (60s buckets)
logAnalyzer app.log --stream --stats "moving_average_rate:60" --stats-interval 1000

# Count how many times each log level appears across all entries
logAnalyzer app.log --stats count_by_level

# Get a breakdown of source files with their entry counts
logAnalyzer app.log --stats "type=field_value_count,target_field=source"

# Find the top 5 source files generating the most ERROR entries
logAnalyzer app.log --level ERROR --stats "type=top_n_field_values,target_field=source,top_n=5"

# Detect time gaps in the log stream longer than 5 seconds
logAnalyzer app.log --stats "find_gaps:5000"
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

# Export all messages from a specific day to an XML file
logAnalyzer server.log --start "today" --format xml --output today_logs.xml
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

#### Using `--since`

The `--since` flag is a shorthand for entries from the last N — equivalent to `--start 'N ago'`. It accepts the same duration formats as `--duration`.

```bash
# Get entries from the last hour
logAnalyzer app.log --since 1h

# Get errors from the last 30 minutes
logAnalyzer app.log --since 30m --level ERROR
```

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

### CLI Advanced Filtering and Parsing

These examples demonstrate more advanced command-line filtering and parsing capabilities.

- **Expression filter**:
  Utilize a powerful expression language for complex conditional filtering.
  ```bash
  logAnalyzer app.log --expression "level >= WARNING AND message contains 'timeout'"
  ```
- **JSON filter expression workflows (C++ API):**
  For C++ developers, you can build, serialize, and deserialize nested filter-expression trees via the `filter::FilterExpression` JSON APIs.
  (See tests under `tests/filter/core_json/*` and the [API Reference](docs/api-reference.md) for details).
- **Multi-line entry parsing**:
  Handle log entries that span multiple lines by defining patterns for start and end, and buffering settings.
  ```bash
  logAnalyzer app.log \
    --pattern "^(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}) (\\w+): ([\\s\\S]*)$" \
    --multiline-start-pattern "^\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2} \\w+:" \
    --max-multiline-buffer 10MB
  ```
- **Parse error behavior**:
  Control how `logAnalyzer` reacts to unparseable log lines.
  ```bash
  logAnalyzer app.log --on-parse-error warn
  ```

### Example 9: Counting and Deduplication

- **Count matching entries (like `grep -c`):**
  Prints only the number of matching entries and exits. No per-entry output is produced.
  ```bash
  logAnalyzer app.log --level ERROR --count
  ```

- **Count entries from the last hour:**
  ```bash
  logAnalyzer app.log --since 1h --count
  ```

- **Count matching entries AND write statistics to a file:**
  Because `--count` exits before exporting entries, combining it with `--stats-output` lets you
  capture the match count on stdout while saving statistics to a separate file.
  ```bash
  logAnalyzer app.log --level ERROR --count --stats "count_by_level" --stats-output stats.json
  ```

- **Deduplicate by message — keep only the first occurrence of each unique message:**
  ```bash
  logAnalyzer app.log --dedup-field message
  ```

- **Keep only the first error per source file:**
  Combine `--level` with `--dedup-field source` to surface one representative error per module.
  ```bash
  logAnalyzer app.log --level ERROR --dedup-field source
  ```

- **Deduplicate by thread — keep only the first entry per thread ID:**
  ```bash
  logAnalyzer app.log --dedup-field threadId
  ```