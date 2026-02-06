# Why logAnalyzer?

Modern applications generate gigabytes of logs daily. While tools like `grep`, `awk`, or `less` are powerful, they often fall short when dealing with the scale and complexity of today's logging formats. You've likely felt the pain of:

-   **Complex Queries**: Trying to filter logs by a specific time range and multiple keywords (`grep "ERROR" | grep "2023-10-27 10:"`) is cumbersome and inefficient.
-   **Lack of Structure**: Parsing structured formats like JSON or key-value pairs requires custom, often brittle, scripting.
-   **Performance Bottlenecks**: Searching multi-gigabyte files can be slow and memory-intensive, bringing your analysis to a crawl.
-   **No Built-in Analytics**: `grep` can find lines, but it can't tell you the rate of errors per minute or the top 10 most common log messages.

`logAnalyzer` was built to solve these problems. It treats your logs as a structured data source, allowing you to query them with power and flexibility, right from your terminal. Its stream-based architecture ensures it can handle files of any size with minimal memory usage.
