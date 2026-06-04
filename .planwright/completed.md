- [x] Add --limit N and --count CLI options
      Mode: develop
      Rationale: invent-tier: filtering pipeline had no count ceiling (--limit) and no match-count-only output (--count / grep -c behavior).
      Surfaces: include/config/cli.h, src/config/cli.cpp, src/main.cpp, tests/config/cli/filtering.cpp, tests/config/cli/analysis.cpp
      Acceptance: --limit N truncates filteredEntries to N before export; --count prints entry count and exits 0 with no other output.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_filtering$|^config_cli_analysis$" --output-on-failure

- [x] Add TIME_BUCKET_HISTOGRAM statistic collector
      Mode: develop
      Rationale: invent-tier: StatisticType had ENTRY_RATE but no time-bucket histogram; users cannot identify log volume spikes without per-bucket counts grouped by configurable time window.
      Surfaces: include/stats/core.h, src/stats/core.cpp, src/stats/analyzer.cpp, src/utils/core.cpp, tests/stats/core.cpp
      Acceptance: TimeBucketHistogramCollector buckets entries by floor(epoch/bucketSeconds)*bucketSeconds, silently skips entries without timestamps, sorts ascending; statisticTypeToString/stringToStatisticType round-trip passes.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [x] Add NDJSON export format
      Mode: develop
      Rationale: invent-tier: the log-streaming ecosystem (jq, logstash, fluentd, Elasticsearch Bulk API) expects newline-delimited JSON — one compact JSON object per line — which the Exporter cannot currently produce, forcing users to post-process full JSON arrays.
      Evidence: ExportFormat enum in include/export/core.h enumerates {PLAINTEXT, JSON, CSV, XML, UNKNOWN} — no NDJSON value; --format option in src/config/cli.cpp parseCLI restricts to {"text","json","csv","xml"} via CLI::IsMember; Exporter::exportLogEntries in src/export/core.cpp has no NDJSON dispatch case.
      Surfaces: include/export/core.h, src/export/json.cpp, src/export/core.cpp, src/config/cli.cpp, src/utils/core.cpp, tests/CMakeLists.txt
      New Surfaces: tests/export/ndjson.cpp
      Acceptance: --format ndjson emits one compact JSON object per line with no array wrapper; each line independently parses; existing JSON/CSV/text/XML export tests remain green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^export_ndjson$|^export_json$" --output-on-failure

- [x] Add tests for structured-field JSON export (nested parse and raw-string fallback)
      Mode: improve
      Rationale: The JSON exporter expands a STRUCTURED_FIELD whose value is valid JSON into a nested JSON object and falls back to the raw string on parse error, but no export test exercises this path, so the nesting and fallback behavior is unverified.
      Evidence: src/export/json.cpp exportAsJson handles LogEntryField::STRUCTURED_FIELD (lines 151-161) by json::parse(entry.structuredData.value()) into value_json, catching json::parse_error to store the raw string instead; grep shows no test under tests/export/ sets structuredData for a JSON-format export (the only structuredData test, in tests/export/core.cpp, exercises XML).
      Surfaces: tests/export/json.cpp
      Development: In tests/export/json.cpp add a TEST that sets entry.structuredData to a valid nested JSON string (object with a nested array) and settings.fieldsToExport = { ExportFieldMapping(LogEntryField::STRUCTURED_FIELD) }, exports as JSON, parses the output, and asserts j["entries"][0]["STRUCTURED_FIELD"] is a nested object/array with the expected values; add a second TEST with invalid JSON in structuredData asserting the field serializes as the raw string.
      Acceptance: The structured-field JSON path (nested parse and raw-string fallback) is covered by passing assertions and the export suite stays green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^export_json$" --output-on-failure

- [x] Fix build break: add missing <climits> include in tests/utils/time.cpp
      Mode: repair
      Rationale: The entire test build fails to compile because LLONG_MAX is used without including <climits>.
      Evidence: tests/utils/time.cpp:80 references std::to_string(LLONG_MAX); g++ reports "error: 'LLONG_MAX' was not declared in this scope"; the include block (lines 4-9) pulls in <cstdlib>/<chrono> but not <climits>.
      Surfaces: tests/utils/time.cpp
      Development: Add `#include <climits>` to the standard-header include block (alongside <cstdlib> at line 9) in tests/utils/time.cpp; the macro is consumed at line 80 inside the stoll out_of_range test for parseDuration.
      Acceptance: The full project build completes with no compiler errors and the utils_time target's parseDuration overflow assertions pass unchanged.
      Verification: cmake --build build -j && ctest --test-dir build -R "^utils_time$" --output-on-failure

- [x] Enforce --limit and --count in stream path
      Mode: repair
      Rationale: streamEntryCallback always returned true; --limit and --count had no effect in --stream mode.
      Surfaces: src/main.cpp
      Acceptance: --stream --limit N stops after N matches; --stream --count prints count and no entries; --stream --limit N --count prints count up to N.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_filtering$|^config_cli_analysis$" --output-on-failure

- [x] Add TIME_BUCKET_HISTOGRAM factory tests with bucket param
      Mode: improve
      Surfaces: tests/stats/core.cpp
      Acceptance: Three factory tests cover default bucket, explicit bucket=30, and invalid bucket=0 fallback; stats_core green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [x] Add TIME_BUCKET_HISTOGRAM CLI --stats parsing tests
      Mode: improve
      Surfaces: tests/config/cli/analysis.cpp
      Acceptance: Bare-name and KV-form parsing verified; config_cli_analysis green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_analysis$" --output-on-failure

- [x] Add NDJSON round-trip tests for ExportSettings
      Mode: improve
      Surfaces: tests/export/settings.cpp
      Acceptance: NDJSON serializes to "NDJSON" and deserializes back to ExportFormat::NDJSON; export_settings green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^export_settings$" --output-on-failure

- [x] Add --offset N CLI pagination option
      Mode: develop
      Surfaces: include/config/cli.h, src/config/cli.cpp, src/main.cpp, tests/config/cli/filtering.cpp
      Acceptance: --offset N skips first N in batch path; --offset 2 --limit 2 returns entries 2-3; config_cli_filtering green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_filtering$" --output-on-failure

- [x] Add NDJSON support to stream mode
      Mode: develop
      Surfaces: src/main.cpp
      Acceptance: --stream --format ndjson emits one JSON object per entry line; --stream --format xml still errors; full suite green.
      Verification: cmake --build build -j && ctest --test-dir build --output-on-failure

- [x] Add PERCENTILE_STATS statistic collector for numeric custom fields
      Mode: develop (invent-tier)
      Surfaces: include/stats/core.h, src/stats/core.cpp, src/stats/analyzer.cpp, src/utils/core.cpp, tests/stats/core.cpp
      Acceptance: P50/P95/P99 computed for named numeric custom field; silently skips absent/non-numeric; factory returns nullptr for missing field param; round-trip passes; stats_core green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [x] Update docs/features.md with NDJSON, --limit/--offset/--count, stats
      Mode: docs
      Surfaces: docs/features.md
      Acceptance: Flexible Export row updated with NDJSON; Statistical Analysis mentions time-bucket histograms and percentiles; Output Control row added.
      Verification: cmake --build build -j --output-on-failure
