# planwright Plan — .
<!-- Session: 2026-06-04T00:00:00Z -->

- [ ] Enforce --dedup-field in stream path
      Mode: repair
      Rationale: --dedup-field is registered in CLI and applied in the batch path (src/main.cpp), but the streamEntryCallback has no reference to cliOptions.dedupField — a user doing --stream --dedup-field message silently gets all entries, not deduplicated ones. Same defect pattern as --offset before Cycle 3 fix.
      Evidence: src/main.cpp grep shows cliOptions.dedupField appears only in the batch path (lines 358-385); streamEntryCallback (lines 241-306) has no seen-set logic; the stream path ends at line 316 with no dedup step.
      Surfaces: src/main.cpp
      Development: Before streamEntryCallback declaration add: std::unordered_set<std::string> streamDedupSeen; size_t streamDedupIdx = 0; Inside the callback, after the filter check and offset skip but before the limit check, add the same key-resolution logic as the batch path: if (!cliOptions.dedupField.empty()) { resolve key; if (!streamDedupSeen.insert(key).second) return true; } This skips duplicates without counting them toward limit.
      Acceptance: --stream --dedup-field message keeps only first match per unique message; --stream --dedup-field level keeps first per level; absent custom field treats each as unique; --stream --dedup-field message --limit 2 respects both constraints.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_filtering$" --output-on-failure

- [ ] Fix --stats --stream silent no-op
      Mode: repair
      Rationale: When --stats is combined with --stream, the statistics block (src/main.cpp:459) is inside the batch path's else branch and is never reached. The user gets no stats output and no warning. The statisticsEnabled flag is correctly computed before the stream/batch branch, so the fix is to wire stats collection into the stream path.
      Evidence: src/main.cpp:185 starts the stream path; src/main.cpp:317 starts the else (batch) path; the statisticsEnabled block at line 459 is inside the batch branch; grep confirms no call to processEntryForStatistics or getAllStatisticReports inside the stream path (lines 185-316).
      Surfaces: src/main.cpp
      Development: In streamEntryCallback, after incrementing streamMatchCount and before the output dispatch, add: if (statisticsEnabled) { analyzer.processEntryForStatistics(entry); } After analyzeStream completes (after the countOnly block at line 313), add: if (statisticsEnabled) { auto reports = analyzer.getAllStatisticReports(); if (!cliOptions.statsOutputPath.empty()) { std::ofstream statsFile(cliOptions.statsOutputPath); for (const auto& rp : reports) statsFile << rp.second.dump(cliOptions.prettyPrint ? 4 : -1) << '\n'; } else { *outputStream << "\n--- Statistics ---\n"; for (const auto& rp : reports) *outputStream << rp.second.dump(cliOptions.prettyPrint ? 4 : -1) << '\n'; } }
      Acceptance: --stream --stats unique_messages outputs the stats block after all entries; --stream --stats unique_messages --stats-output stats.json writes stats to file; --stream without --stats is unchanged.
      Verification: cmake --build build -j && ctest --test-dir build --output-on-failure

- [ ] Add MOVING_AVERAGE_RATE bare-name and KV-form CLI --stats parsing tests
      Mode: improve
      Rationale: tests/config/cli/analysis.cpp has bare-name and KV-form tests for TIME_BUCKET_HISTOGRAM and PERCENTILE_STATS (added cycles 2-3), but no test for MOVING_AVERAGE_RATE added in Cycle 3. The bare-name path ("moving_average_rate" → stringToStatisticType) and KV form ("type=moving_average_rate,bucket=30") are completely untested at the CLI parse layer.
      Evidence: grep of tests/config/cli/analysis.cpp shows no "moving_average" or "MOVING_AVERAGE" reference; src/utils/core.cpp:175 maps "MOVING_AVERAGE_RATE" in stringToStatisticType; src/config/cli_helpers.cpp generic KV parser handles it via the type= key.
      Surfaces: tests/config/cli/analysis.cpp
      Development: Add MovingAverageRateStatBareName: parse {"--stats","moving_average_rate"}, assert statisticConfigs[0].type==MOVING_AVERAGE_RATE and params does not contain "bucket". Add MovingAverageRateStatKVForm: parse {"--stats","type=moving_average_rate,bucket=30"}, assert type==MOVING_AVERAGE_RATE and params.at("bucket")=="30".
      Acceptance: Both tests compile and pass; config_cli_analysis suite stays green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_analysis$" --output-on-failure

