# Features

| Feature                      | Description                                                                                             |
| ---------------------------- | ------------------------------------------------------------------------------------------------------- |
| **Memory-Efficient Processing** | Handles massive files with minimal memory usage using the `--stream` mode.                              |
| **Multi-File Support**       | Parses and analyzes multiple log files in a single run.                                                 |
| **Sorting**                  | Sort results by timestamp, log level, message, or other fields in ascending or descending order.        |
| **Structured FieldParsing** | Automatically parses log messages into fields using custom patterns (regex) or by natively parsing full JSON log lines. |
| **Keyword & Regex Filtering**| Filter by log level, keywords, and case-sensitive/insensitive regular expressions. |
| **Field-Value Matching**     | Match field values with case-sensitive/insensitive text, regex, and glob patterns.                      |
| **Field Filtering**          | Target fields within structured data (e.g., specific JSON keys).                                         |
| **Advanced Data Types**      | Compare fields as `version` numbers (semantic versioning) or `IP addresses`.                            |
| **Numeric & Bool Filtering** | Perform numeric (`>`, `<`, `==`) or boolean (`true`, `false`) comparisons on flat and nested fields.     |
| **Set-Based Filtering**      | Check if a field's value is `in` or `not in` a specific set of values.                                   |
| **Time-based Filtering**     | Filter by absolute time range, relative time (`5m ago`), or for a specific day (`yesterday`, `2023-10-20`). |
| **Field Presence Checks**    | Filter for logs where a specific field `is present` or `is absent`.                                       |
| **Automatic Type Inference** | Automatically infers data types (Integer, Double, Boolean, IP, Version) in filter expressions, simplifying queries. |
| **Complex Filter Expressions** | Build sophisticated filter logic using parenthesized, nested `AND`/`OR`/`NOT` conditions.                     |
| **Filter JSON Roundtrip**    | Serialize/deserialize `filter::FilterExpression` trees to/from JSON for reusable configs and API-driven workflows. |
| **Flexible Export**          | Save results in Text, JSON, NDJSON (newline-delimited JSON), CSV, or XML formats with customizable and aliasable output fields. NDJSON is also supported in `--stream` mode for low-latency, tool-chain-friendly output (compatible with `jq`, logstash, Elasticsearch Bulk API). |
| **Statistical Analysis**     | Generate statistics on log data: entry rates, top messages, log level counts, unique value counts, time-bucket histograms (configurable window), and P50/P95/P99 percentiles for any numeric custom field. |
| **Output Control**           | `--limit N` caps matching output at N entries; `--offset N` skips the first N (enabling pagination: `--offset 200 --limit 100`); `--count` prints only the match count (like `grep -c`), with no per-entry output. |
