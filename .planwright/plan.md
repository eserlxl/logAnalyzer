# planwright Plan — .
<!-- Session: 2026-06-05T00:00:00Z -->

- [x] Add --max-level to docs/cli-reference.md Filtering table and update features.md
      Mode: docs
      Rationale: --max-level was implemented in Cycle 10 but the CLI reference still has no --max-level row; docs/features.md line 9 "Keyword & Regex Filtering" row also omits range filtering. Users relying on docs to discover options will not find the feature.
      Evidence: docs/cli-reference.md:50 — has --min-level row but no --max-level row; grep for "max.level" in docs/cli-reference.md returns nothing; docs/features.md:9 — says "Filter by log level, keywords" with no mention of --min-level or --max-level; include/config/cli.h now has maxLogLevel field; src/config/cli.cpp has the --max-level registration; tests pass.
      Surfaces: docs/cli-reference.md, docs/features.md
      Development: In docs/cli-reference.md, insert a `--max-level LEVEL` row immediately after the `--min-level LEVEL` row (line 50), describing it as "Includes log entries with a level equal to or less severe than the specified level (e.g., `WARNING` will include `TRACE`, `DEBUG`, `INFO`, `WARNING`). Case-insensitive." In docs/features.md line 9, update the "Keyword & Regex Filtering" row description to mention level range filtering: add "Supports `--min-level` / `--max-level` for inclusive severity range filtering."
      Acceptance: grep -n "max-level" docs/cli-reference.md shows the new row; grep "max-level\|min-level" docs/features.md shows the updated row; no existing test breaks.
      Verification: grep -n "max-level" /opt/lxl/c++/logAnalyzer/docs/cli-reference.md && grep "min-level\|max-level" /opt/lxl/c++/logAnalyzer/docs/features.md

- [x] Add MaxLevelFilterTest unit test to tests/filter/core/unit/basic.cpp
      Mode: improve
      Rationale: tests/filter/core/unit/basic.cpp has a MinLevelFilterTest (line 123) that directly exercises MinLevelFilter::matches, but MaxLevelFilter was added in Cycle 10 without a symmetric filter-level unit test — its behavior (entry.level <= maxLevel_) is only indirectly covered by the CLI option test.
      Evidence: tests/filter/core/unit/basic.cpp:123-135 — MinLevelFilterTest creates MinLevelFilter(WARNING) and asserts FATAL/ERROR/WARNING pass, INFO/DEBUG fail; no MaxLevelFilterTest exists; tests/config/cli/filtering.cpp:226 MaxLevel only confirms the CLI option sets maxLogLevel.has_value() but never exercises the filter's matches() method.
      Surfaces: tests/filter/core/unit/basic.cpp
      Development: After the MinLevelFilterTest block (line 135), add TEST_F(FilterTest, MaxLevelFilterTest) creating MaxLevelFilter(WARNING) and asserting entry_trace/debug/info/warning pass (level <= WARNING), and entry_error/critical/fatal fail (level > WARNING) — mirror the structure of MinLevelFilterTest.
      Acceptance: MaxLevelFilterTest covers all 7 LogLevel values; filter_core_unit_basic green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^filter_core_unit_basic$" --output-on-failure

- [x] Add MaxLevelInvalid and MinLevelMaxLevelRange CLI option tests
      Mode: improve
      Rationale: tests/config/cli/filtering.cpp has InvalidLogLevel (for --level INVALID) but no parallel for --max-level INVALID; it also tests --min-level and --max-level individually but never together, leaving the AND-combination (range filter) behavior unconfirmed.
      Evidence: tests/config/cli/filtering.cpp:28 InvalidLogLevel tests "--level INVALID" → Code::InvalidCLIOption; MaxLevel (line 226) and MinLevel (line 210) test valid values only; no MinLevelMaxLevelRange test verifying both options are set simultaneously; the filterLevels AND logic that combines them is what users actually rely on.
      Surfaces: tests/config/cli/filtering.cpp
      Development: Add TEST_F(CLIConfigTest, MaxLevelInvalid) parsing "--max-level INVALID" asserting !result.has_value() with Code::InvalidCLIOption — mirror InvalidLogLevel. Add TEST_F(CLIConfigTest, MinLevelMaxLevelRange) parsing "--min-level INFO --max-level WARNING" asserting both minLogLevel=INFO and maxLogLevel=WARNING are set.
      Acceptance: MaxLevelInvalid rejects, MinLevelMaxLevelRange parses both correctly; config_cli_filtering green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_filtering$" --output-on-failure

