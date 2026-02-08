## Executive summary
- Project intent is clear: a C++23 log analysis tool/library with parsing, filtering, export, stats, and CLI orchestration (`src/main.cpp`, `src/analyzer/*`, `src/filter/*`, `src/export/*`, `src/stats/*`).
- Required module boundaries are present and aligned: `include/{analyzer,config,core,export,filter,stats,utils}`, mirrored by `src/{...}`, with corresponding `tests/{...}`.
- Build discipline is strong (`-Wall -Wextra -Wpedantic -Werror`), and current build is warning-clean.
- Test suite breadth is good (49 executables across config/core/filter/export/analyzer/stats/utils), and all tests pass.
- Main correctness risks are now primarily error-model consistency and API/build hygiene.
- Multiline parser integration in `load/append` has been fixed in commit `c3ea33b` by switching analyzer pipeline parsing to `processLine()` and adding regression coverage (`src/analyzer/IO.cpp`, `tests/analyzer/Core.cpp`).
- Descending sort comparator strict-order bug has been fixed in commit `f76b95e` in both sorted-filter paths, with regression coverage in analyzer tests (`src/analyzer/Filter.cpp`, `tests/analyzer/Core.cpp`).
- Statistic collector creation now avoids runtime exceptions on invalid collector params (commit `7e7e68e`), reducing crash risk in constructor/settings flows (`src/analyzer/Stats.cpp`, `tests/analyzer/Core.cpp`).
- Statistic JSON deserialization now avoids throw-based control flow by mapping malformed/unknown types to `UNKNOWN` for downstream validation (commit `4dd1531`; `include/stats/Core.h`, `tests/config/Core/Json.cpp`).
- Analyzer parsing pipeline now catches parser exceptions and converts them into structured `AnalysisReport` errors instead of propagating throws (commit `15eb077`; `src/analyzer/IO.cpp`, `tests/analyzer/Core.cpp`).
- Error model is improved and mostly `Result`-driven; constructor-time parser init failures now fall back to defaults instead of throwing for invalid user settings (commit `eddbe55`).
- README/product claim gaps have narrowed; query parsing is now implemented and wired into `--expression` filtering (commit `5cc4c4d`).
- Overall: solid foundation with real strengths, but several high-likelihood correctness and maintainability risks remain.

## Scorecard

| Area | Score (0-10) | Evidence |
|---|---:|---|
| Architecture & separation of concerns | 7.0 | Clear subsystem split in `src/`/`include/`; analyzer coordinates parser/filter/export/stats. But `include/analyzer/Core.h:7` has heavy cross-module coupling and large public surface. |
| Correctness / edge-case handling | 7.5 | Good filter/type handling and multiline parser tests exist; analyzer load path now uses `processLine()`, descending sort now uses strict ordering semantics, and query parsing is implemented for `--expression` with parser tests (`src/analyzer/IO.cpp`, `src/analyzer/Filter.cpp`, `src/filter/Parser.cpp`, `tests/filter/Iteration15.cpp`). |
| Error handling consistency | 8.5 | `ErrorCode::Result` is now consistently preferred through parser/analyzer/config paths: stats config handling is non-throwing (including malformed statistic `params` values in commit `e622f96`), analyzer catches parser exceptions, parser throw mode returns structured `Result` errors (commit `3ce6b12`), and settings updates now roll back safely without leaving partial invalid state (commit `074892a`). |
| Performance risks | 6.0 | Stream mode exists; regex caches present. But non-stream load/append keeps full vectors and sorts/merges (`src/analyzer/Log/Loader.cpp:44`, `src/analyzer/Log/Loader.cpp:224`), and some string copying in parse path. |
| Test quality | 9.0 | 50 passing tests with good breadth; parser/filter/export coverage is strong, query parser behavior is covered, concurrency now includes deterministic snapshot/load coverage, concurrent append/filter/export checks, concurrent `loadAsync` filter/export stress coverage, and concurrent multiline `analyzeStream` isolation checks (`tests/analyzer/Core.cpp`, commits `9a69617`, `9e47304`, `51d85c3`, `6a63d62`), plus direct JsonLogParser tests (commit `ade5392`). |
| Build hygiene | 9.2 | Strict warnings-as-errors in CMake (`CMakeLists.txt`), clean ctest integration, CI now includes repeated analyzer concurrency runs (`.github/workflows/ci.yml`, commit `075f68b`), and dependency strategy supports system packages, optional fetch, plus hermetic offline mirror mode (`LOGANALYZER_OFFLINE_DEPS`, commit `624536a`). |
| API hygiene | 7.0 | Public API header is broad, but analyzer state access now uses thread-safe snapshots for both explicit snapshot APIs and reference-returning accessors (`src/analyzer/IO.cpp`, commits `17ef73c`, `645a17e`). |

