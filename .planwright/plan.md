# planwright Plan — .
<!-- Session: 2026-06-05T00:00:00Z -->

- [ ] Add --max-level to docs/cli-reference.md Filtering table and update features.md
      Mode: docs
      Rationale: --max-level was implemented in Cycle 10 but the CLI reference still has no --max-level row; docs/features.md line 9 "Keyword & Regex Filtering" row also omits range filtering. Users relying on docs to discover options will not find the feature.
      Evidence: docs/cli-reference.md:50 — has --min-level row but no --max-level row; grep for "max.level" in docs/cli-reference.md returns nothing; docs/features.md:9 — says "Filter by log level, keywords" with no mention of --min-level or --max-level; include/config/cli.h now has maxLogLevel field; src/config/cli.cpp has the --max-level registration; tests pass.
      Surfaces: docs/cli-reference.md, docs/features.md
      Development: In docs/cli-reference.md, insert a `--max-level LEVEL` row immediately after the `--min-level LEVEL` row (line 50), describing it as "Includes log entries with a level equal to or less severe than the specified level (e.g., `WARNING` will include `TRACE`, `DEBUG`, `INFO`, `WARNING`). Case-insensitive." In docs/features.md line 9, update the "Keyword & Regex Filtering" row description to mention level range filtering: add "Supports `--min-level` / `--max-level` for severity range filtering (inclusive)."
      Acceptance: grep -n "max-level" docs/cli-reference.md shows the new row; grep "max-level\|min-level" docs/features.md shows the updated row; no existing test breaks.
      Verification: grep -n "max-level" /opt/lxl/c++/logAnalyzer/docs/cli-reference.md && grep "min-level\|max-level" /opt/lxl/c++/logAnalyzer/docs/features.md

- [ ] Add MaxLevelFilterTest unit test to tests/filter/core/unit/basic.cpp
      Mode: improve
      Rationale: tests/filter/core/unit/basic.cpp has a MinLevelFilterTest (line 123) that directly exercises MinLevelFilter::matches, but MaxLevelFilter was added in Cycle 10 without a symmetric filter-level unit test — its behavior (entry.level <= maxLevel_) is only indirectly covered by the CLI option test.
      Evidence: tests/filter/core/unit/basic.cpp:123-135 — MinLevelFilterTest creates MinLevelFilter(WARNING) and asserts FATAL/ERROR/WARNING pass, INFO/DEBUG fail; no MaxLevelFilterTest exists; tests/config/cli/filtering.cpp:226 MaxLevel only confirms the CLI option sets maxLogLevel.has_value() but never exercises the filter's matches() method.
      Surfaces: tests/filter/core/unit/basic.cpp
      Development: After the MinLevelFilterTest block (line 135), add TEST_F(FilterTest, MaxLevelFilterTest) creating MaxLevelFilter(WARNING) and asserting entry_trace/debug/info/warning pass (level <= WARNING), and entry_error/critical/fatal fail (level > WARNING) — mirror the structure of MinLevelFilterTest.
      Acceptance: MaxLevelFilterTest covers all 7 LogLevel values; filter_core_unit_basic green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^filter_core_unit_basic$" --output-on-failure

- [ ] Add MaxLevelInvalid and MinLevelMaxLevelRange CLI option tests
      Mode: improve
      Rationale: tests/config/cli/filtering.cpp has InvalidLogLevel (for --level INVALID) but no parallel for --max-level INVALID; it also tests --min-level and --max-level individually but never together, leaving the AND-combination (range filter) behavior unconfirmed.
      Evidence: tests/config/cli/filtering.cpp:28 InvalidLogLevel tests "--level INVALID" → Code::InvalidCLIOption; MaxLevel (line 226) and MinLevel (line 210) test valid values only; no MinLevelMaxLevelRange test verifying both options are set simultaneously; the filterLevels AND logic that combines them is what users actually rely on.
      Surfaces: tests/config/cli/filtering.cpp
      Development: Add TEST_F(CLIConfigTest, MaxLevelInvalid) parsing "--max-level INVALID" asserting !result.has_value() with Code::InvalidCLIOption — mirror InvalidLogLevel. Add TEST_F(CLIConfigTest, MinLevelMaxLevelRange) parsing "--min-level INFO --max-level WARNING" asserting both minLogLevel=INFO and maxLogLevel=WARNING are set.
      Acceptance: MaxLevelInvalid rejects, MinLevelMaxLevelRange parses both correctly; config_cli_filtering green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_filtering$" --output-on-failure