- [x] Add TopNFieldValuesCollectorForId and ValidateStatisticConfig_TopNFieldValuesIdAccepted tests
      Mode: improve
      Rationale: FieldValueCountCollectorForId tests the id field for FieldValueCountCollector; both collector types use stats::detail::extractFieldValue via getFieldValueAsString, but TopNFieldValuesCollector's id path (including its top-N sort-and-truncate logic) is untested. ValidateStatisticConfig_FieldValueCountIdAccepted exists but no parallel for TOP_N_FIELD_VALUES.
      Evidence: tests/stats/core.cpp — FieldValueCountCollectorForId added in C10; TopNFieldValuesCollector tests exist for message/thread_id but not for id field; tests/config/core/validation.cpp — ValidateStatisticConfig_FieldValueCountIdAccepted exists; TOP_N_FIELD_VALUES with target_field=id untested.
      Surfaces: tests/stats/core.cpp, tests/config/core/validation.cpp
      Development: In tests/stats/core.cpp add TEST_F(StatisticsTest, TopNFieldValuesCollectorForId): create TopNFieldValuesCollector(2, "id"), collect 4 entries with ids {10, 10, 20, 30} (entry with no id skipped), assert report["values"][0]["value"]=="10" (most frequent, count 2); assert only 2 values returned (top_n=2). In tests/config/core/validation.cpp add ValidateStatisticConfig_TopNFieldValuesIdAccepted: FIELD_VALUE_COUNT-like config with TOP_N_FIELD_VALUES type, target_field=id, top_n=5, assert errors.empty().
      Acceptance: Both new tests pass; existing tests unaffected; stats_core and config_core_validation green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$|^config_core_validation$" --output-on-failure

- [x] Add case-insensitive id field tests to config/core/validation
      Mode: improve
      Rationale: normalizeTargetFieldName lowercases the input before matching, so "ID", "Id", "iD" all map to "id" — but no test verifies this for the newly added id field specifically (existing case-insensitive tests only cover "LeVeL" and similar standard fields).
      Evidence: include/stats/helpers.h:43-55 — std::transform to lowercase then if (field == "id") return "id"; tests/config/core/validation.cpp:172 — ValidateStatisticConfig_TopNFieldValues_CaseInsensitiveTargetField uses "LeVeL" to verify lowercasing; no test with "ID" or "Id" in target_field.
      Surfaces: tests/config/core/validation.cpp
      Development: In tests/config/core/validation.cpp add TEST_F(ConfigValidationTest, ValidateStatisticConfig_FieldValueCountIdCaseInsensitive): pass target_field="ID" (uppercase) for FIELD_VALUE_COUNT, assert errors.empty(). Add a second case with "Id" (mixed) in the same test or a separate test. Mirror the ValidateStatisticConfig_TopNFieldValues_CaseInsensitiveTargetField pattern.
      Acceptance: target_field="ID" and target_field="Id" both pass validation; config_core_validation green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_core_validation$" --output-on-failure

- [x] Extend PercentileStatsCollector to support standard LogEntry numeric fields
      Mode: develop
      Rationale: PercentileStatsCollector::collect only looks up customFields by key; extractFieldValue in helpers.h supports lineNumber (std::to_string(*entry.sourceLineNumber)), id (std::to_string(*entry.id)), timestamp (formatted string), and other standard fields — any numeric-valued standard field (lineNumber, id) can produce meaningful percentile results.
      Evidence: src/stats/core.cpp:212-218 — collect uses normalizeTargetFieldName + extractFieldValue; stats/helpers.h extractFieldValue handles all LogEntry fields; tests/stats/core.cpp — PercentileStatsCollectorForLineNumber and PercentileStatsCollectorForId added.
      Surfaces: src/stats/core.cpp, include/stats/core.h, tests/stats/core.cpp, docs/cli-reference.md
      Development: Implemented in C11. normalizeTargetFieldName + extractFieldValue path active; custom field fallback preserved.
      Acceptance: --stats percentile_stats:lineNumber and percentile_stats:id work; stats_core green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [x] Add MinLevelInvalid test parallel to MaxLevelInvalid
      Mode: improve
      Rationale: InvalidLogLevel tests --level INVALID and MaxLevelInvalid (from item 3) tests --max-level INVALID; --min-level also uses the same CLI11 CheckedTransformer and should have its own rejection test for completeness, but none exists.
      Evidence: tests/config/cli/filtering.cpp — InvalidLogLevel (line 28) covers --level; no MinLevelInvalid test; MaxLevelInvalid is being added by the preceding plan item; --min-level uses transform(CLI::CheckedTransformer(Config::LogLevelMap, CLI::ignore_case)) same as --max-level.
      Surfaces: tests/config/cli/filtering.cpp
      Development: In tests/config/cli/filtering.cpp add TEST_F(CLIConfigTest, MinLevelInvalid) parsing "--min-level INVALID" asserting !result.has_value() with Code::InvalidCLIOption — single test, mirrors MaxLevelInvalid.
      Acceptance: MinLevelInvalid rejects with InvalidCLIOption; config_cli_filtering green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_filtering$" --output-on-failure