## Risk register

1. **Multiline parsing bypass in primary load/append path (Resolved)**  
- Severity: High  
- Likelihood: High  
- Where found: `src/analyzer/IO.cpp`, `src/core/Log/Parser.cpp`  
- Resolution: Fixed in commit `c3ea33b`; analyzer now consumes parser output via `processLine()` and flushes buffered multiline entries correctly. Regression test added: `LoadAndReplaceSupportsMultilineEntries` in `tests/analyzer/Core.cpp`.
- Follow-up: Keep coverage in CI to prevent regressions when parser/analyzer integration changes.

2. **Query-expression feature not implemented but exposed in UX/docs (Resolved)**  
- Severity: High  
- Likelihood: High  
- Where found: `src/filter/Parser.cpp`, `src/main.cpp`  
- Resolution: Fixed in commit `5cc4c4d`; recursive-descent parsing is now implemented and `--expression` is applied in both stream and non-stream filtering paths.
- Follow-up: Extend parser coverage for additional operator aliases and malformed-expression diagnostics as grammar evolves.

3. **Descending sort comparator can violate strict weak ordering (Resolved)**  
- Severity: High  
- Likelihood: Medium  
- Where found: `src/analyzer/Filter.cpp`  
- Resolution: Fixed in commit `f76b95e` by implementing descending comparison as reversed strict ascending (`less(b, a)`) instead of `!less(a, b)` in both sorted-filter overloads.
- Follow-up: Keep regression coverage in `tests/analyzer/Core.cpp` and add broader sort-property tests when expanding test depth.

4. **Mixed exception + `Result` error model in critical paths (Further mitigated)**  
- Severity: High  
- Likelihood: Medium  
- Where: `src/core/Log/Parser.cpp:322`, `src/core/Log/JsonParser.cpp:184`, `src/analyzer/Core.cpp:60`  
- Why it matters: Unexpected throws can bypass expected error-handling paths and terminate CLI/library consumers.  
- Mitigation progress: `src/analyzer/Stats.cpp` throw paths for invalid collector parameters were removed in commit `7e7e68e`.
- Mitigation progress: `src/analyzer/Stats.cpp` throw paths for invalid collector parameters were removed in commit `7e7e68e`, `include/stats/Core.h` JSON deserialization was hardened in commit `4dd1531`, and analyzer parsing now catches parser exceptions in commit `15eb077`.
- Mitigation progress: `src/analyzer/Stats.cpp` throw paths for invalid collector parameters were removed in commit `7e7e68e`, `include/stats/Core.h` JSON deserialization was hardened in commit `4dd1531`, analyzer parsing catches parser exceptions in commit `15eb077`, and parser throw mode now returns `Result` errors instead of throwing in commit `3ce6b12`.
- Mitigation progress: Constructor parser initialization now falls back to default settings instead of throwing on invalid user-supplied regex/config in commit `eddbe55`.
- Mitigation progress: `setSettings()` is now transactional and `setCustomLogLevelMapping()` no longer throws on parser recreation failures, preserving prior valid state in commit `074892a`.
- Minimal mitigation idea: Define and document one error boundary policy (no-throw across public API, or explicit throw boundaries) and test for it.

5. **Thread-safety contract leak via returned references after lock release (Resolved)**  
- Severity: Medium  
- Likelihood: Medium  
- Where: `src/analyzer/IO.cpp:95`, `src/analyzer/IO.cpp:101`, `src/analyzer/IO.cpp:106`  
- Why it matters: Returning references/spans while unlocking immediately can race with concurrent mutation.  
- Mitigation progress: Thread-safe snapshot APIs were added in commit `17ef73c` (`getEntriesSnapshot()`, `getLastReportSnapshot()`).
- Resolution: Fixed in commit `645a17e`; `getEntries()`, `getLastReport()`, and `getEntriesView()` now return thread-local snapshots rather than aliases to shared mutable state, with regression coverage in `tests/analyzer/Core.cpp`.
- Follow-up: Continue using explicit snapshot APIs for clarity in performance-sensitive call paths.

