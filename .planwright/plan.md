# planwright Plan — .
<!-- Session: 2026-06-04T00:00:00Z -->

- [ ] Implement GapDetectorCollector to fix --find-gaps no-op
      Mode: repair
      Rationale: --find-gaps MS is a registered CLI option (src/config/cli.cpp:219), its value is stored in CLIOptions.findGapsDuration (include/config/cli.h:67), but it is never consumed: grep of src/main.cpp shows no reference to findGapsDuration and cli.cpp:316 comments "No FIND_GAPS... So I leave it as is." A user passing --find-gaps 5000 gets no gap report, no warning, nothing. The TimeGap struct in include/core/log/types.h exists but is never instantiated.
      Evidence: grep of src/ for "findGapsDuration" shows only cli.cpp:317 setting it; src/main.cpp has no findGapsDuration reference; include/stats/core.h has no FIND_GAPS type; tests pass because nothing tests the behavior.
      Surfaces: include/stats/core.h, src/stats/core.cpp, src/stats/analyzer.cpp, src/utils/core.cpp, src/config/cli.cpp
      Development: Add FIND_GAPS to StatisticType enum. Add GapDetectorCollector: private _thresholdMs (long long), _timestamps (vector<system_clock::time_point>). collect() appends timestamp. generateReport() sorts _timestamps, scans adjacent pairs for gap > _thresholdMs, emits {"name":"gap_detector","threshold_ms":N,"gap_count":K,"gaps":[{"start":ISO,"end":ISO,"duration_ms":M},...]}. Add to statisticTypeToString/stringToStatisticType. Add factory case with threshold_ms param. In cli.cpp, after the gapDurationMs block at line 317, if gapDurationMs > 0, push a StatisticConfig with type=FIND_GAPS and params["threshold_ms"]=to_string(gapDurationMs). Existing findGapsDuration assignment stays for legacy compat.
      Acceptance: --find-gaps 100 on a log with a 200ms gap emits a gap report; --find-gaps on a log with no gaps emits gap_count:0; factory with threshold_ms param works; stats_core green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [ ] Add GapDetectorCollector unit tests
      Mode: improve
      Rationale: The new GapDetectorCollector (Cycle 5 item 1) needs focused tests analogous to TimeBucketHistogramCollector: detect a gap that exceeds threshold, skip a gap below threshold, handle entries without timestamps, empty collector, reset, round-trip, factory.
      Evidence: Item 1 is pre-requisite; once implemented tests/stats/core.cpp will have no coverage for GapDetectorCollector behavior.
      Surfaces: tests/stats/core.cpp
      Development: Add GapDetectorCollectorDetectsGap: 3 entries at t=0,t=50ms,t=300ms with threshold=100ms → gap_count=1, gap duration_ms=250. Add GapDetectorCollectorSkipsBelowThreshold: 3 entries at t=0,t=50ms,t=80ms with threshold=100ms → gap_count=0. Add GapDetectorCollectorSkipsNoTimestamp: one entry without timestamp → gap_count=0 (or no report entry). Add GapDetectorCollectorEmpty: no entries → gap_count=0. Add GapDetectorCollectorReset: collect two entries, reset, collect two different entries, assert only second pair considered. Add GapDetectorRoundTrip: statisticTypeToString/stringToStatisticType round-trip. Add factory test with threshold_ms=200.
      Acceptance: All 7 tests compile and pass; stats_core green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [ ] Fix EntryRateCollector inconsistent duration_sec output schema
      Mode: repair
      Rationale: EntryRateCollector::generateReport() (src/stats/core.cpp:116) emits duration_sec:"0" (a JSON string) in the <2-entries path but omits duration_sec entirely in the normal path (the computation at line 128 stores secs as long long but never writes report["duration_sec"] in the success branch). Any consumer parsing the report will see a field present sometimes as a string, absent otherwise. This is a schema inconsistency.
      Evidence: src/stats/core.cpp:122 writes report["duration_sec"] = "0"; no report["duration_sec"] assignment exists in the else/success branch (lines 127-134); the success branch only writes average_rate_per_sec and total_entries.
      Surfaces: src/stats/core.cpp
      Development: In generateReport(): change the <2-entries path to report["duration_sec"] = 0 (integer, not string). In the success branch, after computing secs, add report["duration_sec"] = secs to emit the actual duration. This makes duration_sec a consistently present integer field.
      Acceptance: generateReport() always emits duration_sec as an integer; the <2-entries case emits 0 (integer); the normal case emits the actual seconds; EntryRateCollector tests (item 4) confirm this.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [ ] Add EntryRateCollector direct unit tests
      Mode: improve
      Rationale: tests/stats/core.cpp has 40+ tests but zero for EntryRateCollector logic — only utils/enum.cpp covers the round-trip and a factory test exists in CreateCollectorFactory. The collect→generateReport pipeline (timestamp accumulation, sort, average rate computation, duration) is completely untested by focused assertions.
      Evidence: grep of tests/stats/core.cpp for "EntryRate" or "entry_rate" returns nothing; src/stats/core.cpp:116 EntryRateCollector::generateReport is a non-trivial computation (sort + duration + division) with a special <2-entries branch that has never been directly exercised by a test that inspects the output JSON.
      Surfaces: tests/stats/core.cpp
      Development: Add EntryRateCollectorBasic: collect 4 entries spanning 4 seconds (at t, t+1s, t+2s, t+4s), assert average_rate_per_sec≈1.0 (4 entries / 4 seconds), total_entries==4, duration_sec==4. Add EntryRateCollectorSingleEntry: collect 1 entry, assert average_rate_per_sec==0 and total_entries==1 and duration_sec==0. Add EntryRateCollectorReset: collect 3 entries, call reset(), collect 2 entries, assert total_entries==2. Add EntryRateRoundTrip: statisticTypeToString/stringToStatisticType for ENTRY_RATE. Add CreateCollectorFactoryEntryRate: factory creates non-null collector.
      Acceptance: All 5 tests compile and pass; stats_core green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure

