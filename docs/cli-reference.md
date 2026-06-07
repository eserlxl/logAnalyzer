## Command-Line Interface (CLI)

`logAnalyzer` provides a rich command-line interface for ad-hoc analysis.

Run `logAnalyzer --help` for a full list of commands.

### General Options

| Option                 | Shorthand | Description                                                                                                                                                                             | Default    |
| :--------------------- | :-------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :--------- |
| `--help`               | `-h`      | Displays the help message and exits.                                                                                                                                                    |            |
| `--version`            |           | Displays the version information and exits.                                                                                                                                             |            |
| `--config [FILE]`      |           | Specifies a configuration file to load. Command-line arguments will override settings defined in the file.                                                                                  |            |
| `--output FILE`        |           | Redirects all output (filtered logs, statistics) to the specified file instead of standard output.                                                                                      | `(stdout)` |
| `--color OPT`          |           | Controls colorized output. Options are `always`, `auto` (default, colors if stdout is a TTY and not redirected), or `never`.                                                              | `auto`     |
| `--stream`             |           | Enables memory-efficient stream processing for very large files, avoiding full memory load. **Caution:** Some features (e.g., sorting) are incompatible with stream mode.           | `false`    |
| `--stdin` |           | Reads log entries from standard input (e.g., from a pipe). This mode is automatically enabled if `-` is used as a log file path. See Example 5 for details.                                           | `false`    |


### Parsing

| Option                          | Description                                                                                                          | Default   |
| :------------------------------ | :------------------------------------------------------------------------------------------------------------------- | :-------- |
| `--pattern REGEX`               | Overrides the log line parsing regular expression defined in the configuration.                                      | (builtin) |
| `--multiline-start-pattern REGEX` | Regex to identify the start of a multi-line log entry. For example, `^[\[]\d{4}-\d{2}-\d{2}` to match a timestamp at the start of a new log entry.                                       |           |
| `--max-multiline-buffer SIZE`   | Max buffer size for multi-line entries. Supports units like `10MB`, `50KB`, or raw bytes (e.g., `1048576`).           | `10MB`    |
| `--field-map MAPPING`           | Map regex capture group to a field. Format: `INDEX=FIELD[:FORMAT]`. If `FIELD` is a standard field (e.g., `timestamp`), it maps to that property. If `FIELD` is unknown, it is treated as a custom field name and stored in `customFields`. Example: `1=timestamp:%Y-%m-%d`, `2=request_id`. |           |
| `--on-parse-error OPT`          | Action on parse error. Options are `skip`/`ignore`, `log`/`warn`, or `fail`/`throw`.                                     | `warn`    |


### Monitoring

| Option             | Description                                                   | Default |
| :----------------- | :------------------------------------------------------------ | :------ |
| `--tail`           | Enable tail mode to monitor files for new lines.              | `false` |
| `--tail-interval MS`| Polling interval for tail mode in milliseconds.               | `1000`  |


### Filtering

