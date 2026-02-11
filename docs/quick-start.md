# Quick Start

`logAnalyzer` is a versatile and powerful command-line utility. Here’s a quick overview of its basic usage to get you started.

## Basic Syntax

The general syntax for `logAnalyzer` commands is:

```bash
logAnalyzer [input-file(s)] [options]
```

Where:
*   `[input-file(s)]`: One or more paths to log files you want to analyze. If omitted, `logAnalyzer` will attempt to read from standard input (`stdin`).
*   `[options]`: Various command-line flags and arguments to control filtering, output format, statistics, and more.

## Example: Basic Filtering

Here are a few common scenarios to demonstrate `logAnalyzer`'s capabilities:

### Filter for specific log levels

To analyze a specific log file (`/var/log/syslog`) and filter only for entries marked as `ERROR`:

```bash
logAnalyzer /var/log/syslog --level ERROR
```

### Process data from standard input

You can pipe output from other commands directly into `logAnalyzer`. For instance, to process `application.log` and search for the keyword "authentication failed":

```bash
cat application.log | logAnalyzer --stdin --keyword "authentication failed"
```
The `--stdin` flag explicitly tells `logAnalyzer` to read from standard input.

### Export filtered logs to JSON

To find all errors from `app.log` that occurred within the last 2 hours, and then export these results into a pretty-printed JSON file named `errors.json`:

```bash
logAnalyzer app.log --start "2h ago" --level ERROR --format json --pretty --output errors.json
```

This command demonstrates filtering by time, log level, and exporting to a structured format.

---

For a deep dive into all functionalities, advanced filtering techniques, multi-line parsing, and many more detailed examples, please check out our [**Usage Examples**](usage-examples.md) and the comprehensive [**CLI Reference**](cli-reference.md).