- [ ] Add TopNFieldValuesCollectorForId and ValidateStatisticConfig_TopNFieldValuesIdAccepted tests
      Mode: improve
      Rationale: FieldValueCountCollectorForId tests the id field for FieldValueCountCollector; both collector types use stats::detail::extractFieldValue via getFieldValueAsString, but TopNFieldValuesCollector's id path (including its top-N sort-and-truncate logic) is untested. ValidateStatisticConfig_FieldValueCountIdAccepted exists but no parallel for TOP_N_FIELD_VALUES.
      Evidence: tests/stats/core.cpp — FieldValueCountCollectorForId added in C10; TopNFieldValuesCollector tests exist for message/thread_id but not for id field; tests/config/core/validation.cpp — ValidateStatisticConfig_FieldValueCountIdAccepted exists; TOP_N_FIELD_VALUES with target_field=id untested.
      Surfaces: tests/stats/core.cpp, tests/config/core/validation.cpp
      Development: In tests/stats/core.cpp add TEST_F(StatisticsTest, TopNFieldValuesCollectorForId): create TopNFieldValuesCollector(2, "id"), collect 4 entries with ids {10, 10, 20, 30} (entry with no id skipped), assert report["values"][0]["value"]=="10" (most frequent, count 2); assert only 2 values returned (top_n=2). In tests/config/core/validation.cpp add ValidateStatisticConfig_TopNFieldValuesIdAccepted: FIELD_VALUE_COUNT-like config with TOP_N_FIELD_VALUES type, target_field=id, top_n=5, assert errors.empty().
      Acceptance: Both new tests pass; existing tests unaffected; stats_core and config_core_validation green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$|^config_core_validation$" --output-on-failure

- [ ] Add case-insensitive id field tests to config/core/validation
      Mode: improve
      Rationale: normalizeTargetFieldName lowercases the input before matching, so "ID", "Id", "iD" all map to "id" — but no test verifies this for the newly added id field specifically (existing case-insensitive tests only cover "LeVeL" and similar standard fields).
      Evidence: include/stats/helpers.h:43-55 — std::transform to lowercase then if (field == "id") return "id"; tests/config/core/validation.cpp:172 — ValidateStatisticConfig_TopNFieldValues_CaseInsensitiveTargetField uses "LeVeL" to verify lowercasing; no test with "ID" or "Id" in target_field.
      Surfaces: tests/config/core/validation.cpp
      Development: In tests/config/core/validation.cpp add TEST_F(ConfigValidationTest, ValidateStatisticConfig_FieldValueCountIdCaseInsensitive): pass target_field="ID" (uppercase) for FIELD_VALUE_COUNT, assert errors.empty(). Add a second case with "Id" (mixed) in the same test or a separate test. Mirror the ValidateStatisticConfig_TopNFieldValues_CaseInsensitiveTargetField pattern.
      Acceptance: target_field="ID" and target_field="Id" both pass validation; config_core_validation green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_core_validation$" --output-on-failure

