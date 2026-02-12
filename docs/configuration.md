## Configuration

`LogAnalyzer` can be configured using a JSON file for complex or persistent setups. Use the `--config` option to specify a configuration file. Settings provided via command-line arguments will override the corresponding settings in the file.

The application performs robust validation on startup. If it finds any errors in the configuration file (e.g., a missing required parameter, an invalid value, or a malformed regex), it will print a descriptive error message and exit.

### Example `config.json`

Here is an example demonstrating a more advanced configuration:

```json
{
  "lineParsePattern": "^(\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}.\\d{3}Z) \\s*\\[(\\w+)\\] \\(tid:(\\d+)\\) (.*) \\{\\"session\\": \\"([a-f0-9-]+)\\" \\}",
  "fieldMappings": [
    { "field": "timestamp", "groupIndex": 1 },
    { "field": "level", "groupIndex": 2 },
    { "field": "threadId", "groupIndex": 3 },
    { "field": "message", "groupIndex": 4 },
    { "field": "customFields", "groupIndex": 5, "customFieldKey": "session" }
  ],
  "customLogLevelMappings": {
    "db_trace": "TRACE",
    "warn": "WARNING"
  },
  "filterRules": [
    { "field": "level", "operator": "EQUALS", "value": "ERROR" },
    { "field": "message", "operator": "REGEX_MATCH", "value": "database connection failed" }
  ],
  "exportSettings": {
    "format": "json",
    "outputFile": "error_report.json",
    "fieldsToExport": [
      { "field": "timestamp" },
      { "field": "level" },
      { "field": "message" },
      { "field": "customFields", "customFieldKey": "session" }
    ]
  },
  "statisticConfigs": [
    {
      "type": "TOP_MESSAGES",
      "params": { "top_n": "5" }
    },
    {
      "type": "TOP_N_FIELD_VALUES",
      "params": {
        "target_field": "customFields",
        "custom_field_key": "session",
        "top_n": "10"
      }
    },
    {
      "type": "FIELD_VALUE_COUNT",
      "params": { "target_field": "level" }
    }
  ]
}
```

### Key Configuration Sections

#### `fieldMappings`
An array of objects that map regular expression capture groups from `lineParsePattern` to internal log fields. Each object requires:
-   `field`: The name of the target log field (e.g., `timestamp`, `level`, `message`).
-   `groupIndex`: The 1-based index of the capture group from `lineParsePattern` to map to this field.
-   `customFieldKey` (optional): If `field` is set to `"customFields"`, this specifies a key under which the captured value will be stored within a map of custom fields. This is useful for extracting structured data that doesn't fit standard fields.

**Validation Rules (Current Behavior):**
- `field` must be a non-empty string after trimming whitespace.
- `field: "CUSTOM"` is rejected (reserved token). Use a concrete custom name, e.g. `"session_id"`.
- `groupIndex` must be a non-negative integer or `null`.
- `formats` (if present) must be an array of strings or `null`.
- For `field: "STRUCTURED_FIELD"`, `formats` must contain at least one non-empty regex pattern.
- `customFieldType` is allowed only for custom fields (non-standard field names), and when provided it must be a non-empty string (or `null`).

**Example:**
If `lineParsePattern` extracts a JSON string into `groupIndex 5`, and you want to parse a specific key `session` from that JSON, you would use:
`{ "field": "customFields", "groupIndex": 5, "customFieldKey": "session" }`
This would make the `session` value accessible for filtering and export via `customFields.session`.

#### `customLogLevelMappings`
An object that maps custom log level strings found in your log files to internal log levels recognized by `logAnalyzer`.
- **Keys**: The custom log level strings (e.g., `"VERBOSE"`, `"WRN"`, `"trace_db"`). Leading and trailing whitespace is automatically trimmed.
- **Values**: The target internal log level (e.g., `"DEBUG"`, `"WARNING"`, `"TRACE"`). These are case-insensitive and also trimmed of whitespace.

Supported internal levels are: `TRACE`, `DEBUG`, `INFO`, `WARNING`, `ERROR`, `FATAL`.