- [x] Add PercentileStatsCollectorReset test
      Mode: improve
      Rationale: Every IStatisticCollector has a reset() method; stats/core.h line 220 defines PercentileStatsCollector::reset() as { _values.clear(); }; no test verifies that reset() discards accumulated samples so the collector can be reused.
      Evidence: tests/stats/core.cpp — MovingAverageRateCollectorReset exists; no PercentileStatsCollectorReset test; the pattern is: collect some entries, call reset(), re-collect, assert report reflects only post-reset entries.
      Surfaces: tests/stats/core.cpp
      Development: In tests/stats/core.cpp add TEST_F(StatisticsTest, PercentileStatsCollectorReset): create PercentileStatsCollector("latency_ms"), collect one entry with latency_ms="100", call reset(), collect one entry with latency_ms="200", call generateReport(), assert count==1 and p50≈200.
      Acceptance: PercentileStatsCollectorReset passes; stats_core green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [x] Fix --count + --stats: stats silently dropped in batch mode when --count is active
      Mode: repair
      Rationale: In batch mode, src/main.cpp:426-429 returns before the statistics block at lines 486-510 when --count is set, silently discarding all stats output. If a user combines --count and --stats (or --stats-output), the statistics are never computed or written. This is a confirmed defect with no workaround.
      Evidence: src/main.cpp:425-429 — `if (cliOptions.countOnly) { *outputStream << filteredEntries.size() << '\n'; return 0; }` — exits before stats at lines 486-510; `--stats-output` path at line 495 is also skipped; stream mode has no corresponding early-return because entries are processed one by one and stats are emitted at the end of the stream loop (separate code path).
      Surfaces: src/main.cpp
      Development: In src/main.cpp, move the stats computation block (lines 486-510) to before the `--count` check at line 426. The stats block must still run against `filteredEntries` (after limit is applied). After stats are emitted, then check `if (cliOptions.countOnly)` and print the count + return. This ensures that `--count --stats-output result.json` writes stats and then prints the count to stdout/outputStream.
      Acceptance: Running with `--count --stats-output /tmp/s.json` writes the stats file AND prints the count. Running with `--count` alone (no --stats) still only prints the count and exits normally. No existing tests break.
      Verification: cmake --build build -j && ctest --test-dir build --output-on-failure

- [x] Add CountWithStats test verifying --count and --stats both produce output
      Mode: improve
      Rationale: The repair to --count + --stats (preceding item) needs a test that covers the combined path so regression is caught. Currently tests/config/cli/analysis.cpp has a CountOnly test (line 120-126) that only parses the CLI option; no integration test verifies that stats are produced alongside the count.
      Evidence: tests/config/cli/analysis.cpp:122-125 — parses "--count" and asserts countOnly==true, nothing more; no test exercises both countOnly and statisticsEnabled in the same run; the bug is in src/main.cpp's runtime path, not in parsing.
      Surfaces: tests/config/cli/analysis.cpp
      Development: In tests/config/cli/analysis.cpp add TEST_F(CLIConfigTest, CountWithStatsOption): parse `{"--count", "--stats", "count_by_level"}` (or use the key-value form), assert countOnly==true AND statisticsSettings is non-empty. This confirms the options can coexist at the parsing layer. If an integration harness exists for batch output (or if tests run main() directly), also add an integration case that verifies stats output is present alongside the count number.
      Acceptance: CountWithStatsOption passes; no existing tests break; config_cli_analysis green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_analysis$" --output-on-failure