- [ ] Add find_gaps:N colon shorthand and update --find-gaps CLI test
      Mode: develop
      Rationale: The existing --find-gaps CLI test (tests/config/cli/analysis.cpp:99) only asserts options.findGapsDuration==5000ms; after item 1 fixes the no-op, it should also assert statisticConfigs[0].type==FIND_GAPS. Additionally, as with top_messages:N, percentile_stats:FIELD, and moving_average_rate:N, a --stats find_gaps:5000 colon shorthand is the natural parallel for specifying the threshold via the --stats path rather than the legacy --find-gaps.
      Evidence: src/config/cli_helpers.cpp has top_messages:, percentile_stats:, moving_average_rate: branches but no find_gaps: branch; tests/config/cli/analysis.cpp:99 FindGapsDuration test does not check statisticConfigs after item 1 maps findGapsDuration to a StatisticConfig.
      Surfaces: src/config/cli_helpers.cpp, tests/config/cli/analysis.cpp
      Development: In parseStatisticConfig after the moving_average_rate: block, add: if (normalizedLower.rfind("find_gaps:", 0) == 0) { string threshStr = normalized.substr(10); trimInPlace(threshStr); if (threshStr.empty() || !all_of digits) return nullopt; config.type = StatisticType::FIND_GAPS; config.params["threshold_ms"] = threshStr; return config; } Update FindGapsDuration test: add ASSERT for statisticConfigs[0].type==FIND_GAPS and params["threshold_ms"]=="5000". Add FindGapsColonShorthand: parse {"--stats","find_gaps:2000"}, assert type==FIND_GAPS and params["threshold_ms"]=="2000". Add FindGapsColonShorthandEmptyThresholdFails.
      Acceptance: --find-gaps 5000 creates StatisticConfig(FIND_GAPS, threshold_ms=5000); --stats find_gaps:2000 creates same; config_cli_analysis green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_analysis$" --output-on-failure

