# planwright Plan — .
<!-- Session: 2026-06-05T00:00:00Z -->

- [x] Fix --stats-interval partial stats routing to outputStream
      Mode: repair
      Rationale: When --stats-interval fires, partial stats are routed to *outputStream when statsOutputPath is set (main.cpp:268-270), which corrupts the main data output (e.g., mixes stats JSON into an NDJSON log file). Partial stats are informational metadata and should always go to stderr.
      Evidence: src/main.cpp:264-274 — after `++streamMatchCount` the interval block does `std::ostream& statsOut = cliOptions.statsOutputPath.empty() ? std::cerr : *outputStream;`; the end-of-stream path (lines 342-358) correctly opens a separate std::ofstream for statsOutputPath; the interval path bypasses that and writes to the main output stream.
      Surfaces: src/main.cpp
      Development: In src/main.cpp, change the interval stats ostream selection to always use std::cerr: replace `std::ostream& statsOut = cliOptions.statsOutputPath.empty() ? std::cerr : *outputStream;` with `std::ostream& statsOut = std::cerr;` — the end-of-stream path still writes to statsOutputPath correctly; partial-interval output is always informational and must not interleave with data.
      Acceptance: Interval stats always write to stderr; end-of-stream stats still write to statsOutputPath when set; no data corruption of outputStream; config_cli_analysis green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_analysis$" --output-on-failure

- [x] Fix validation rejects TOP_MESSAGES without top_n param
      Mode: repair
      Rationale: LogAnalyzerSettings::validate() (validation.cpp:135-143) errors when a TOP_MESSAGES StatisticConfig has no top_n param, but both factories (stats/core.cpp and stats/analyzer.cpp) default topN=10 when top_n is absent — making bare --stats top_messages configs impossible to use from JSON files.
      Evidence: src/config/validation.cpp:135 checks `sc.type == StatisticType::TOP_MESSAGES || sc.type == StatisticType::TOP_N_FIELD_VALUES` and errors when !has_top_n; tests/config/core/validation.cpp:262 always passes top_n="5" for TOP_MESSAGES (no test without top_n); stats/core.cpp:424-433 sets `int topN = 10;` then optionally reads top_n; TOP_N_FIELD_VALUES genuinely requires top_n (factory returns nullptr without it).
      Surfaces: src/config/validation.cpp, tests/config/core/validation.cpp
      Development: In src/config/validation.cpp, change the condition at line 135 from `StatisticType::TOP_MESSAGES || StatisticType::TOP_N_FIELD_VALUES` to only `StatisticType::TOP_N_FIELD_VALUES`; add a separate block for TOP_MESSAGES that only validates the format of top_n when it IS present; in tests/config/core/validation.cpp add a test ValidateTopMessagesNoTopNAccepted that constructs {TOP_MESSAGES, {}} and asserts errors.empty().
      Acceptance: TOP_MESSAGES with no top_n passes validate(); TOP_MESSAGES with invalid top_n still errors; TOP_N_FIELD_VALUES without top_n still errors; validation tests green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_core_validation$" --output-on-failure

- [x] Consolidate third normalizeTargetFieldName copy from validation.cpp
      Mode: improve
      Rationale: src/config/validation.cpp has a third anonymous-namespace copy of normalizeTargetFieldName (lines 15-39) identical to the stats::detail version in include/stats/helpers.h; Cycle 8 only deduplicated the two stats/ copies and missed this third one — future drift in field alias handling (e.g., adding a new alias) must still be applied in two places.
      Evidence: src/config/validation.cpp:15 defines `std::optional<std::string> normalizeTargetFieldName(std::string_view rawField)` in an anonymous namespace; include/stats/helpers.h provides the identical `stats::detail::normalizeTargetFieldName`; the bodies are byte-for-byte identical.
      Surfaces: src/config/validation.cpp
      Development: In src/config/validation.cpp, remove the anonymous-namespace normalizeTargetFieldName definition (lines 13-40) and add `#include "stats/helpers.h"` plus `using stats::detail::normalizeTargetFieldName;` — the dependency is appropriate since validation.cpp already validates statistic configs which are stats-domain objects.
      Acceptance: Build succeeds; all validation tests pass; no anonymous normalizeTargetFieldName definition remains in validation.cpp.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_core_validation$" --output-on-failure