- [x] Fix docs/features.md line 21: percentile_stats docs omit standard numeric fields
      Mode: docs
      Rationale: docs/features.md:21 says "P50/P95/P99 percentiles for any numeric custom field" — but C11 extended PercentileStatsCollector to support standard LogEntry numeric fields (lineNumber, id) via normalizeTargetFieldName + extractFieldValue. The doc still implies only custom fields are supported, misleading users who want --stats percentile_stats:lineNumber.
      Evidence: docs/features.md:21 — "P50/P95/P99 percentiles for any numeric custom field"; src/stats/core.cpp:212-218 — PercentileStatsCollector::collect calls normalizeTargetFieldName(_fieldName); if normalized, calls extractFieldValue(entry, *normalized, ""); docs/cli-reference.md statistics row also says "FIELD may be a standard numeric field such as lineNumber or id, or any custom field key" — already correct.
      Surfaces: docs/features.md
      Development: In docs/features.md line 21, change "P50/P95/P99 percentiles for any numeric custom field" to "P50/P95/P99 percentiles for standard numeric fields (e.g., lineNumber, id) or any numeric custom field".
      Acceptance: grep "percentile\|P50" docs/features.md shows the updated wording; no tests break.
      Verification: grep "percentile\|P50" /opt/lxl/c++/logAnalyzer/docs/features.md

- [x] Route batch mode stats output to stderr (consistent with stream mode)
      Mode: develop
      Rationale: In batch mode, statistics are written to `*outputStream` (stdout or --output file) alongside log entries (src/main.cpp:505). In stream mode they go to std::cerr (different code path). This inconsistency means piping batch output through `jq` or `grep` sees the stats header mixed in with log JSON, breaking downstream tools. Routing batch stats to stderr (when --stats-output is not set) makes the two modes consistent and simplifies toolchain integration.
      Evidence: src/main.cpp:505 — `*outputStream << "\n--- Statistics ---\n";` (batch, no stats-output); src/main.cpp:363 (stream mode) — stats go to std::cerr; docs/cli-reference.md --stats-output row describes the flag but does not note the stderr routing difference.
      Surfaces: src/main.cpp, docs/cli-reference.md
      Development: In src/main.cpp, in the batch stats block (line 505), change `*outputStream` to `std::cerr` for the stats header and per-report lines when `cliOptions.statsOutputPath` is empty (the --stats-output path at line 495 already writes to a file, leave that unchanged). In docs/cli-reference.md, add a note to the --stats-output row: "When omitted, statistics are written to stderr (both batch and stream mode), so log entry output can be piped without interleaving."
      Acceptance: Running without --stats-output routes all stats lines to stderr; running with --stats-output still writes to the file; existing tests pass; piped output of batch mode contains only log entries.
      Verification: cmake --build build -j && ctest --test-dir build --output-on-failure

- [x] Extend getDedupKey to support all standard LogEntry fields (lineNumber, id, threadId, module, host, timestamp)
      Mode: develop
      Rationale: include/utils/dedup.h getDedupKey handles only level/message/source/source_file. Users can specify --dedup-field lineNumber or --dedup-field threadId but these fall through to the custom field lookup and produce __absent__N (treating every entry as unique) instead of using the actual LogEntry fields. normalizeTargetFieldName in stats/helpers.h already handles 9 standard fields — getDedupKey should have matching coverage.
      Evidence: include/utils/dedup.h:19-26 — getDedupKey checks level/message/source/source_file then falls to customFields.find; include/stats/helpers.h:normalizeTargetFieldName handles lineNumber/line_number/id/timestamp/thread_id/threadId/module/host; LogEntry fields: id (optional<size_t>), sourceLineNumber (optional<size_t>), timestamp (optional<tp>), threadId (optional<string>), module (optional<string>), host (optional<string>); docs/cli-reference.md --dedup-field row says "Standard fields: level, message, source".
      Surfaces: include/utils/dedup.h, docs/cli-reference.md
      Development: In getDedupKey (include/utils/dedup.h:19), add branches before the customFields.find fallback: `if (field == "id") return entry.id ? std::to_string(*entry.id) : "__absent__" + std::to_string(absentIdx++);` — same absent semantics. Similarly for line_number/lineNumber → entry.sourceLineNumber; thread_id/threadId → entry.threadId; module → entry.module; host → entry.host; timestamp → entry.timestamp (format via Utils::formatTimestamp). Update the applyDedupField comment (line 29-31) to list all supported standard fields.
      Acceptance: getDedupKey("id", entry_with_id) returns the id string; getDedupKey("lineNumber", entry_with_no_line) returns __absent__N (unique); --dedup-field threadId deduplicates by threadId in integration; utils_dedup green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^utils_dedup$" --output-on-failure