| Option                   | Shorthand | Description                                                                                                                                                                                                                                                                                                                                                         | Default   |
| :----------------------- | :-------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | :-------- |
| `--keyword TEXT`         |           | Filters log messages containing this keyword or phrase. Can be used multiple times, combined by `--logic`.                                                                                                                                                                                                                                                          |           |
| `--exclude-keyword TEXT` |           | Excludes log messages containing this keyword or phrase. Can be used multiple times.                                                                                                                                                                                                                                                                                |           |
| `--regex PATTERN`        |           | Filters log messages matching this regular expression. Can be used multiple times, combined by `--logic`.                                                                                                                                                                                                                                                           |           |
| `--exclude-regex PATTERN`|           | Excludes log messages matching this regular expression. Can be used multiple times.                                                                                                                                                                                                                                                                                 |           |
| `--logic [AND|OR]`       |           | Specifies the logical operator for combining multiple `--keyword` or `--regex` filters. Case-insensitive.                                                                                                                                                                                                                                                           | `AND`     |
| `--case-sensitive`       |           | Makes keyword and regex filtering case-sensitive.                                                                                                                                                                                                                                                                                                                   | `false`   |
| `--level LEVEL`          |           | Includes log entries of a specific level (e.g., `ERROR`, `INFO`). Can be used multiple times to include multiple levels. Case-insensitive.                                                                                                                                                                                                                         |           |
| `--min-level LEVEL`      |           | Includes log entries with a level equal to or more severe than the specified level (e.g., `WARNING` will include `WARNING`, `ERROR`, `CRITICAL`). Case-insensitive.                                                                                                                                                                                                |           |
| `--max-level LEVEL`      |           | Includes log entries with a level equal to or less severe than the specified level (e.g., `WARNING` will include `TRACE`, `DEBUG`, `INFO`, `WARNING`). Combine with `--min-level` for range filtering (e.g., `--min-level INFO --max-level WARNING`). Case-insensitive.                                                                                             |           |
| `--map-level KEY=LEVEL`  |           | Maps a custom log level string found in logs (KEY) to a recognized internal level (LEVEL, e.g., `TRC=TRACE`, `WRN=WARNING`). Can be used multiple times. Whitespace around `KEY` and `LEVEL` is automatically trimmed. `LEVEL` is case-insensitive.                                                                                                                |           |
| `--start TIME`           |           | Filters logs appearing after the specified timestamp. Supports absolute, relative, ISO 8601, and Unix timestamp formats.                                                                                                                                                                                                                                            |           |
| `--end TIME`             |           | Filters logs appearing before the specified timestamp. Supports the same formats as `--start`.                                                                                                                                                                                                                                                                      |           |
| `--since DURATION`       |           | Shorthand for `--start`: shows only entries from the last DURATION (e.g. `1h`, `30m`, `2d`). Cannot be combined with `--start`.                                                                                                                                                                                                                                    |           |
| `--duration DURATION`    |           | Specifies a time window when used with `--start` or `--end`. Accepts units like `s` (seconds), `m` (minutes), `h` (hours), or `d` (days).                                                                                                                                                                                                                           |           |
| `--expression "EXPR"`    |           | A powerful filter using a logical expression language. Supports fields, nested `and`/`or`/`not` logic, and rich operators like `contains_i` (case-insensitive), `in` (set), `>` (numeric), `startswith`, `is present`, and type casting (e.g., `ip(client_ip)`) for advanced filtering. Example: `(level=ERROR and msg contains_i "database") or not status_code in [200, 304]` |           |

#### Expression Filter Reference

**Fields:** `level`, `message` (alias: `msg`), `source` (alias: `source_file`), `timestamp`, `line_number`, `thread_id`, `module`, `host`. Any other name is treated as a custom field key.

**Operators:**

| Category | Operators | Notes |
|----------|-----------|-------|
| Equality | `=`, `!=` | Case-sensitive by default |
| Case-insensitive | `=_i`, `!=_i`, `contains_i`, `not contains_i`, `startswith_i`, `endswith_i` | Suffix `_i` means case-insensitive |
| String | `contains`, `not contains`, `startswith`, `endswith`, `matches` | `matches` uses regex |
| Numeric / relational | `>`, `<`, `>=`, `<=`, `==`, `!=` | Field value is parsed as a number |
| Set | `in [v1, v2, ...]`, `not in [v1, v2, ...]` | Bracket syntax |
| Presence | `is present`, `is absent` | Field must / must not exist in the entry |

**Type casts (wrap the field name):** `ip(FIELD)` — compare as IP address; `version(FIELD)` — compare as semantic version.

**Logic and precedence:** `not` > `and` > `or`. Use parentheses to override: `(A or B) and C`.

**Examples:**
```
level = ERROR and message contains_i "timeout"
(level in [WARNING, ERROR]) and source startswith "db/"
not status_code in [200, 204, 304]
version(app_version) >= 2.1.0
ip(client_ip) = 10.0.0.1
```


### Sorting

| Option                 | Shorthand | Description                                                                                         | Default     |
| :--------------------- | :-------- | :-------------------------------------------------------------------------------------------------- | :---------- |
| `--sort-by FIELD`      |           | Sorts the output by a specific field. Supported values: `time`/`timestamp`, `level`, `msg`/`message`, `source`/`source_file`, `thread`/`thread_id`, `module`, `host`, `id`, and `line`/`line_number`. Entries missing an optional field (`module`, `host`, `id`, `line_number`, `thread_id`) sort first in ascending order; `id` and `line_number` sort numerically. | `timestamp` |
| `--order ORDER`        |           | Sets the sorting order. Available orders: `asc`/`ascending` and `desc`/`descending`.                | `ascending` |


### Output & Export

