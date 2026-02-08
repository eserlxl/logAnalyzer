## Executive summary
- Project intent is clear: a C++23 log analysis tool/library with parsing, filtering, export, stats, and CLI orchestration (`src/main.cpp`, `src/analyzer/*`, `src/filter/*`, `src/export/*`, `src/stats/*`).
- Required module boundaries are present and aligned: `include/{analyzer,config,core,export,filter,stats,utils}`, mirrored by `src/{...}`, with corresponding `tests/{...}`.
- Build discipline is strong (`-Wall -Wextra -Wpedantic -Werror`), and current build is warning-clean.
- Test suite breadth is good (50 executables across config/core/filter/export/analyzer/stats/utils), and all tests pass.
- Main correctness risks identified in this audit have been systematically addressed; remaining concerns are primarily long-term maintainability and API surface breadth.
- Multiline parser integration in `load/append` has been fixed in commit `c3ea33b` by switching analyzer pipeline parsing to `processLine()` and adding regression coverage (`src/analyzer/IO.cpp`, `tests/analyzer/Core.cpp`).
- Descending sort comparator strict-order bug has been fixed in commit `f76b95e` in both sorted-filter paths, with regression coverage in analyzer tests (`src/analyzer/Filter.cpp`, `tests/analyzer/Core.cpp`).
- Statistic collector creation now avoids runtime exceptions on invalid collector params (commit `7e7e68e`), reducing crash risk in constructor/settings flows (`src/analyzer/Stats.cpp`, `tests/analyzer/Core.cpp`).
- Statistic JSON deserialization now avoids throw-based control flow by mapping malformed/unknown types to `UNKNOWN` for downstream validation (commit `4dd1531`; `include/stats/Core.h`, `tests/config/Core/Json.cpp`).
- Analyzer parsing pipeline now catches parser exceptions and converts them into structured `AnalysisReport` errors instead of propagating throws (commit `15eb077`; `src/analyzer/IO.cpp`, `tests/analyzer/Core.cpp`).
- Error model is improved and mostly `Result`-driven; constructor-time parser init failures now fall back to defaults instead of throwing for invalid user settings (commit `eddbe55`).
- README/product claim gaps have narrowed; query parsing is now implemented and wired into `--expression` filtering (commit `5cc4c4d`).
- Overall: solid foundation with real strengths, and the high-likelihood correctness risks identified in this audit are now closed with code + CI/test coverage.

## Scorecard

| Area | Score (0-10) | Evidence |
|---|---:|---|
| Architecture & separation of concerns | 7.0 | Clear subsystem split in `src/`/`include/`; analyzer coordinates parser/filter/export/stats. But `include/analyzer/Core.h:7` has heavy cross-module coupling and large public surface. |
| Correctness / edge-case handling | 8.0 | Good filter/type handling and multiline parser tests exist; analyzer load path uses `processLine()`, descending sort uses strict ordering semantics, query parsing is implemented for `--expression`, and concurrent append/stream lost-update behavior was fixed by atomic merge under unique lock with regression coverage (`src/analyzer/Log/Loader.cpp`, `src/analyzer/Streaming.cpp`, commit `17a85b4`). |
| Error handling consistency | 9.0 | `ErrorCode::Result` is now consistently preferred through parser/analyzer/config paths: stats config handling is non-throwing (including malformed statistic `params` values in commit `e622f96`), analyzer catches parser exceptions, parser throw mode returns structured `Result` errors (commit `3ce6b12`), settings updates roll back safely (commit `074892a`), and constructors now avoid explicit fallback-init throws (commit `a4d9101`). |
| Performance risks | 7.2 | Stream mode exists; regex caches present; noisy unconditional export debug dumps were removed (commit `2dc97f8`), parse pipeline copies were reduced via move-based entry handling (commit `055d6e8`), and append/stream merge paths now use ordered fast-paths to avoid full merge work when ranges are already non-overlapping (commit `8b4d35c`). Remaining cost driver is non-stream load-and-replace full sort for large datasets. |
| Test quality | 9.1 | 50 passing tests with good breadth; parser/filter/export coverage is strong, query parser behavior is covered, concurrency includes deterministic snapshot/load coverage, concurrent append/filter/export checks, concurrent `loadAsync` filter/export stress coverage, concurrent multiline `analyzeStream` isolation checks, and explicit concurrent dual-append preservation coverage (`tests/analyzer/Core.cpp`, commits `9a69617`, `9e47304`, `51d85c3`, `6a63d62`, `17a85b4`), plus direct JsonLogParser tests (commit `ade5392`). |
| Build hygiene | 9.4 | Strict warnings-as-errors in CMake (`CMakeLists.txt`), clean ctest integration, CI now includes repeated and shuffled long-running analyzer concurrency stress runs (`.github/workflows/ci.yml`, commits `075f68b`, `113a617`), and dependency strategy supports system packages, optional fetch, plus hermetic offline mirror mode (`LOGANALYZER_OFFLINE_DEPS`, commit `624536a`). |
| API hygiene | 7.6 | Public API header is still broad, but analyzer state access now uses thread-safe snapshots (`src/analyzer/IO.cpp`, commits `17ef73c`, `645a17e`) and public include bloat/layering in `include/analyzer/Core.h` was improved via dependency pruning (commit `7b6b9d2`), decoupling from `stats/Core.h` with forward declarations and `nlohmann/json_fwd.hpp` (commit `8232921`), and replacing `filter/Core.h` umbrella inclusion with targeted filter headers (commit `1cbd836`). |