6. **Concurrency behavior largely untested (Further mitigated)**  
- Severity: Medium  
- Likelihood: High  
- Where: `tests/analyzer/Core.cpp:108`  
- Why it matters: Real-world async/stream use can race; current tests don’t validate concurrent access correctness.  
- Mitigation progress: Added deterministic concurrent snapshot-read/load test in commit `9a69617`.
- Mitigation progress: Added concurrent append + filter/export stability coverage in commit `9e47304`.
- Mitigation progress: Added concurrent `loadAsync` + filter/export stress coverage in commit `51d85c3`.
- Mitigation progress: `analyzeStream()` now clones parser state per invocation and has concurrent multiline regression coverage in commit `6a63d62`.
- Mitigation progress: CI now runs repeated `analyzer_Core` executions with `ctest --repeat until-fail` in commit `075f68b` to detect flaky/racy behavior continuously.
- Mitigation progress: `streamIn()` lock boundary now covers `entries_.size()` used for merge reservation, removing a concrete read-race in commit `fc3f674`.
- Minimal mitigation idea: Add dedicated long-running stress/fuzz job for high-contention scenarios in CI.

7. **Fresh builds depend on live network FetchContent (Resolved)**  
- Severity: Medium  
- Likelihood: High  
- Where: `CMakeLists.txt`, `README.md`  
- Resolution: Fixed in commit `624536a`; offline mirror mode was added (`LOGANALYZER_OFFLINE_DEPS`, `LOGANALYZER_DEPS_MIRROR_DIR`, per-dependency source overrides), enabling hermetic configuration without network fetch.
- Follow-up: Add a CI job using `LOGANALYZER_OFFLINE_DEPS=ON` with a prepared mirror cache.

8. **Public API install incomplete for “C++ API” consumers (Resolved)**  
- Severity: Medium  
- Likelihood: Medium  
- Where: `CMakeLists.txt`, `cmake/logAnalyzerConfig.cmake.in`  
- Resolution: Fixed in commit `43ebdec`; install now exports `logAnalyzerTargets`, installs `logAnalyzerConfig.cmake` + version file, and supports `find_package(logAnalyzer CONFIG REQUIRED)` for installed consumers.
- Follow-up: Keep a small external-consumer configure/build check in CI to guard install-package regressions.

9. **Stats config parsing throws from JSON helpers (Resolved)**  
- Severity: Medium  
- Likelihood: Medium  
- Where: `include/stats/Core.h`, `tests/config/Core/Json.cpp`  
- Resolution: Fixed in commit `e622f96`; `StatisticConfig::from_json` now avoids throw-prone map conversions, handles malformed `params` values with no-throw invalidation, and regression tests assert validation-oriented error messages.
- Follow-up: Keep malformed stats-config coverage in config JSON tests as statistic parameters evolve.

10. **Test data path default likely mismatched with repository layout (Resolved)**  
- Severity: Low  
- Likelihood: Medium  
- Where found: `tests/CMakeLists.txt`  
- Resolution: Fixed in commit `00a6093`; default test data directory now points to `${CMAKE_SOURCE_DIR}/tests/data`.
- Follow-up: Keep this default aligned with repository layout if test fixture directories move.

## Build/test results block

