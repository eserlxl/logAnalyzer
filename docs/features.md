# Features

| Feature                      | Description                                                                                             |
| ---------------------------- | ------------------------------------------------------------------------------------------------------- |
| **Memory-Efficient Processing** | Handles massive files with minimal memory usage using the `--stream` mode.                              |
| **Multi-File Support**       | Parses and analyzes multiple log files in a single run.                                                 |
| **Sorting**                  | Sort results by timestamp, log level, message, or other fields in ascending or descending order.        |
| **Structured Field Parsing** | Automatically parses log messages into fields, including structured data, using custom patterns and intelligent detection.                         |
| **Keyword & Regex Filtering**| Filter by log level, keywords, glob patterns (anchored), and case-sensitive/insensitive regular expressions. |
| **Field-Value Matching**     | Match field values with case-sensitive/insensitive text, regex, and glob patterns.                      |
| **Nested Field Filtering**   | Target nested fields within structured data (e.g., `user.id` in a JSON log).                            |
| **Advanced Data Types**      | Compare fields as `version` numbers (semantic versioning) or `IP addresses`.                            |
| **Numeric & Bool Filtering** | Perform numeric (`>`, `<`, `==`) or boolean (`true`, `false`) comparisons on flat and nested fields.     |
| **Set-Based Filtering**      | Check if a field's value is `in` or `not in` a specific set of values.                                   |
| **Time-based Filtering**     | Filter by absolute time range, relative time (`5m ago`), or for a specific day (`yesterday`, `2023-10-20`). |
| **Field Presence Checks**    | Filter for logs where a specific field `is present` or `is absent`.                                       |
| **Complex Filter Expressions** | (Coming Soon) Build sophisticated filter logic using parenthesized, nested `AND`/`OR`/`NOT` conditions.                     |
| **Flexible Export**          | Save results in Text, JSON, or CSV formats with customizable and aliasable output fields.                 |
| **Statistical Analysis**     | Generate statistics on log data, such as entry rates, top messages, log level counts, and unique value counts for any field. |