- [ ] Extract getDedupKey helper to unify stream and batch dedup logic
      Mode: improve
      Rationale: src/main.cpp has two copies of the key-resolution logic for --dedup-field: one in the batch path (delegated to Utils::applyDedupField in include/utils/dedup.h) and one inline in the stream path (lines 265-280 of main.cpp). They can diverge silently if one is updated. Extracting a getDedupKey(entry, field, idx&) free function into include/utils/dedup.h lets both paths share one implementation, and the existing utils_dedup tests already cover the key-resolution logic transitively, but a focused getDedupKey unit test removes the indirect dependency.
      Evidence: src/main.cpp stream callback (lines 248-272) duplicates the if/else key-resolution logic from include/utils/dedup.h:27-42; the two copies can diverge; no unit test directly exercises getDedupKey in isolation; tests/utils/dedup.cpp tests applyDedupField (which includes key resolution internally).
      Surfaces: include/utils/dedup.h, src/main.cpp, tests/utils/dedup.cpp
      Development: In include/utils/dedup.h add inline std::string getDedupKey(const LogEntry& entry, const std::string& field, size_t& idx) that implements the level/message/source/custom/absent logic. Update applyDedupField to call getDedupKey. Update src/main.cpp stream path to call Utils::getDedupKey(entry, cliOptions.dedupField, streamDedupIdx) and insert into streamDedupSeen. Add test GetDedupKeyLevel/GetDedupKeyMessage/GetDedupKeyCustom/GetDedupKeyAbsent in tests/utils/dedup.cpp.
      Acceptance: Stream and batch paths share getDedupKey; existing dedup tests still pass; 4 new direct tests pass; utils_dedup green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^utils_dedup$" --output-on-failure

- [ ] Update docs/features.md and cli-reference.md for gap detection and stats-in-stream
      Mode: docs
      Rationale: docs/features.md Statistical Analysis row omits gap detection (now functional after item 1). docs/cli-reference.md Statistics table shows --find-gaps as "Deprecated" which is misleading — after item 1 it works via GapDetectorCollector. Also stats-in-stream is a new behavior (Cycle 4 item 2) not yet documented: --stats in stream mode now produces output.
      Evidence: docs/features.md line 21 lists "entry rates, top messages, log level counts, unique value counts, time-bucket histograms, P50/P95/P99" but not gap detection; docs/cli-reference.md line 87 marks --find-gaps as "(Deprecated)" (the option still works — just the legacy form is deprecated); no mention of stats working in --stream mode anywhere in docs.
      Surfaces: docs/features.md, docs/cli-reference.md
      Development: In features.md Statistical Analysis row, add "gap detection" to the list. In cli-reference.md: update --find-gaps row to say it maps to the FIND_GAPS collector (remove "Deprecated" label or add note it now works via --stats). Add a --stats + --stream note near the stream row or in the Statistics section. Update --stats NAME description to include find_gaps[:THRESHOLD_MS].
      Acceptance: Gap detection mentioned in features; cli-reference --find-gaps row updated; --stats/--stream interaction noted; no regression in docs format.
      Verification: cmake --build build -j --output-on-failure

- [ ] Add UniqueMessagesCollector and TopMessagesCollector direct unit tests
      Mode: improve
      Rationale: tests/stats/core.cpp has no direct tests for UniqueMessagesCollector or TopMessagesCollector (the first two collectors in the codebase). They appear only in the factory test (CreateCollectorFactory) which does not verify collector output. Any regression in their collect→generateReport logic would be invisible to the test suite.
      Evidence: grep of tests/stats/core.cpp for "UniqueMessages" or "TopMessages" finds only the factory case; src/stats/core.cpp UniqueMessagesCollector::generateReport (line ~45) computes unique_count and messages array; TopMessagesCollector::generateReport (line ~75) sorts by frequency and trims to topN — neither is directly exercised.
      Surfaces: tests/stats/core.cpp
      Development: Add UniqueMessagesCollectorBasic: collect 5 entries (3 unique messages), assert unique_count==3 and messages array has 3 elements. Add UniqueMessagesCollectorReset: collect 2 unique, reset, collect 1, assert unique_count==1. Add TopMessagesCollectorBasic: collect 5 entries (msg A×3, B×2), assert top_messages[0].message=="A" and count==3. Add TopMessagesCollectorRespectN: collector(2) with A×3,B×2,C×1, assert output has 2 entries. Add TopMessagesCollectorReset: collect A×2, reset, collect B×1, assert top is B. 
      Acceptance: All 5 tests compile and pass; stats_core green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_core$" --output-on-failure