```text
Environment: /opt/lxl/c++/logAnalyzer (read-only review)
Date: 2026-02-08

Repository structure verification:
- include/{analyzer,config,core,export,filter,stats,utils}: PRESENT
- src/{...}: PRESENT and mirrored by subsystem folders
- tests/{...}: PRESENT with subsystem coverage

Configure/build:
- Fresh new build dir configure (offline mode + missing mirror): FAILED as expected with explicit offline dependency error
- Configure using existing build tree (Debug): SUCCESS
  Command: cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
- Clean rebuild: SUCCESS
  Command: cmake --build build --target clean && cmake --build build --parallel
- Post-fix verification (multiline parser integration): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure -R analyzer_Core
- Post-fix verification (descending comparator strict ordering): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure -R analyzer_Core
- Post-fix verification (stats collector exception hardening): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure -R analyzer_Core
- Post-fix verification (query parser + --expression integration): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (installed package export/consumer): SUCCESS
  Commands:
  - cmake --install build --prefix /tmp/loganalyzer-install-test
  - cmake -S /tmp/loganalyzer-consumer -B /tmp/loganalyzer-consumer/build -DlogAnalyzer_DIR=/tmp/loganalyzer-install-test/lib/cmake/logAnalyzer
  - cmake --build /tmp/loganalyzer-consumer/build --parallel
- Post-fix verification (concurrent append/filter/export test coverage): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure -R analyzer_Core
- Post-fix verification (no-throw stats config JSON parsing): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure -R config_Core_Json
- Post-fix verification (constructor fallback on invalid parser settings): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure -R analyzer_Core
- Post-fix verification (reference accessor snapshot safety): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure -R analyzer_Core
- Post-fix verification (transactional settings + non-throw remap): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure -R analyzer_Core
- Post-fix verification (offline mirror dependency mode behavior): SUCCESS
  Commands:
  - cmake -S . -B /tmp/loganalyzer-offline-check -DLOGANALYZER_OFFLINE_DEPS=ON -DLOGANALYZER_FETCH_DEPS=ON -DLOGANALYZER_DEPS_MIRROR_DIR=/tmp/does-not-exist
  - Verified expected configure-time failure with actionable dependency guidance (no network attempt required)
- Post-fix verification (loadAsync/filter/export concurrency coverage): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure -R analyzer_Core
- Post-fix verification (concurrent analyzeStream parser isolation): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure -R analyzer_Core
- Post-fix verification (repeat stability gate for concurrency suite): SUCCESS
  Commands:
  - ctest --test-dir build --output-on-failure --repeat until-fail:10 -R analyzer_Core
- Post-fix verification (streamIn race fix): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure

Warnings:
- warning count: 0
- error count during build: 0
- Top 10 warnings: none emitted

Tests:
- Command: ctest --test-dir build --output-on-failure
- Total tests: 50
- Passed: 50
- Failed: 0
- Total wall time: 0.26 sec
- Slowest observed tests:
  1) config_CLI_Filtering: 0.03 sec
  2) config_CLI_Formatting: 0.02 sec
  3) config_CLI_Errors: 0.02 sec
  4) config_CLI_InputOutput: 0.01 sec
  5) analyzer_Core: 0.01 sec
```

## Reality check vs README

**Claims not clearly supported by code/tests**
- No material mismatches found for previously audited claims; prior README/CMake/API packaging gaps are closed in commits `0309f10`, `43ebdec`, and `624536a`.

**Features present but under-documented in README**
- Advanced parser and expression options are now surfaced in README (commit `47e4cbc`), including `--expression`, `--multiline-start-pattern`, `--max-multiline-buffer`, and `--on-parse-error`.
- JSON-based filter expression serialization/deserialization is well-covered in tests (`tests/filter/CoreJson/*`) but not clearly surfaced in README feature summary.

## Appendix: key file paths reviewed
- `CMakeLists.txt`
- `tests/CMakeLists.txt`
- `README.md`
- `docs/features.md`
- `docs/project-structure.md`
- `include/analyzer/Core.h`
- `include/core/Error.h`
- `include/core/Log/Parser.h`
- `include/filter/Expression.h`
- `include/stats/Core.h`
- `src/main.cpp`
- `src/analyzer/Core.cpp`
- `src/analyzer/IO.cpp`
- `src/analyzer/Filter.cpp`
- `src/analyzer/Log/Loader.cpp`
- `src/analyzer/Stats.cpp`
- `src/core/Log/Parser.cpp`
- `src/core/Log/JsonParser.cpp`
- `src/filter/Parser.cpp`
- `src/filter/Expression.cpp`
- `src/filter/ConditionEvaluation.cpp`
- `tests/analyzer/Core.cpp`
- `tests/core/Log/Parser.cpp`
- `tests/filter/Iteration15.cpp`
- `tests/filter/CoreJson/Expression.cpp`
- `tests/export/Core.cpp`