- [ ] Add MovingAverageRateCollector reset test
      Mode: improve
      Rationale: tests/stats/core.cpp has PercentileStatsCollectorReset (cycle 3) but no analogous reset test for MovingAverageRateCollector. The reset() method clears both _counts and resets _total to 0; if this invariant breaks (e.g., _total not reset), subsequent generateReport() would produce wrong mean_per_bucket and total_entries.
      Evidence: tests/stats/core.cpp grep shows no test calls reset() on MovingAverageRateCollector; MovingAverageRateCollector::reset() in include/stats/core.h is { _counts.clear(); _total = 0; } — both fields must be cleared but only one is exercised per test invocation.
      Surfaces: tests/stats/core.cpp
      Development: Add MovingAverageRateCollectorReset: create collector(10), collect one entry with timestamp, call reset(), collect a different entry in a different bucket, assert bucket_count==1 and total_entries==1 and mean_per_bucket==1.0 — verifying that pre-reset data is gone.
      Acceptance: Test compiles and passes; stats_core suite stays green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [ ] Extract and unit-test applyDedupField helper
      Mode: improve
      Rationale: The --dedup-field logic in src/main.cpp (lines 358-385) is inline in main() and untestable — only the CLI parse path (options.dedupField=="session_id") is verified. Extracting applyDedupField(vector<LogEntry>&, string_view) into a small helper (in src/main.cpp or a new utils file) lets us add focused unit tests for the three key behaviors: standard-field dedup (message), custom-field dedup, and absent-custom-field uniqueness.
      Evidence: tests/config/cli/filtering.cpp DedupFieldOption only tests parse; the actual std::remove_if+unordered_set logic is never verified by any test; the absent-field branch uses a per-entry index counter that could silently be wrong if dedupIdx is declared wrong.
      Surfaces: src/main.cpp, tests/config/cli/filtering.cpp
      Development: Extract the dedup logic from main() into a static helper function applyDedupField(std::vector<LogEntry>& entries, std::string_view field) defined before main(). In its implementation, use the same key resolution and unordered_set logic currently inline. Add three tests in tests/config/cli/filtering.cpp (or a new tests/utils/dedup.cpp if CMakeLists.txt needs a new target): DedupFieldByMessage (5 entries, 2 unique messages → 2 kept), DedupFieldByCustomField (entries with and without the field → unique kept), DedupFieldAbsentFieldKeepsAll (all entries missing the custom field → all kept as unique).
      Acceptance: Extracted function produces identical output; dedup tests pass; no regression in config_cli_filtering.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_filtering$" --output-on-failure

- [ ] Add moving_average_rate:N colon shorthand to parseStatisticConfig
      Mode: develop
      Rationale: top_messages:N and percentile_stats:field already have colon shorthands (src/config/cli_helpers.cpp). MOVING_AVERAGE_RATE requires only a bucket size parameter (default 60s), making --stats moving_average_rate:30 a natural shorthand for bucket=30 analogous to top_messages:5. There is no rfind("moving_average_rate:", 0) branch in parseStatisticConfig.
      Evidence: src/config/cli_helpers.cpp:29 top_messages: branch; src/config/cli_helpers.cpp:42 percentile_stats: branch; no moving_average_rate: branch exists; --stats "type=moving_average_rate,bucket=30" is the only current path.
      Surfaces: src/config/cli_helpers.cpp, tests/config/cli/analysis.cpp
      Development: In parseStatisticConfig, after the percentile_stats: check, add: if (normalizedLower.rfind("moving_average_rate:", 0) == 0) { std::string bucketStr = normalized.substr(20); trimInPlace(bucketStr); if (bucketStr.empty() || !std::all_of(bucketStr.begin(), bucketStr.end(), [](unsigned char c){ return std::isdigit(c) != 0; })) return std::nullopt; config.type = StatisticType::MOVING_AVERAGE_RATE; config.params["bucket"] = bucketStr; return config; } Add test MovingAverageRateColonShorthand: parse {"--stats","moving_average_rate:30"}, assert type==MOVING_AVERAGE_RATE and params["bucket"]=="30". Add test MovingAverageRateColonShorthandEmptyBucketFails: parse {"--stats","moving_average_rate:"}, assert !result.has_value() and error code==InvalidCLIOption.
      Acceptance: --stats moving_average_rate:30 produces correct StatisticConfig; empty bucket fails; existing tests green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_analysis$" --output-on-failure

- [ ] Update docs/cli-reference.md for Cycle 1-3 features
      Mode: docs
      Rationale: docs/cli-reference.md predates Cycles 1-3. Seven new CLI options and updated --format/--stats enumerations are missing. A user reading the reference cannot discover --offset, --count, --limit, --since, --dedup-field, --stats-output, or that NDJSON is a format option; they also cannot see time_bucket_histogram, percentile_stats, or moving_average_rate in the --stats description.
      Evidence: grep of docs/cli-reference.md for "offset", "count", "since", "dedup", "stats-output", "ndjson", "moving_average", "percentile" returns nothing; --format row still lists only text/json/csv/xml; --stats NAME description omits time_bucket_histogram, percentile_stats, moving_average_rate.
      Surfaces: docs/cli-reference.md
      Development: In Output & Export table: update --format row to list ndjson alongside text/json/csv/xml. Add rows for --limit N, --offset N, --count, --stats-output PATH. In Filtering table: add --since DURATION row. In Output & Export or a new "Entry Processing" section: add --dedup-field FIELD row. In Statistics table: update --stats NAME description to add time_bucket_histogram[:N], percentile_stats[:FIELD], moving_average_rate[:BUCKET_SECONDS].
      Acceptance: All seven new options appear in the reference; --format list includes ndjson; --stats list includes the three new collectors; no existing rows removed or broken.
      Verification: cmake --build build -j --output-on-failure
