# planwright Plan — .
<!-- Session: 2026-06-04T00:00:00Z -->

- [x] Add NDJSON export format
      Mode: develop
      Rationale: invent-tier: the log-streaming ecosystem (jq, logstash, fluentd, Elasticsearch Bulk API) expects newline-delimited JSON — one compact JSON object per line — which the Exporter cannot currently produce, forcing users to post-process full JSON arrays.
      Evidence: ExportFormat enum in include/export/core.h enumerates {PLAINTEXT, JSON, CSV, XML, UNKNOWN} — no NDJSON value; --format option in src/config/cli.cpp parseCLI restricts to {"text","json","csv","xml"} via CLI::IsMember; Exporter::exportLogEntries in src/export/core.cpp has no NDJSON dispatch case.
      Surfaces: include/export/core.h, src/export/json.cpp, src/export/core.cpp, src/config/cli.cpp, tests/CMakeLists.txt
      New Surfaces: tests/export/ndjson.cpp
      Development: In include/export/core.h add NDJSON to ExportFormat enum (after XML); in src/export/json.cpp add void Exporter::exportAsNdjson(ostream&, entries, settings) that iterates entries and writes one compact json object per line (no array wrapper, jsonIndent=-1 forced); in src/export/core.cpp add case ExportFormat::NDJSON: exportAsNdjson(os,entries,settings); break; in src/config/cli.cpp extend the --format IsMember list to include "ndjson" and map outputFormat=="ndjson" to ExportFormat::NDJSON; in tests/export/ndjson.cpp add TEST(ExporterNdjsonTest, EachEntryOnOwnLine) asserting N entries produce N lines each parseable by json::parse, and add "export/ndjson.cpp" to TEST_SOURCES in tests/CMakeLists.txt.
      Acceptance: --format ndjson emits one compact JSON object per line with no array wrapper; each line independently parses; existing JSON/CSV/text/XML export tests remain green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^export_ndjson$|^export_json$" --output-on-failure

- [x] Add TIME_BUCKET_HISTOGRAM statistic collector
      Mode: develop
      Rationale: invent-tier: StatisticType has ENTRY_RATE (average rate over full run) but no time-bucket histogram; users cannot identify when log volume spikes occurred — a core log-analysis use case — without a per-bucket count grouped by configurable time window.
      Evidence: StatisticType enum in include/stats/core.h enumerates {UNIQUE_MESSAGES, TOP_MESSAGES, ENTRY_RATE, LOG_LEVEL_COUNT, FIELD_VALUE_COUNT, TOP_N_FIELD_VALUES, UNKNOWN} — no histogram type; Statistics::createCollector switch in src/stats/core.cpp has no histogram case; LogAnalyzer::createStatisticCollector in src/stats/analyzer.cpp likewise has no histogram case.
      Surfaces: include/stats/core.h, src/stats/core.cpp, src/stats/analyzer.cpp, tests/stats/core.cpp
      Development: In include/stats/core.h add TIME_BUCKET_HISTOGRAM to StatisticType and declare class TimeBucketHistogramCollector : public IStatisticCollector with int _bucketSeconds param (default 60); collect(entry) increments _counts[floor(epoch/_bucketSeconds)*_bucketSeconds] if entry.timestamp is set, skips silently otherwise; generateReport() emits {name:"time_bucket_histogram", bucket_seconds, buckets:[{start_time, count}]} sorted ascending by start_time; in Statistics::createCollector add TIME_BUCKET_HISTOGRAM case parsing optional "bucket" param (e.g. "60", "1m", "1h"); in src/stats/analyzer.cpp add matching case to LogAnalyzer::createStatisticCollector; in tests/stats/core.cpp add TEST_F(StatisticsTest, TimeBucketHistogramCollector) with 6 entries spanning 3 one-minute buckets asserting 3 buckets with correct counts; include Utils::statisticTypeToString/stringToStatisticType round-trip for TIME_BUCKET_HISTOGRAM.
      Acceptance: --stats "type=TIME_BUCKET_HISTOGRAM,bucket=60" outputs per-minute entry counts sorted by timestamp; entries without timestamps are silently skipped; stats_core test suite passes.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [x] Add --limit N CLI option to cap output at N matching entries
      Mode: develop
      Rationale: invent-tier: the filtering pipeline returns all matching entries with no count ceiling; users inspecting large logs have no built-in way to stop after the first N matches — standard grep-m / head behavior — and must pipe through an external tool.
      Evidence: CLIOptions in include/config/cli.h has no limit field; parseCLI in src/config/cli.cpp registers no --limit option; src/main.cpp processes filteredEntries vector without any count ceiling in both the batch and stream paths.
      Surfaces: include/config/cli.h, src/config/cli.cpp, src/main.cpp, tests/config/cli/filtering.cpp
      Development: In include/config/cli.h add std::optional<size_t> limit to CLIOptions; in src/config/cli.cpp parseCLI add app.add_option("--limit", appOptions.limit, "Stop after N matching entries")->check(CLI::PositiveNumber); in src/main.cpp after filteredEntries is built add if (cliOptions.limit && filteredEntries.size() > *cliOptions.limit) filteredEntries.resize(*cliOptions.limit); in the stream path increment a counter per callback and break when counter reaches limit; in tests/config/cli/filtering.cpp add a TEST that parses {"--limit","2","logfile"} and asserts CLIOptions::limit == 2.
      Acceptance: --limit 5 caps output at 5 entries regardless of how many match; stats collectors run only on the limited set; no existing test is broken.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_filtering$" --output-on-failure

- [x] Add --count flag to print only the match count
      Mode: develop
      Rationale: invent-tier: users frequently need only the count of matching entries (e.g. "how many ERRORs in the last hour?") but currently must run a full export and pipe to wc -l; a --count flag (grep -c behavior) would short-circuit the export pipeline and print a single integer.
      Evidence: CLIOptions in include/config/cli.h has no countOnly field; parseCLI in src/config/cli.cpp registers no --count flag; src/main.cpp always proceeds to full format/export output after filtering.
      Surfaces: include/config/cli.h, src/config/cli.cpp, src/main.cpp, tests/config/cli/analysis.cpp
      Development: In include/config/cli.h add bool countOnly = false to CLIOptions; in src/config/cli.cpp parseCLI add app.add_flag("--count", appOptions.countOnly, "Print only the count of matching entries, then exit"); in src/main.cpp after filteredEntries is populated (batch path) add if (cliOptions.countOnly) { *outputStream << filteredEntries.size() << '\n'; return 0; }; in stream path count callback invocations and print on stream completion when countOnly is set; in tests/config/cli/analysis.cpp add a TEST that parses {"--count","logfile"} and asserts CLIOptions::countOnly == true.
      Acceptance: --count prints a single integer followed by newline and exits 0; no other output is produced; --count is mutually exclusive with --format (error if combined); existing tests unaffected.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_analysis$" --output-on-failure