## Risk register

1. **Multiline parsing bypass in primary load/append path (Resolved)**  
- Severity: High  
- Likelihood: High  
- Where found: `src/analyzer/IO.cpp`, `src/core/Log/Parser.cpp`  
- Resolution: Fixed in commit `c3ea33b`; analyzer now consumes parser output via `processLine()` and flushes buffered multiline entries correctly. Regression test added: `LoadAndReplaceSupportsMultilineEntries` in `tests/analyzer/Core.cpp`.
- Follow-up: Completed in commit `eec3aab`; CI now runs a focused parser/analyzer integration regression step for multiline load and concurrent stream-parser state isolation.

2. **Query-expression feature not implemented but exposed in UX/docs (Resolved)**  
- Severity: High  
- Likelihood: High  
- Where found: `src/filter/Parser.cpp`, `src/main.cpp`  
- Resolution: Fixed in commit `5cc4c4d`; recursive-descent parsing is now implemented and `--expression` is applied in both stream and non-stream filtering paths.
- Follow-up: Completed in commit `bf6dbb2`; parser now supports logical aliases (`&&`, `||`, `!`) and has regression coverage for malformed-expression diagnostics.

3. **Descending sort comparator can violate strict weak ordering (Resolved)**  
- Severity: High  
- Likelihood: Medium  
- Where found: `src/analyzer/Filter.cpp`  
- Resolution: Fixed in commit `f76b95e` by implementing descending comparison as reversed strict ascending (`less(b, a)`) instead of `!less(a, b)` in both sorted-filter overloads.
- Follow-up: Completed in commit `3daedfc`; analyzer tests now include broader sort-property coverage validating ascending/descending monotonicity and entry-set preservation across sort keys.

4. **Mixed exception + `Result` error model in critical paths (Resolved)**  
- Severity: High  
- Likelihood: Medium  
- Where: `src/core/Log/Parser.cpp:322`, `src/core/Log/JsonParser.cpp:184`, `src/analyzer/Core.cpp:60`  
- Why it matters: Unexpected throws can bypass expected error-handling paths and terminate CLI/library consumers.  
- Mitigation progress: `src/analyzer/Stats.cpp` throw paths for invalid collector parameters were removed in commit `7e7e68e`; `include/stats/Core.h` JSON deserialization was hardened in commit `4dd1531`; analyzer parsing catches parser exceptions in commit `15eb077`; and parser throw mode now returns `Result` errors instead of throwing in commit `3ce6b12`.
- Mitigation progress: Constructor parser initialization now falls back to default settings instead of throwing on invalid user-supplied regex/config in commit `eddbe55`.
- Mitigation progress: `setSettings()` is now transactional and `setCustomLogLevelMapping()` no longer throws on parser recreation failures, preserving prior valid state in commit `074892a`.
- Mitigation progress: API-level error boundary policy is now documented in `docs/api-reference.md`, and regression tests assert non-throw behavior for invalid file inputs across `loadAndReplace`, `append`, and `analyzeStream` in commit `ff802a8`.
- Resolution: Constructor fallback path no longer throws explicitly when parser initialization remains unavailable, keeping the object constructible and shifting failures to structured runtime `Result` errors (commit `a4d9101`).
- Follow-up: Completed in commit `7c1c04c`; analyzer regression coverage now explicitly includes throw-mode `append` and `analyzeStream` non-throw paths, and stream parsing/flush exceptions are translated into `Result` errors.