- [x] Add getDedupKey standard-field tests for lineNumber, id, threadId, host
      Mode: improve
      Rationale: tests/utils/dedup.cpp tests getDedupKey for level/message/custom/absent/source_alias but has no tests for the extended fields added by the preceding item (lineNumber, id, threadId, module, host). Without these tests a regression in the new branches goes undetected.
      Evidence: tests/utils/dedup.cpp — GetDedupKeyLevel/Message/Custom/Absent/SourceAlias exist; no test exercises the newly added standard-field paths; LogEntry has optional id/sourceLineNumber/threadId/module/host fields.
      Surfaces: tests/utils/dedup.cpp
      Development: In tests/utils/dedup.cpp add: GetDedupKeyId — entry.id=42, getDedupKey("id") == "42"; GetDedupKeyIdAbsent — entry.id absent, getDedupKey("id") returns __absent__N and increments idx; GetDedupKeyLineNumber — entry.sourceLineNumber=7, getDedupKey("line_number") == "7" and getDedupKey("lineNumber") == "7" (both aliases); GetDedupKeyThreadId — entry.threadId="t1", getDedupKey("thread_id") == "t1" and getDedupKey("threadId") == "t1"; GetDedupKeyHost — entry.host="srv-01", getDedupKey("host") == "srv-01". Use the existing makeEntry helper or extend it to accept id/threadId/host.
      Acceptance: All 5 new tests pass; no existing tests break; utils_dedup green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^utils_dedup$" --output-on-failure

- [x] Update docs/cli-reference.md --dedup-field row to list all supported standard fields
      Mode: docs
      Rationale: docs/cli-reference.md --dedup-field row says "Standard fields: level, message, source. Any other name is treated as a custom field key." After extending getDedupKey (preceding items) the supported standard fields are level/message/source/id/lineNumber/threadId/module/host/timestamp. The doc is now inaccurate and will mislead users who try --dedup-field threadId.
      Evidence: docs/cli-reference.md:110 — "Standard fields: `level`, `message`, `source`. Any other name is treated as a custom field key."; include/utils/dedup.h (after extension) supports 9 standard fields.
      Surfaces: docs/cli-reference.md
      Development: Update the --dedup-field description cell to: "Standard fields: `level`, `message`, `source` (alias: `source_file`), `id`, `lineNumber` (alias: `line_number`), `threadId` (alias: `thread_id`), `module`, `host`, `timestamp`. Any other name is treated as a custom field key."
      Acceptance: grep "dedup-field" docs/cli-reference.md shows the updated field list; no tests break.
      Verification: grep "dedup-field" /opt/lxl/c++/logAnalyzer/docs/cli-reference.md

- [x] Add --dedup-field and --count usage examples to docs/usage-examples.md
      Mode: docs
      Rationale: docs/usage-examples.md has no examples for --dedup-field or --count (or the --count --stats-output combination). These are non-obvious features that users discover through the CLI reference but benefit from seeing in context (e.g., "keep only first ERROR per source file", "count matching entries without output").
      Evidence: docs/usage-examples.md — scanning the file reveals no mention of --dedup-field or --count; docs/cli-reference.md:109-110 describes --dedup-field; docs/cli-reference.md:109 describes --count; --stats-output is also underdocumented with examples.
      Surfaces: docs/usage-examples.md
      Development: Add a new section or extend an existing one with: (1) `--dedup-field message` — deduplicate repeated messages, keep first occurrence per unique message; (2) `--dedup-field source` — keep only the first error per source file (combine with --level ERROR); (3) `--count` — print the count of matching entries like grep -c; (4) `--count --stats-output stats.json` — count matching entries AND write statistics to a file. Each example should include a realistic command line and a one-line description of the output.
      Acceptance: grep "dedup-field\|--count" docs/usage-examples.md shows the new examples; no tests break.
      Verification: grep "dedup-field\|--count" /opt/lxl/c++/logAnalyzer/docs/usage-examples.md