- [x] Add validate() call after analyzerSettings.merge() in main.cpp
      Mode: improve
      Rationale: main.cpp calls analyzerSettings.merge(cliSettings) at line 75 but never calls analyzerSettings.validate() — invalid CLI-originated settings (bad regex patterns in --pattern, invalid filter rules, etc.) are silently accepted and may cause runtime failures deep in the pipeline instead of being reported at startup.
      Evidence: src/main.cpp:75 merges CLI settings; src/config/json.cpp:370 calls validate() in the JSON config path; src/main.cpp has no validate() call; src/config/validation.cpp validates lineParsePattern with std::regex (line 68) which would catch bad regexes early.
      Surfaces: src/main.cpp
      Development: In src/main.cpp, after line 75 (`analyzerSettings.merge(cliSettings);`), add `auto validationErrors = analyzerSettings.validate(); if (!validationErrors.empty()) { for (const auto& e : validationErrors) std::cerr << "Configuration error: " << e << '\n'; return 1; }` — this reports all validation errors to stderr and exits cleanly before any analysis begins.
      Acceptance: Invalid settings (e.g., bad regex in lineParsePattern from CLI) produce an error message and exit code 1; valid settings proceed normally; full test suite passes.
      Verification: cmake --build build -j && ctest --test-dir build --output-on-failure

- [x] Add warning when --stats-interval is used without --stream
      Mode: improve
      Rationale: --stats-interval only has effect in stream mode (the interval check is in the stream callback); in batch mode the option is silently ignored — users have no indication their interval setting is ineffective.
      Evidence: src/main.cpp:266 interval check is inside the `if (cliOptions.streamMode)` block at line 186; src/config/cli.cpp:183 adds the option with no stream-mode validation; if a user runs `logAnalyzer myfile.log --stats-interval 100 --stats unique_messages` they see no intervals and no warning.
      Surfaces: src/config/cli.cpp
      Development: In src/config/cli.cpp parseCLI, in the post-parse validation section (around line 358), add a check: if `app.count("--stats-interval") && !appOptions.streamMode`, emit `std::cerr << "Warning: --stats-interval has no effect without --stream mode.\n";` — not an error (it's a valid config, just non-functional).
      Acceptance: Using --stats-interval without --stream emits a warning to stderr; parsing still succeeds; using --stats-interval with --stream emits no warning; config_cli_analysis green.
      Verification: cmake --build build -j && ctest --test-dir build -R "^config_cli_analysis$" --output-on-failure

- [x] Add test that --stats-interval 0 is rejected by CLI validation
      Mode: improve
      Rationale: --stats-interval uses CLI::PositiveNumber which should reject 0, but this constraint is untested — if the validator was accidentally removed, 0 would silently enter the code and cause an immediate stats flush on every entry (every-match modulo 0 is undefined behavior / divide-by-zero).
      Evidence: src/config/cli.cpp:184 `->check(CLI::PositiveNumber)`; main.cpp:266 `streamMatchCount % *cliOptions.statsInterval == 0` — if statsInterval were 0, this is UB; tests/config/cli/analysis.cpp StatsIntervalOption tests N=100 but not N=0; CLI::PositiveNumber should reject 0 (requires >0) but is not directly exercised.
      Surfaces: tests/config/cli/analysis.cpp
      Development: In tests/config/cli/analysis.cpp add TEST_F(CLIConfigTest, StatsIntervalZeroRejected) that parses {"log_analyzer","dummy_log_file.log","--stats-interval","0"} and asserts !result.has_value() with code InvalidCLIOption; add TEST_F(CLIConfigTest, StatsIntervalNegativeRejected) for "--stats-interval","-1" similarly.
      Acceptance: Both tests pass; config_cli_analysis green.
      Verification: ctest --test-dir build -R "^config_cli_analysis$" --output-on-failure

- [x] Consolidate dual stat collector factories into single createCollector
      Mode: reorganize
      Rationale: Statistics::createCollector (stats/core.cpp:418+) and LogAnalyzer::createStatisticCollector (stats/analyzer.cpp:82+) implement the same switch-on-StatisticType dispatch with near-identical code for all 10 types — any new StatisticType must be added in both places, and subtle bugs (like different FIND_GAPS parse paths) can diverge silently.
      Evidence: src/stats/core.cpp:418-535 and src/stats/analyzer.cpp:82-255 both contain full switch statements over all StatisticType values; both are called from different contexts (Statistics::addConfiguredCollectors vs LogAnalyzer internal stats); Cycle 8 consolidated the helpers but left both factory bodies untouched.
      Surfaces: src/stats/core.cpp, src/stats/analyzer.cpp, include/stats/core.h, include/analyzer/core.h
      Development: Make LogAnalyzer::createStatisticCollector delegate to Statistics::createCollector: in src/stats/core.cpp, ensure Statistics::createCollector returns std::unique_ptr<IStatisticCollector>; in src/stats/analyzer.cpp, replace the full switch-case body with a single `return Statistics::createCollector(config);` call, adjusting the return type if needed (shared_ptr vs unique_ptr); delete the duplicate switch cases; run stats tests to verify both call paths exercise the same factory.
      Acceptance: Build succeeds; all stats tests pass; src/stats/analyzer.cpp createStatisticCollector no longer contains a switch-case over StatisticType.
      Verification: cmake --build build -j && ctest --test-dir build -R "^stats_" --output-on-failure