- [ ] Extend PercentileStatsCollector to support standard LogEntry numeric fields
      Mode: develop
      Rationale: PercentileStatsCollector::collect only looks up customFields by key; extractFieldValue in helpers.h supports lineNumber (std::to_string(*entry.sourceLineNumber)), id (std::to_string(*entry.id)), timestamp (formatted string), and other standard fields — any numeric-valued standard field (lineNumber, id) can produce meaningful percentile results. Users cannot do --stats percentile_stats:lineNumber today.
      Evidence: src/stats/core.cpp:212-218 — collect does entry.customFields.find(_fieldName) only; stats/helpers.h extractFieldValue handles all LogEntry fields; include/stats/core.h:213 comment says "named numeric custom field"; docs/cli-reference.md stats table does not clarify field scope; --stats percentile_stats:lineNumber would silently produce zero-count output today.
      Surfaces: src/stats/core.cpp, include/stats/core.h, tests/stats/core.cpp, docs/cli-reference.md
      Development: In src/stats/core.cpp PercentileStatsCollector::collect, replace the bare customFields.find with: (1) call normalizeTargetFieldName(_fieldName); (2) if it resolves, call extractFieldValue(entry, *normalized, "") for valueStr; (3) else fall through to customFields.find(_fieldName) as before; (4) if valueStr.empty() return; (5) stod as before. Update include/stats/core.h line 213 comment from "named numeric custom field" to "named numeric field (standard LogEntry field or custom field key)". In tests/stats/core.cpp add PercentileStatsCollectorForLineNumber: create collector("lineNumber") or ("line_number" alias), collect fixture entries (sourceLineNumber 1-7), assert p50 ≈ 4. Update docs/cli-reference.md percentile_stats row to mention standard field support.
      Acceptance: --stats percentile_stats:lineNumber collects line number values; --stats percentile_stats:latency_ms still works (custom field); non-numeric fields silently produce zero samples; stats_core green; docs updated.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [ ] Add MinLevelInvalid test parallel to MaxLevelInvalid
      Mode: improve
      Rationale: InvalidLogLevel tests --level INVALID and MaxLevelInvalid (from item 3) tests --max-level INVALID; --min-level also uses the same CLI11 CheckedTransformer and should have its own rejection test for completeness, but none exists.
      Evidence: tests/config/cli/filtering.cpp — InvalidLogLevel (line 28) covers --level; no MinLevelInvalid test; MaxLevelInvalid is being added by the preceding plan item; --min-level uses transform(CLI::CheckedTransformer(Config::LogLevelMap, CLI::ignore_case)) same as --max-level.
      Surfaces: tests/config/cli/filtering.cpp
      Development: In tests/config/cli/filtering.cpp add TEST_F(CLIConfigTest, MinLevelInvalid) parsing "--min-level INVALID" asserting !result.has_value() with Code::InvalidCLIOption — single test, mirrors MaxLevelInvalid. This can be added in the same editing pass as the MaxLevelInvalid test (item 3) if execution order allows, or as its own small commit.
      Acceptance: MinLevelInvalid rejects with InvalidCLIOption; config_cli_filtering green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_filtering$" --output-on-failure

- [ ] Add PercentileStatsCollectorReset test
      Mode: improve
      Rationale: Every IStatisticCollector has a reset() method; stats/core.h line 220 defines PercentileStatsCollector::reset() as { _values.clear(); }; no test verifies that reset() discards accumulated samples so the collector can be reused (e.g., in --stats-interval windows) — only the ENTRY_RATE reset test and some MovingAverage tests cover this pattern.
      Evidence: tests/stats/core.cpp — scanning existing reset tests: MovingAverageRateCollectorReset exists; no PercentileStatsCollectorReset test; the pattern is: collect some entries, call reset(), re-collect, assert report reflects only post-reset entries.
      Surfaces: tests/stats/core.cpp
      Development: In tests/stats/core.cpp add TEST_F(StatisticsTest, PercentileStatsCollectorReset): create PercentileStatsCollector("latency_ms"), collect one entry with latency_ms="100", call reset(), collect one entry with latency_ms="200", call generateReport(), assert count==1 and p50≈200 (only post-reset sample counted) — mirror the MovingAverageRateCollectorReset pattern.
      Acceptance: PercentileStatsCollectorReset passes; stats_core green; existing percentile tests unaffected.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure
