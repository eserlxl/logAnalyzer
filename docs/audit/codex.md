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
| Correctness / edge-case handling | 9.0 | Good filter/type handling and multiline parser tests exist; analyzer load path uses `processLine()`, descending sort uses strict ordering semantics, query parsing is implemented for `--expression`, stream ingestion now provides best-effort progress callback reporting for seekable streams (`src/analyzer/IO.cpp`, `tests/analyzer/LogWriter.cpp`, commit `93baf2a`) with cancellation-safe final-status signaling (no misleading final `completed` event after cancellation in commit `6be927e`, and cancelled status now remains authoritative even when parse issues were encountered in commit `47c5c42`), append/stream stats collection is now preserved even across move-based merge paths with explicit regression coverage (`src/analyzer/Log/Loader.cpp`, `src/analyzer/Streaming.cpp`, `tests/analyzer/Core.cpp`, commit `6ee4c3f`), statistics are now reset on `loadAndReplace` and `clear` so collector reports stay consistent with current analyzer state (commit `2a9ee54`), deprecated compatibility overload `loadAndReplace(filePath, pattern)` now actually applies the provided pattern to parser settings with regression coverage (commit `266e02f`), scoped settings handling in `LogReader` now avoids re-lock deadlock risk introduced by snapshot-locking `getSettings()` (commit `8aaea83`), append/stream operations now consistently update `lastReport` (including empty-input paths) with regression coverage to prevent stale report snapshots (commit `ff6b276`), reader compatibility `doLoadAndReplace` now also resets collectors before recomputation to avoid stale statistic accumulation in that path (commit `be17c2a`), custom log-level mapping parser reconfiguration is now synchronized on `stateMutex_` to match parser read/write lock discipline (commit `b2109b4`), CLI non-stream sorting now correctly treats missing sort options as default timestamp-ascending/no-explicit-sort instead of implicitly sorting by message when options are unset (`src/main.cpp`, commit `2d75a0c`), non-stream sorting now implements all declared sort keys (`SOURCE`, `THREAD_ID`) instead of falling back to message-order semantics for those options (`src/main.cpp`, commit `a361555`), CSV streaming output now quotes header/value fields containing carriage returns to prevent malformed multi-line records (`src/main.cpp`, commit `d32f626`), CLI text case-normalization now avoids undefined behavior on non-ASCII bytes by using `unsigned char` conversions for `tolower`/`toupper` paths (`src/config/CLI.cpp`, commit `31f1714`), `--stats` key/value parsing now trims internal whitespace so inputs like `type = TOP_MESSAGES , top_n = 7` are interpreted correctly instead of producing malformed parameter keys (`src/config/CLI.cpp`, `tests/config/CLI/Analysis.cpp`, commit `66542dd`), non-stream CLI statistics now avoid duplicate accumulation by resetting configured collectors and evaluating only the final filtered entry set (instead of collecting during append and then collecting again in main; `src/main.cpp`, commit `766b8cf`), `--stats` parsing now rejects unknown bare tokens so malformed values such as `type=top_messages,bogus` fail fast instead of being silently ignored (`src/config/CLI.cpp`, `tests/config/CLI/Analysis.cpp`, commit `e3a20ea`), malformed key/value stats parameters with empty keys now fail validation instead of creating invalid parameter map entries (`src/config/CLI.cpp`, `tests/config/CLI/Analysis.cpp`, commit `f28438d`), `--map-level` assignments now trim key/value whitespace so inputs like `CRITICAL = fatal` map correctly instead of creating space-padded keys or failing level lookup (`src/config/CLI.cpp`, `tests/config/CLI/Filtering.cpp`, commit `e7a03af`), `--stats` values now reject multiple type declarations (e.g. `type=top_messages,entry_rate`) to avoid ambiguous/overwritten collector intent (`src/config/CLI.cpp`, `tests/config/CLI/Analysis.cpp`, commit `d0fe11a`), legacy `top_messages:<n>` syntax now validates `<n>` as a numeric value to avoid silently accepting malformed counts that would otherwise degrade to fallback defaults (`src/config/CLI.cpp`, `tests/config/CLI/Analysis.cpp`, commit `624af13`), non-stream output now honors all configured statistic collectors (including legacy `--stats-window`) rather than only collectors requested via `--stats` flags (`src/main.cpp`, `tests/config/CLI/Analysis.cpp`, commit `481cd1e`), invalid non-positive `--tail-interval` values are now rejected at CLI parse time to prevent negative/zero polling intervals from propagating into runtime behavior (`src/config/CLI.cpp`, `tests/config/CLI/Errors.cpp`, commit `61bb4aa`), legacy `--stats-window` / `--find-gaps` values now require positive numbers so invalid non-positive inputs fail fast instead of being silently ignored (`src/config/CLI.cpp`, `tests/config/CLI/Errors.cpp`, commit `1c5297c`), non-positive `top_n` values in programmatic statistic configs now safely fall back to valid defaults to prevent invalid collector behavior in analyzer/stats factory paths (`src/analyzer/Stats.cpp`, `src/stats/Core.cpp`, `tests/analyzer/Core.cpp`, commit `46f72a3`), config validation now enforces strict full-string numeric parsing so malformed values like `top_n=5abc` or numeric filter values with trailing junk are rejected instead of partially accepted (`src/config/CoreValidation.cpp`, `tests/config/Core/Validation.cpp`, commit `eba38c1`), analyzer/stats collector creation now also rejects partial numeric `top_n` strings (`7abc`) with strict parsing in both runtime collector paths (`src/analyzer/Stats.cpp`, `src/stats/Core.cpp`, `tests/analyzer/Core.cpp`, `tests/stats/Core.cpp`, commit `9b80fc0`), parser line-number field extraction now enforces strict full-string unsigned conversion to reject malformed values like `12abc` instead of partially accepting prefixes (`src/core/Log/Parser.cpp`, `tests/core/Log/Parser.cpp`, commit `17f4723`), filter evaluation now uses strict integer/float conversion (no partial parse acceptance for values like `123abc` or `1.23ms`) across INT/FLOAT/DOUBLE comparisons (`src/filter/ConditionEvaluation.cpp`, `tests/filter/Evaluation.cpp`, commit `4b13c0b`), utility enum/option parsing now uses `unsigned char`-safe uppercase conversion to eliminate undefined behavior on high-bit bytes in case-normalization paths (`src/utils/Core.cpp`, `tests/utils/Enum.cpp`, commit `9877a4e`), ISO-8601 parsing now rejects invalid timezone offsets with hour component `>=24` (for both `+` and `-` offsets) instead of accepting out-of-range values (`src/utils/Time.cpp`, `tests/utils/Time.cpp`, commit `dade986`), human-readable size parsing now rejects non-finite/overflowed values and guards character classification against signed-char UB before conversion to `size_t` (`src/utils/Core.cpp`, `tests/utils/String.cpp`, commit `3ae8628`), and duration parsing now uses overflow-checked microsecond conversion factors so large but syntactically valid values (including extended units like weeks/months/years) fail with structured range errors instead of overflowing during unit scaling (`src/utils/Time.cpp`, `tests/utils/Time.cpp`, commit `ccc67f9`). |
| Error handling consistency | 9.1 | `ErrorCode::Result` is now consistently preferred through parser/analyzer/config paths: stats config handling is non-throwing (including malformed statistic `params` values in commit `e622f96`), analyzer catches parser exceptions, parser throw mode returns structured `Result` errors (commit `3ce6b12`), settings updates roll back safely (commit `074892a`), constructors now avoid explicit fallback-init throws (commit `a4d9101`), and settings access now uses thread-local snapshot semantics in `getSettings()` to avoid exposing references to shared mutable state across lock release (commit `0219ef2`). |
| Performance risks | 7.2 | Stream mode exists; regex caches present; noisy unconditional export debug dumps were removed (commit `2dc97f8`), parse pipeline copies were reduced via move-based entry handling (commit `055d6e8`), and append/stream merge paths now use ordered fast-paths to avoid full merge work when ranges are already non-overlapping (commit `8b4d35c`). Remaining cost driver is non-stream load-and-replace full sort for large datasets. |
| Test quality | 9.1 | 50 passing tests with good breadth; parser/filter/export coverage is strong, query parser behavior is covered, concurrency includes deterministic snapshot/load coverage, concurrent append/filter/export checks, concurrent `loadAsync` filter/export stress coverage, concurrent multiline `analyzeStream` isolation checks, and explicit concurrent dual-append preservation coverage (`tests/analyzer/Core.cpp`, commits `9a69617`, `9e47304`, `51d85c3`, `6a63d62`, `17a85b4`), plus direct JsonLogParser tests (commit `ade5392`). |
| Build hygiene | 9.4 | Strict warnings-as-errors in CMake (`CMakeLists.txt`), clean ctest integration, CI now includes repeated and shuffled long-running analyzer concurrency stress runs (`.github/workflows/ci.yml`, commits `075f68b`, `113a617`), dependency strategy supports system packages, optional fetch, plus hermetic offline mirror mode (`LOGANALYZER_OFFLINE_DEPS`, commit `624536a`), shared CLI conversion maps are now `inline` header variables to avoid per-translation-unit static duplication (`include/config/CommonTypes.h`, commit `7dd6392`), and CLI executable translation-unit dependencies were tightened by removing unused filter/settings/json/iomanip/error/optional dependencies (`src/main.cpp`, commits `716563e`, `e1341e6`, `11e6502`, `6a0198b`, `9cb13c7`, `e52632f`). |
| API hygiene | 8.9 | Public API header is still broad, but analyzer state access now uses thread-safe snapshots (`src/analyzer/IO.cpp`, commits `17ef73c`, `645a17e`) and public include bloat/layering in `include/analyzer/Core.h` was improved via dependency pruning (commit `7b6b9d2`), decoupling from `stats/Core.h` with forward declarations + `nlohmann/json_fwd.hpp` (commit `8232921`), replacing `filter/Core.h` umbrella inclusion with targeted filter headers (commit `1cbd836`), removing the heavy `config/CLI.h` dependency by using the shared `ParserErrorAction` type directly (commit `4fd8a6e`), forward-declaring `filter::FilterCriteria` to avoid pulling `filter/Legacy.h` into the public API header (commit `24cffc8`), removing the transitive `core/Log/IParserFactory.h` dependency in favor of forward declarations (commit `ee9c2cd`), correcting stale deprecation guidance text to match the actual public parser action type (`ParserErrorAction`) in the API surface (commit `6b29e72`), migrating analyzer internals/public reader API signatures from `CLIConfig::ParserErrorAction` to the core `ParserErrorAction` alias (commit `2cbe65f`), applying the same decoupling in core parser interfaces/implementations with explicit direct includes replacing former transitive CLI-header dependencies (commit `bf24b81`), making `include/analyzer/Log/Reader.h` more self-contained with explicit standard-library dependencies instead of transitive reliance (commit `1c52ed1`), adding explicit standard includes in `include/analyzer/Core.h` for directly used types (`size_t`, `std::string_view`, `std::pair`) to reduce hidden transitive dependencies (commit `cbdbf5c`), aligning non-CLI tests to the same parser-action alias to reduce namespace coupling and stale type usage drift (commit `3729716`), implementing previously declared statistics lifecycle APIs (`remove/clear/reset/processEntries`) with analyzer regression coverage to close public API declaration/definition gaps (commit `a54839f`), removing stale CLI-header coupling from analyzer core implementation (`src/analyzer/Core.cpp`, commit `5e4768f`), removing an unused analyzer-core mutex field after lock-discipline consolidation to reduce dead API-internal state (`include/analyzer/Core.h`, commit `f52311c`), replacing direct filter header dependencies in the analyzer public API header with explicit forward declarations for filter types/enums (commit `8bf51c7`), removing an unused transitive settings dependency from `config/CommonTypes.h` to tighten include layering in config mappings (commit `afa61c1`), decoupling CLI option parsing from legacy `CompositeFilter::Logic` by using `FilterLogicalOperator` in config-facing types/maps with conversion at composition boundaries (`include/config/CommonTypes.h`, `include/config/CLI.h`, `src/main.cpp`, commit `b5ea304`), removing additional unused standard-library dependencies from the public CLI header (`include/config/CLI.h`, commits `cb75d91`, `8dda8f2`), and removing another unused standard include from the analyzer public header to tighten API transitive dependencies (`include/analyzer/Core.h`, commit `2da1fb0`). |

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
- Post-fix verification (public API header decoupled from `filter/Legacy.h`): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (public API header decoupled from parser-factory transitive include): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (deprecation guidance type names aligned with current API): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (analyzer module decoupled from CLI parser-action namespace): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (core parser module decoupled from CLI parser-action namespace): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (log reader header self-contained include hygiene): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (analyzer core header explicit standard includes): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (test suite parser-action alias alignment): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (stream progress callback reporting for seekable inputs): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (progress callback cancellation-completion semantics): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (append/stream stats collection preserved across move-merge paths): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (statistics lifecycle API implementation + regression coverage): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (statistics reset on load-replace and clear): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (stale CLI include removal from analyzer core implementation): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (deprecated load-replace pattern compatibility path): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (thread-safe settings snapshot accessor): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (scoped reader settings deadlock-avoidance update): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (append/stream lastReport state consistency): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (reader compatibility load-replace statistics reset alignment): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (cancellation status precedence over partial parse outcomes): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (synchronized custom-level parser reconfiguration): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
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
- Post-fix verification (public analyzer header CLI-layer decoupling): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (remove unused analyzer core mutex member): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (forward-declare filter types in analyzer header): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (config common-types include layering cleanup): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (CLI filter-logic type decoupled from legacy composite filter): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (inline shared config mapping tables): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (CLI header unused include cleanup): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (CLI header unused expected include cleanup): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (analyzer header unused chrono include cleanup): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (main translation-unit include cleanup): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (main redundant config include cleanup): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (main unused json include cleanup): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (main unused iomanip include cleanup): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (main unused core-error include cleanup): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (main unused optional include cleanup): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (CLI default sort behavior when sort options are unset): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (CLI non-stream SOURCE/THREAD_ID sorting support): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (CSV carriage-return escaping in stream output): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (CLI parser safe case conversion for non-ASCII bytes): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (--stats key/value whitespace trimming): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (non-stream CLI statistics duplicate-accumulation fix): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (--stats rejects unknown bare tokens): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (--stats rejects empty key parameters): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (--map-level whitespace trimming and validation): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (--stats duplicate type declaration rejection): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (legacy top_messages numeric value validation): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (non-stream statistics honor all configured collectors): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (--tail-interval positive-value validation): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (--stats-window/--find-gaps positive-value validation): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (positive top_n enforcement in analyzer/stats collector creation): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (strict full-string numeric parsing in config validation): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (strict top_n parsing in analyzer/stats collector creation): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (strict parser line-number field conversion): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (strict numeric parsing in filter INT/FLOAT evaluation): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (safe utility case-normalization on high-bit bytes): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (ISO-8601 timezone offset hour-range validation): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (human-readable size overflow and numeric-safety validation): SUCCESS
  Commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  - cmake --build build --parallel
  - ctest --test-dir build --output-on-failure
- Post-fix verification (overflow-safe duration unit conversion): SUCCESS
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