| Option                  | Shorthand | Description                                                                                                                             | Default                         |
| :---------------------- | :-------- | :-------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------ |
| `--format [text|json|ndjson|csv|xml]` |      | Sets the output format for filtered log entries. `ndjson` (newline-delimited JSON) emits one JSON object per line — also supported in `--stream` mode for tool-chain-friendly output (e.g., `jq`, Elasticsearch). | `text`             |
| `--text-format FORMAT_STRING` |           | Custom format string for `text` output. Placeholders: `{timestamp}`, `{level}`, `{message}`, `{id}`, `{sourceFile}`, `{lineNumber}`, `{threadId}`, `{module}`, `{host}`, `{customFields}`. | `{timestamp} {level}: {message}`|
| `--csv-sep CHAR`        |           | Specifies the separator character for `csv` output.                                                                                     | `,`                             |
| `--csv-fields "FIELDS"` |           | Comma-separated list of fields to include in `csv` output (e.g., `timestamp,level,message,file`).                                       | `timestamp,level,message,file`  |
| `--json-fields "FIELDS"`|           | Comma-separated list of fields to include in `json` output. If omitted, all standard fields are included.                               | `(all)`                         |
| `--pretty`              |           | Pretty-prints `json` output with indentation for readability.                                                                           | `false`                         |
| `--include-summary`     |           | Includes a summary section (e.g., total entries) in `json` output.                                                                      | `false`                         |
| `--limit N`             |           | Stops output after N matching entries. Applies to both batch and stream mode.                                                           |                                 |
| `--offset N`            |           | Skips the first N matching entries before output begins. Combine with `--limit` for pagination (e.g., `--offset 200 --limit 100`).       | `0`                             |
| `--count`               |           | Prints only the count of matching entries and exits (like `grep -c`). No per-entry output. Works in both batch and stream mode.         | `false`                         |
| `--dedup-field FIELD`   |           | Keeps only the first entry per unique value of FIELD. Standard fields: `level`, `message`, `source` (alias: `source_file`), `id`, `lineNumber` (alias: `line_number`, `line`), `threadId` (alias: `thread_id`, `tid`), `module`, `host`, `timestamp` (alias: `time`). Any other name is treated as a custom field key. Entries where an optional standard field or custom field is absent are each treated as unique. Applies before `--offset`/`--limit`. | |
| `--dedup-keep-last`     |           | With `--dedup-field`, keep the **last** entry per unique value instead of the first (e.g. the latest event per `session_id`), preserving original order. Batch mode only; ignored with a warning under `--stream` (keeping the last requires buffering all entries). | `false` |


### Statistics

| Option             | Description                                                                                                       | Default |
| :----------------- | :---------------------------------------------------------------------------------------------------------------- | :------ |
| `--stats NAME`     | Enables a statistic collector. Can be used multiple times. Available names and shorthands: `unique_messages`, `top_messages[:N]`, `entry_rate`, `count_by_level`, `field_value_count`, `top_n_field_values`, `time_bucket_histogram[:BUCKET_SECONDS]`, `percentile_stats[:FIELD]` (FIELD may be a standard numeric field such as `lineNumber` or `id`, or any custom field key; reports `count`, `min`, `max`, `mean`, and P50/P95/P99 by default — supply a custom percentile set via the key-value form `type=PERCENTILE_STATS,field=FIELD,percentiles=50;90;99.9`, a semicolon-separated list of numbers in `(0, 100]`), `moving_average_rate[:BUCKET_SECONDS]`, `find_gaps[:THRESHOLD_MS]`. Also accepts key-value form: `type=TOP_MESSAGES,top_n=5`. Works in both batch and `--stream` mode. |         |
| `--stats-output PATH` |         | Writes the statistics JSON report to PATH instead of stderr. When omitted, statistics are written to stderr (both batch and stream mode), so log entry output can be piped without interleaving. Stats are written even if no entries match. |  |
| `--stats-interval N` |         | In `--stream` mode, emit an intermediate statistics report to stderr every N matching entries (N must be > 0). Has no effect without `--stream`. |  |
| `--top-n N`        | Sets the number of top items to display for statistics like `top_messages` if not specified directly (e.g., `top_messages:10`). (Deprecated) | `10`    |
| `--stats-window SEC` | Shows log frequency distribution over a time window in seconds. (Deprecated)                                                   |         |
| `--find-gaps MS`   | Detects and reports time gaps in logs longer than MS milliseconds. Creates a `gap_detector` statistic collector reporting each gap's start, end, and duration. Equivalent to `--stats find_gaps:MS`. |         |
