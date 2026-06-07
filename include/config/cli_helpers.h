// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef CONFIG_CLI_HELPERS_H
#define CONFIG_CLI_HELPERS_H

#include "config/common_types.h"
#include "stats/core.h"
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace CLIConfigHelpers {

void trimInPlace(std::string& s);
std::optional<StatisticConfig> parseStatisticConfig(const std::string& statStr);
std::pair<std::string, std::string> parseFieldAlias(const std::string& fieldStr);

// Resolve the process exit status for a successful run. With grep-style
// --exit-code mode on, a run that matched no entries returns 1 (and a run that
// matched at least one returns 0); with the mode off the status is always 0, so
// default behavior is unchanged. Error paths set their own non-zero status.
inline int resolveExitCode(bool exitCodeMode, std::size_t matchCount) {
    return (exitCodeMode && matchCount == 0) ? 1 : 0;
}

// Decide whether to emit ANSI color. An explicit --color always/never always
// wins; AUTO emits color only to an interactive terminal that is not redirected
// to a file and when the NO_COLOR convention (no-color.org) is not signalled.
inline bool shouldUseColor(Config::ColorOption opt, bool isTerminal,
                           bool outputIsFile, bool noColorEnv) {
    if (opt == Config::ColorOption::ALWAYS) {
        return true;
    }
    if (opt == Config::ColorOption::NEVER) {
        return false;
    }
    return isTerminal && !outputIsFile && !noColorEnv;
}

// Worked examples appended to --help (CLI11 footer) so a first-time user has a
// runnable starting point instead of only the flat option list.
inline std::string usageExamples() {
    return
        "Examples:\n"
        "  logAnalyzer app.log --level ERROR\n"
        "  logAnalyzer app.log --keyword database --keyword timeout --logic OR\n"
        "  logAnalyzer app.log --expression '(level=ERROR and msg contains \"auth\") or status_code >= 500'\n"
        "  logAnalyzer system.log --level ERROR --since 30m --format json --pretty\n"
        "  logAnalyzer app.log --stats count_by_level --stats 'percentile_stats:latency_ms'\n"
        "  cat app.log | logAnalyzer - --level WARN\n"
        "\n"
        "See docs/cli-reference.md for the full option reference.";
}

} // namespace CLIConfigHelpers

#endif // CONFIG_CLI_HELPERS_H
