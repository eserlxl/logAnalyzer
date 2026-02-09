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
| `--logic [AND|OR]`       |           | Specifies the logical operator for combining multiple `--keyword` or `--regex` filters.                                                                                                                                                                                                                                                                             | `AND`     |
| `--case-sensitive`       |           | Makes keyword and regex filtering case-sensitive.                                                                                                                                                                                                                                                                                                                   | `false`   |
| `--level LEVEL`          |           | Includes log entries of a specific level (e.g., `ERROR`, `INFO`). Can be used multiple times to include multiple levels.                                                                                                                                                                                                                                            |           |
| `--min-level LEVEL`      |           | Includes log entries with a level equal to or more severe than the specified level (e.g., `WARNING` will include `WARNING`, `ERROR`, `CRITICAL`).                                                                                                                                                                                                                  |           |
| `--map-level KEY=LEVEL`  |           | Maps a custom log level string found in logs (KEY) to a recognized internal level (LEVEL, e.g., `TRC=TRACE`, `WRN=WARNING`). Can be used multiple times.                                                                                                                                                                                                               |           |
| `--start TIME`           |           | Filters logs appearing after the specified timestamp. Supports absolute, relative, ISO 8601, and Unix timestamp formats.                                                                                                                                                                                                                                            |           |
| `--end TIME`             |           | Filters logs appearing before the specified timestamp. Supports the same formats as `--start`.                                                                                                                                                                                                                                                                      |           |
| `--duration DURATION`    |           | Specifies a time window when used with `--start` or `--end`. Accepts units like `s` (seconds), `m` (minutes), `h` (hours), or `d` (days).                                                                                                                                                                                                                           |           |
| `--expression "EXPR"`    |           | A powerful filter using a logical expression language. Supports fields, nested `and`/`or`/`not` logic, and rich operators like `contains_i` (case-insensitive), `in` (set), `>` (numeric), `startswith`, `is present`, and type casting (e.g., `ip(client_ip)`) for advanced filtering. Example: `(level=ERROR and msg contains_i "database") or not status_code in [200, 304]` |           |


### Sorting

| Option                 | Shorthand | Description                                                                                         | Default     |
| :--------------------- | :-------- | :-------------------------------------------------------------------------------------------------- | :---------- |
| `--sort-by FIELD`      |           | Sorts the output by a specific field. Supported values include `time`/`timestamp`, `level`, `msg`/`message`, `source`/`source_file`, and `thread`/`thread_id`. | `timestamp` |
| `--order ORDER`        |           | Sets the sorting order. Available orders: `asc`/`ascending` and `desc`/`descending`.                | `ascending` |


### Output & Export

| Option                  | Shorthand | Description                                                                                                                             | Default                         |
| :---------------------- | :-------- | :-------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------ |
| `--format [text|json|csv|xml]` |           | Sets the output format for filtered log entries.                                                                                        | `text`                          |
| `--text-format FORMAT_STRING` |           | Custom format string for `text` output. Placeholders: `{timestamp}`, `{level}`, `{message}`, `{id}`, `{sourceFile}`, `{lineNumber}`, `{threadId}`, `{module}`, `{host}`, `{customFields}`. | `{timestamp} {level}: {message}`|
| `--csv-sep CHAR`        |           | Specifies the separator character for `csv` output.                                                                                     | `,`                             |
| `--csv-fields "FIELDS"` |           | Comma-separated list of fields to include in `csv` output (e.g., `timestamp,level,message,file`).                                       | `timestamp,level,message,file`  |
| `--json-fields "FIELDS"`|           | Comma-separated list of fields to include in `json` output. If omitted, all standard fields are included.                               | `(all)`                         |
| `--pretty`              |           | Pretty-prints `json` output with indentation for readability.                                                                           | `false`                         |
| `--include-summary`     |           | Includes a summary section (e.g., total entries) in `json` output.                                                                      | `false`                         |


### Statistics

| Option             | Description                                                                                                       | Default |
| :----------------- | :---------------------------------------------------------------------------------------------------------------- | :------ |
| `--stats NAME`     | Enables a statistic collector. Available: `unique_messages`, `top_messages[:N]`, `entry_rate`, `count_by_level`, `field_value_count`, `top_n_field_values`, or key-value form like `type=TOP_MESSAGES,top_n=5`. Can be used multiple times. |         |
| `--top-n N`        | Sets the number of top items to display for statistics like `top_messages` if not specified directly (e.g., `top_messages:10`). (Deprecated) | `10`    |
| `--stats-window SEC` | Shows log frequency distribution over a time window in seconds. (Deprecated)                                                   |         |
| `--find-gaps MS`   | Detects and reports time gaps in logs longer than the specified milliseconds. (Deprecated)                                     |         |