5. **Thread-safety contract leak via returned references after lock release (Resolved)**  
- Severity: Medium  
- Likelihood: Medium  
- Where: `src/analyzer/IO.cpp:95`, `src/analyzer/IO.cpp:101`, `src/analyzer/IO.cpp:106`  
- Why it matters: Returning references/spans while unlocking immediately can race with concurrent mutation.  
- Mitigation progress: Thread-safe snapshot APIs were added in commit `17ef73c` (`getEntriesSnapshot()`, `getLastReportSnapshot()`).
- Resolution: Fixed in commit `645a17e`; `getEntries()`, `getLastReport()`, and `getEntriesView()` now return thread-local snapshots rather than aliases to shared mutable state, with regression coverage in `tests/analyzer/Core.cpp`.
- Follow-up: Completed in commit `63acf40`; non-stream CLI processing now uses `getEntriesSnapshot()` explicitly when building filtered working sets.

6. **Concurrency behavior largely untested (Resolved)**  
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
- Mitigation progress: `parseAndReport()` now clones parser state per call, and append/stream merge-commit is atomic under unique lock to prevent concurrent lost updates (commit `17a85b4`), with explicit concurrent dual-append preservation test coverage.
- Resolution: Added dedicated long-running shuffled concurrency stress execution in CI (`./build/tests/analyzer_Core --gtest_repeat=50 --gtest_shuffle`) in commit `113a617`.
- Follow-up: Completed in commit `bad7ea0`; CI now runs a dedicated TSAN analyzer-core job, and timestamp parsing/formatting paths were synchronized to eliminate detected time-conversion data races.

7. **Fresh builds depend on live network FetchContent (Resolved)**  
- Severity: Medium  
- Likelihood: High  
- Where: `CMakeLists.txt`, `README.md`  
- Resolution: Fixed in commit `624536a`; offline mirror mode was added (`LOGANALYZER_OFFLINE_DEPS`, `LOGANALYZER_DEPS_MIRROR_DIR`, per-dependency source overrides), enabling hermetic configuration without network fetch.
- Follow-up: Completed in commit `b53724a`; CI now runs `Build (Offline Mirror Mode)` using a prepared mirror cache and `LOGANALYZER_OFFLINE_DEPS=ON`.

8. **Public API install incomplete for “C++ API” consumers (Resolved)**  
- Severity: Medium  
- Likelihood: Medium  
- Where: `CMakeLists.txt`, `cmake/logAnalyzerConfig.cmake.in`  
- Resolution: Fixed in commit `43ebdec`; install now exports `logAnalyzerTargets`, installs `logAnalyzerConfig.cmake` + version file, and supports `find_package(logAnalyzer CONFIG REQUIRED)` for installed consumers.
- Follow-up: Completed in commit `b73916e`; CI package-consumer validation now uses `CMAKE_PREFIX_PATH` package discovery and executes the built consumer binary.

9. **Stats config parsing throws from JSON helpers (Resolved)**  
- Severity: Medium  
- Likelihood: Medium  
- Where: `include/stats/Core.h`, `tests/config/Core/Json.cpp`  
- Resolution: Fixed in commit `e622f96`; `StatisticConfig::from_json` now avoids throw-prone map conversions, handles malformed `params` values with no-throw invalidation, and regression tests assert validation-oriented error messages.
- Follow-up: Completed in commit `0134da5`; config JSON tests now cover additional malformed `params` shapes (array and nested object values) with validation-based, no-throw error reporting expectations.

