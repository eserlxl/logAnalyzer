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

**Example:**
If `lineParsePattern` extracts a JSON string into `groupIndex 5`, and you want to parse a specific key `session` from that JSON, you would use:
`{ "field": "customFields", "groupIndex": 5, "customFieldKey": "session" }`
This would make the `session` value accessible for filtering and export via `customFields.session`.

#### `filterRules`
An array of rule objects that define how to filter log entries. Each rule is an object with:
-   `field`: The log entry field to check (e.g., `level`, `message`, `customFields`).
-   `operator`: The comparison operator (e.g., `EQUALS`, `CONTAINS`, `REGEX_MATCH`, `GREATER_THAN`).
-   `value`: The value to compare against.
-   `customFieldKey` (optional): Required if `field` is `customFields`.

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
