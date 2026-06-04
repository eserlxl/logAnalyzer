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

- [x] Enforce --offset N in stream path
      Mode: repair
      Surfaces: src/main.cpp
      Acceptance: --stream --offset 2 skips first 2 matching entries; --stream --offset 2 --limit 3 skips 2 then emits 3; skipped entries not counted.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_filtering$" --output-on-failure

- [x] Add PERCENTILE_STATS CLI --stats parsing tests
      Mode: improve
      Surfaces: tests/config/cli/analysis.cpp
      Acceptance: PercentileStatsKVForm and PercentileStatsBareNameProducesConfigWithoutField pass; config_cli_analysis green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_analysis$" --output-on-failure

- [x] Add PercentileStatsCollector edge-case and reset tests
      Mode: improve
      Surfaces: tests/stats/core.cpp
      Acceptance: Single-value, two-values, and reset tests pass; stats_core green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [x] Add percentile_stats:field_name colon shorthand to parseStatisticConfig
      Mode: develop
      Surfaces: src/config/cli_helpers.cpp, tests/config/cli/analysis.cpp
      Acceptance: --stats percentile_stats:latency_ms produces correct StatisticConfig; empty field returns nullopt; config_cli_analysis green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_analysis$" --output-on-failure

- [x] Add --since DURATION relative time filter shorthand
      Mode: develop
      Surfaces: include/config/cli.h, src/config/cli.cpp, tests/config/cli/filtering.cpp
      Acceptance: --since 1h sets startTime to now-1h; --since + --start yields InvalidCLIOption; config_cli_filtering green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_filtering$" --output-on-failure

- [x] Add --stats-output PATH option
      Mode: develop
      Surfaces: include/config/cli.h, src/config/cli.cpp, src/main.cpp, tests/config/cli/analysis.cpp
      Acceptance: --stats-output file writes stats JSON to file; entry output goes to stdout; config_cli_analysis green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_analysis$" --output-on-failure

- [x] Add MOVING_AVERAGE_RATE statistic collector
      Mode: develop
      Surfaces: include/stats/core.h, src/stats/core.cpp, src/stats/analyzer.cpp, src/utils/core.cpp, tests/stats/core.cpp
      Acceptance: Mean/min/max per bucket; skips no-timestamp; bucket param overrides default 60s; round-trip passes; stats_core green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [x] Add --dedup-field FIELD option
      Mode: develop
      Surfaces: include/config/cli.h, src/config/cli.cpp, src/main.cpp, tests/config/cli/filtering.cpp
      Acceptance: --dedup-field message keeps first per unique message; absent custom field treats each entry as unique; config_cli_filtering green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_filtering$" --output-on-failure

- [x] Enforce --dedup-field in stream path
      Mode: repair
      Surfaces: src/main.cpp
      Acceptance: --stream --dedup-field field deduplicates in stream path same as batch; stats+stream no longer silent.
      Verification: cmake --build build -j && ctest --test-dir build --output-on-failure

- [x] Fix --stats --stream silent no-op
      Mode: repair
      Surfaces: src/main.cpp
      Acceptance: --stream --stats emits stats block; --stream --stats --stats-output writes to file.
      Verification: cmake --build build -j && ctest --test-dir build --output-on-failure

- [x] Add MOVING_AVERAGE_RATE bare-name, KV-form CLI tests and reset test
      Mode: improve
      Surfaces: tests/config/cli/analysis.cpp, tests/stats/core.cpp
      Acceptance: bare-name, KV-form, and reset tests pass; suites green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_analysis$|^stats_core$" --output-on-failure

- [x] Extract and unit-test applyDedupField helper
      Mode: improve
      Surfaces: include/utils/dedup.h, src/main.cpp, tests/utils/dedup.cpp, tests/CMakeLists.txt
      Acceptance: 5 behavioral dedup tests pass; full suite green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^utils_dedup$" --output-on-failure

- [x] Add moving_average_rate:N colon shorthand
      Mode: develop
      Surfaces: src/config/cli_helpers.cpp, tests/config/cli/analysis.cpp
      Acceptance: --stats moving_average_rate:30 sets bucket=30; empty bucket fails; tests green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_analysis$" --output-on-failure