#### `logEntryStartPattern` (optional)
A regular expression pattern used to identify the beginning of a new log entry. This is essential for parsing multi-line log entries (e.g., stack traces). If not provided, each line is treated as a separate log entry.

#### `filterRules`
An array of rule objects that define how to filter log entries. Each rule is an object with:
-   `field`: The log entry field to check (e.g., `level`, `message`, `customFields`).
-   `operator`: The comparison operator. Supported operators include:
    -   **Relational**: `EQUALS`, `NOT_EQUALS`, `LESS_THAN`, `GREATER_THAN`, `LESS_THAN_OR_EQUAL`, `GREATER_THAN_OR_EQUAL`.
    -   **String**: `CONTAINS`, `NOT_CONTAINS`, `STARTS_WITH`, `ENDS_WITH`, `REGEX`.
    -   **Case-Insensitive String**: `EQUALS_I`, `NOT_EQUALS_I`, `CONTAINS_I`, `NOT_CONTAINS_I`, `STARTS_WITH_I`, `ENDS_WITH_I`.
    -   **Set-based**: `IN`, `NOT_IN`.
    -   **Presence**: `IS_PRESENT`, `IS_ABSENT`, `IS_NULL`, `IS_NOT_NULL`.
-   `value`: The value to compare against.
-   `customFieldKey` (optional): Required if `field` is `customFields`.

#### `exportSettings`
An object that configures the output format and destination for the filtered logs.

| Property           | Description                                                                                                                                                                                             | Default                         |
| :----------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | :------------------------------ |
| `format`           | The output format. Can be `PLAINTEXT`, `JSON`, `CSV`, or `XML`.                                                                                                                                            | `PLAINTEXT`                     |
| `outputFile`       | Path to a file where output should be written. If not specified, output goes to `stdout`.                                                                                                               | `(stdout)`                      |
| `textFormatString` | A custom format string for `PLAINTEXT` output.                                                                                                                                                          | `{timestamp} {level}: {message}` |
| `useAnsiColors`    | If `true`, enables ANSI color codes in `PLAINTEXT` output for log levels.                                                                                                                               | `false`                         |
| `fieldsToExport`   | An array of field mapping objects for `JSON` and `CSV` export, specifying which fields to include.                                                                                                        | (All fields)                    |
| `includeHeader`    | If `true`, includes a header row in `CSV` output.                                                                                                                                                       | `true`                          |
| `jsonIndent`       | The number of spaces to use for indentation in `JSON` output. A value of `-1` disables pretty-printing.                                                                                                 | `4`                             |
| `separator`        | The character to use as a field separator in `CSV` output.                                                                                                                                              | `,`                             |

**Placeholders for `textFormatString`:**
-   `{timestamp}`: The timestamp of the log entry.
-   `{level}`: The log level (e.g., ERROR, INFO). Will be colorized if `useAnsiColors` is true.
-   `{message}`: The main log message.
-   `{id}`: The unique ID of the log entry, if available.
-   `{sourceFile}`: The source file where the log originated.
-   `{lineNumber}`: The line number in the source file.
-   `{threadId}`: The ID of the thread that generated the log.
-   `{module}`: The module or component name.
-   `{host}`: The hostname where the log was generated.
-   `{customFields}`: A formatted string of all custom fields, e.g., `key1:value1;key2:value2`.

#### `statisticConfigs`
An array of objects to configure which statistics to generate. Each object has a `type` and an optional `params` object.

| Type                 | Description                                       | Required `params`                                                                                                 |
| :------------------- | :------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------- |
| `LOG_LEVEL_COUNT`    | Counts entries for each log level.                | None                                                                                                              |
| `TOP_MESSAGES`       | Finds the most frequently occurring messages.     | `top_n`: A positive integer (e.g., `"5"`).                                                                        |
| `FIELD_VALUE_COUNT`  | Counts unique values for a given field.           | `target_field`: The field to analyze (e.g., `level`). If `customFields`, `custom_field_key` is also required.      |
| `TOP_N_FIELD_VALUES` | Finds the most frequent values for a given field. | `top_n`: A positive integer.<br>`target_field`: The field to analyze. If `customFields`, `custom_field_key` is also required. |
| `ENTRY_RATE`         | Calculates the rate of log entries per second.    | None                                                                                                              |
| `UNIQUE_MESSAGES`    | Counts the number of unique log messages.         | None                                                                                                              |