10. **Test data path default likely mismatched with repository layout (Resolved)**  
- Severity: Low  
- Likelihood: Medium  
- Where found: `tests/CMakeLists.txt`  
- Resolution: Fixed in commit `00a6093`; default test data directory now points to `${CMAKE_SOURCE_DIR}/tests/data`.
- Follow-up: Completed in commit `212aa20`; test CMake now validates fixture directory existence, supports legacy fallback, and auto-recovers stale cached test-data paths to `${CMAKE_SOURCE_DIR}/tests/data`.

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
  - cmake -S /tmp/loganalyzer-consumer -B /tmp/loganalyzer-consumer/build -DCMAKE_PREFIX_PATH=/tmp/loganalyzer-install-test
  - cmake --build /tmp/loganalyzer-consumer/build --parallel
  - /tmp/loganalyzer-consumer/build/consumer
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
- Post-fix verification (export debug-noise removal under repeated analyzer runs): SUCCESS
  Commands:
  - ./build/tests/analyzer_Core --gtest_repeat=5 --gtest_shuffle --gtest_brief=1
- Post-fix verification (documented no-throw API policy + regression): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure -R analyzer_Core
- Post-fix verification (append fixture warning-noise cleanup): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (parse-pipeline copy reduction): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (parser-state isolation + atomic append/stream merges): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure -R analyzer_Core
- Post-fix verification (long-running shuffled concurrency stress): SUCCESS
  Commands:
  - ./build/tests/analyzer_Core --gtest_repeat=10 --gtest_shuffle --gtest_brief=1
- Post-fix verification (ordered append/stream merge fast-paths): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (constructor fallback no-throw hardening): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure -R analyzer_Core
- Post-fix verification (CI offline-mirror validation path): SUCCESS
  Commands:
  - cmake -S . -B build-offline-mirror -DCMAKE_BUILD_TYPE=Debug -DLOGANALYZER_OFFLINE_DEPS=ON -DLOGANALYZER_FETCH_DEPS=ON -DLOGANALYZER_DEPS_MIRROR_DIR=/tmp/loganalyzer-mirror
  - cmake --build build-offline-mirror --parallel
- Post-fix verification (TSAN analyzer concurrency gate): SUCCESS
  Commands:
  - cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" -DLOGANALYZER_OFFLINE_DEPS=ON -DLOGANALYZER_FETCH_DEPS=ON -DLOGANALYZER_DEPS_MIRROR_DIR=/tmp/loganalyzer-mirror-tsan
  - cmake --build build-tsan --parallel --target analyzer_Core
  - TSAN_OPTIONS=halt_on_error=1 ./build-tsan/tests/analyzer_Core --gtest_brief=1
- Post-fix verification (query alias parsing + malformed diagnostics coverage): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure -R filter_Iteration15
- Post-fix verification (expanded malformed statistic params JSON coverage): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure -R config_Core_Json
- Post-fix verification (sorted-filter ordering/membership property coverage): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure -R analyzer_Core
- Post-fix verification (parser/analyzer integration CI regression target): SUCCESS
  Commands:
  - ./build/tests/analyzer_Core --gtest_filter=LogAnalyzerTest.LoadAndReplaceSupportsMultilineEntries:LogAnalyzerTest.ConcurrentAnalyzeStreamUsesIndependentParserState --gtest_brief=1
- Post-fix verification (throw-mode append/analyzeStream non-throw coverage): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure -R analyzer_Core
- Post-fix verification (explicit snapshot API usage in non-stream CLI path): SUCCESS
  Commands:
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (test data directory alignment and stale-cache recovery): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (public analyzer header include-bloat reduction): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (public analyzer header stats-layer decoupling): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (public analyzer header filter-layer decoupling): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
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
- JSON-based filter expression serialization/deserialization documentation gap was closed in commit `3ac04a6` (README + features table updates).

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