- [x] Update docs/cli-reference.md for Cycle 1-3 features
      Mode: docs
      Surfaces: docs/cli-reference.md
      Acceptance: --since, --limit, --offset, --count, --dedup-field, --stats-output, ndjson, time_bucket_histogram, percentile_stats, moving_average_rate all documented.
      Verification: cmake --build build -j --output-on-failure

- [x] Implement GapDetectorCollector to fix --find-gaps no-op
      Mode: repair
      Surfaces: include/stats/core.h, src/stats/core.cpp, src/stats/analyzer.cpp, src/utils/core.cpp, src/config/cli.cpp
      Acceptance: --find-gaps 100 on a log with a 200ms gap emits a gap report; factory with threshold_ms param works; stats_core green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [x] Add GapDetectorCollector unit tests
      Mode: improve
      Surfaces: tests/stats/core.cpp
      Acceptance: 7 tests pass: detects gap, skips below threshold, skips no-timestamp, empty, reset, round-trip, factory.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [x] Fix EntryRateCollector inconsistent duration_sec output schema
      Mode: repair
      Surfaces: src/stats/core.cpp
      Acceptance: duration_sec always emitted as integer; 0 in <2-entries path; actual seconds in success path.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [x] Add EntryRateCollector direct unit tests
      Mode: improve
      Surfaces: tests/stats/core.cpp
      Acceptance: 5 tests: basic rate, single-entry, reset, round-trip, factory.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [x] Add find_gaps:N colon shorthand and update --find-gaps CLI test
      Mode: develop
      Surfaces: src/config/cli_helpers.cpp, tests/config/cli/analysis.cpp
      Acceptance: --find-gaps 5000 creates StatisticConfig(FIND_GAPS); --stats find_gaps:2000 works; empty threshold fails.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_analysis$" --output-on-failure

- [x] Extract getDedupKey helper to unify stream and batch dedup logic
      Mode: improve
      Surfaces: include/utils/dedup.h, src/main.cpp, tests/utils/dedup.cpp
      Acceptance: 4 new direct getDedupKey tests pass; stream path uses getDedupKey; utils_dedup green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^utils_dedup$" --output-on-failure

- [x] Update docs/features.md and cli-reference.md for gap detection and stats-in-stream
      Mode: docs
      Surfaces: docs/features.md, docs/cli-reference.md
      Acceptance: Gap detection in features; --find-gaps updated; stats-in-stream noted; --stats NAME includes find_gaps.
      Verification: cmake --build build -j --output-on-failure

- [x] Add UniqueMessagesCollector and TopMessagesCollector direct unit tests
      Mode: improve
      Surfaces: tests/stats/core.cpp
      Acceptance: 5 tests pass; stats_core green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [x] Add PERCENTILE_STATS field-param validation in validation.cpp
      Mode: repair
      Surfaces: src/config/validation.cpp, tests/config/core/validation.cpp
      Acceptance: Missing/empty 'field' emits error; valid passes; config_core_validation green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_core_validation$" --output-on-failure

- [x] Warn on --sort-by + --stream (silent no-op)
      Mode: repair
      Surfaces: src/main.cpp, tests/config/cli/analysis.cpp
      Acceptance: --stream --sort-by level emits warning to stderr but succeeds; config_cli_analysis green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_analysis$" --output-on-failure

- [x] Add reset/no-timestamp/out-of-order tests for stat collectors
      Mode: improve
      Surfaces: tests/stats/core.cpp
      Acceptance: 6 tests pass; stats_core green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [x] Add StatisticConfig JSON round-trip tests for new types
      Mode: improve
      Surfaces: tests/config/core/json/serialization.cpp
      Acceptance: 4 types round-trip correctly; config_core_json_serialization green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_core_json_serialization$" --output-on-failure

- [x] Document --expression filter language in cli-reference.md
      Mode: docs
      Surfaces: docs/cli-reference.md
      Acceptance: Expression Filter Reference subsection present; build passes.
      Verification: cmake --build build -j --output-on-failure