#### `rootFilterExpression`
Allows defining a single, complex filter expression tree using nested `AND`, `OR`, and `NOT` logic. This serves as an alternative or addition to the simpler `filterRules` list.

**Structure:**
The expression object can represent a **Condition**, a **Logical Operation**, or be **Negated**.

- **Condition:**
  ```json
  {
    "condition": {
      "field": "LEVEL",
      "op": "EQUALS",
      "value": "ERROR"
    }
  }
  ```
- **Logical Operation:**
  ```json
  {
    "operator": "OR",
    "operands": [
      { "condition": { ... } },
      { "condition": { ... } }
    ]
  }
  ```
- **Negation:**
  Any expression can be negated by adding `"negated": true`.

**Example:**
```json
"rootFilterExpression": {
  "operator": "OR",
  "operands": [
    {
      "condition": { "field": "LEVEL", "op": "EQUALS", "value": "FATAL" }
    },
    {
      "operator": "AND",
      "operands": [
        { "condition": { "field": "MESSAGE", "op": "CONTAINS", "value": "database" } },
        { "condition": { "field": "LEVEL", "op": "EQUALS", "value": "ERROR" } }
      ]
    }
  ]
}
```


### Advanced Configuration Features

#### Environment Variable Expansion

You can use environment variables in your configuration file using the syntax `${VAR}` or `$VAR`. They will be expanded when the file is loaded.

**Example:**
```json
{
  "exportSettings": {
    "outputFile": "${HOME}/analysis_results.json"
  }
}
```

#### Configuration Includes

You can split your configuration into multiple files using the `includes` key. This allows you to share common settings across different configurations. The paths can be relative to the main configuration file or absolute.

**Example `config.json`:**
```json
{
  "includes": [
    "common_filters.json",
    "output_settings.json"
  ],
  "lineParsePattern": "..."
}
```

**Note:** Properties in the main file override those in included files. Later includes override earlier ones if there are conflicts.

#### Configuration Merging Strategy

`logAnalyzer` uses a hierarchical merging strategy to combine configuration settings from different sources, ensuring flexibility and predictable behavior. The order of precedence, from highest to lowest, is:

1.  **Command-line Arguments**: Settings provided directly via CLI flags always take precedence.
2.  **Main Configuration File**: Settings defined in the `--config` file override any settings from included files.
3.  **Included Configuration Files**: Files specified in the `includes` array are processed in order. Settings from later included files will override those from earlier included files if there are conflicts.

The specific merging behavior depends on the type of setting:

*   **Scalar and Optional Values** (`lineParsePattern`, `caseSensitiveParsing`, `maxMultilineBufferSize`, `parserErrorAction`, `logEntryStartPattern`, `rootFilterExpression`): These settings are directly **overwritten** by the higher-priority source if a value is explicitly provided. If a higher-priority source does not specify a value, the lower-priority value (or the default) is retained.

*   **Mapped Collections** (`fieldMappings`, `customLogLevelMappings`): For these collections, `logAnalyzer` performs an **upsert** operation. If an entry with the same identifier (e.g., the `field` and its specific type for `fieldMappings`, or the key for `customLogLevelMappings`) already exists from a lower-priority source, it will be updated with the values from the higher-priority source. New entries from the higher-priority source are simply added to the collection.

*   **List Collections** (`filterRules`, `statisticConfigs`): Entries from higher-priority sources are **appended** to the existing list of entries from lower-priority sources. This means rules and statistics configurations are additive.

*   **Nested Configuration Objects** (`exportSettings`): Nested configuration objects are merged **recursively** using the same set of merging rules. This allows for fine-grained control over sub-sections of the configuration.
