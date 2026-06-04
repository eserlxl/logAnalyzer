# planwright Plan — .
<!-- Session: 2026-06-05T00:00:00Z -->

- [ ] Fix --order DESC + --stream warning to also cover sort-order
      Mode: repair
      Rationale: The stream sort warning added in Cycle 6 checks only sortBy != TIMESTAMP, missing the case where a user passes --stream --order desc (descending timestamp). In batch mode, both non-default sortBy and non-default sortOrder trigger a sort; stream mode silently ignores both. The warning was already broadened in implementation (checking nonDefaultSortOrder), but needs a test verifying --stream --order desc is parsed and the combination is accepted (warning-only, not error).
      Evidence: src/main.cpp line 192: nonDefaultSortBy and nonDefaultSortOrder both checked; tests/config/cli/analysis.cpp has StreamWithSortByParsesSuccessfully for --sort-by but no test for --order desc + --stream.
      Surfaces: tests/config/cli/analysis.cpp
      Development: Add StreamWithOrderDescParsesSuccessfully: parse {"log_analyzer", "dummy_log_file.log", "--stream", "--order", "desc"}, assert result.has_value() and options.streamMode and options.sortOrder == SortOrder::DESCENDING.
      Acceptance: Test passes; config_cli_analysis green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_analysis$" --output-on-failure

- [ ] Add find_gaps bare-name CLI test
      Mode: improve
      Rationale: tests/config/cli/analysis.cpp tests the colon shorthand (find_gaps:2000) and the empty-threshold failure (find_gaps:), but not the bare name (--stats find_gaps or --stats FIND_GAPS). The bare name falls through to Utils::stringToStatisticType, which uppercases and maps it to FIND_GAPS — but this code path is untested.
      Evidence: grep of tests/config/cli/analysis.cpp for "find_gaps" shows only colon-form tests; stringToStatisticType("find_gaps") → FIND_GAPS via toUpperInPlaceAsciiSafe is untested in the CLI test suite.
      Surfaces: tests/config/cli/analysis.cpp
      Development: Add FindGapsBareName: parse {"log_analyzer", "dummy_log_file.log", "--stats", "find_gaps"}, assert result.has_value(), settings.statisticConfigs[0].type == FIND_GAPS, and params has no threshold_ms (factory uses default 1000ms).
      Acceptance: Test passes; config_cli_analysis green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_analysis$" --output-on-failure

- [ ] Add MovingAverageRate invalid-bucket fallback test
      Mode: improve
      Rationale: TimeBucketHistogramCollector has CreateCollectorFactoryTimeBucketHistogramInvalidBucketFallsBackToDefault (tests/stats/core.cpp:300) testing bucket=0 → default. MovingAverageRateCollector has the same guard (bucketSeconds > 0 ? bucketSeconds : 60) but no parallel test. The pattern is incomplete.
      Evidence: tests/stats/core.cpp:300 tests TIME_BUCKET_HISTOGRAM invalid bucket; grep finds no "MovingAverageRate.*Invalid\|Invalid.*MovingAverageRate" test; src/stats/core.cpp:378 has the guard.
      Surfaces: tests/stats/core.cpp
      Development: Add CreateCollectorFactoryMovingAverageRateInvalidBucketFallsBackToDefault: config.params["bucket"]="0", create collector, assert non-null, assert report["bucket_seconds"] == 60.
      Acceptance: Test passes; stats_core green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [ ] Add GapDetector exact-threshold boundary test
      Mode: improve
      Rationale: GapDetectorCollector::generateReport() uses gapMs > _thresholdMs (strictly greater). A gap exactly equal to the threshold is NOT reported. This boundary behavior is undocumented and untested — a future maintainer might change > to >= thinking it is a bug.
      Evidence: src/stats/core.cpp:363 if (gapMs > _thresholdMs); no existing test with gapMs == threshold; documentation says "longer than" (threshold is exclusive lower bound).
      Surfaces: tests/stats/core.cpp
      Development: Add GapDetectorCollectorExactlyAtThresholdNotReported: 2 entries with gap = threshold (e.g. threshold=100ms, gap=100ms), assert gap_count==0.
      Acceptance: Test passes; stats_core green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [ ] Add round-trip tests for remaining StatisticTypes
      Mode: improve
      Rationale: tests/stats/core.cpp has round-trip tests for TIME_BUCKET_HISTOGRAM, PERCENTILE_STATS, MOVING_AVERAGE_RATE, FIND_GAPS, ENTRY_RATE. Missing: UNIQUE_MESSAGES, TOP_MESSAGES, LOG_LEVEL_COUNT, FIELD_VALUE_COUNT, TOP_N_FIELD_VALUES. If any of these are accidentally renamed in statisticTypeToString, the regression would only appear at JSON config deserialization, not in any focused unit test.
      Evidence: grep of tests/stats/core.cpp for "RoundTrip" shows 5 tests; 5 types have none.
      Surfaces: tests/stats/core.cpp
      Development: Add UniqueMessagesRoundTrip, TopMessagesRoundTrip, LogLevelCountRoundTrip, FieldValueCountRoundTrip, TopNFieldValuesRoundTrip — each asserts statisticTypeToString returns expected string and stringToStatisticType returns the same type.
      Acceptance: All 5 tests pass; stats_core green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [ ] Update README.md to mention NDJSON, pagination, dedup, gap detection
      Mode: docs
      Rationale: README.md Features section (line 37-47) still says "export to JSON, CSV, XML, or text" (missing NDJSON), "frequency counts and value distributions" (missing gap detection, percentiles, moving average rate), and has no mention of pagination (--limit/--offset/--count) or deduplication (--dedup-field). The README is the first page users see; it must reflect the project's current capabilities.
      Evidence: README.md line 39: "JSON, CSV, XML, or text" — no NDJSON; line 40: "frequency counts and value distributions" — no gap detection or percentiles; no bullet for pagination or dedup anywhere.
      Surfaces: README.md
      Development: Update line 39: add "NDJSON (newline-delimited)". Update line 40: expand to "time-bucket histograms, percentile stats (P50/P95/P99), gap detection, and moving average rate". Add two bullets: "**Pagination & Deduplication**: --limit, --offset, --count for cursor-style pagination; --dedup-field removes duplicate values." and "**Stream Statistics**: --stats works in --stream mode, including gap detection and histograms."
      Acceptance: Features section in README.md reflects current capabilities; build passes.
      Verification: cmake --build build -j --output-on-failure
