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

} // namespace CLIConfigHelpers

#endif // CONFIG_CLI_HELPERS_H
