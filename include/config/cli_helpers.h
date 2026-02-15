// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef CONFIG_CLI_HELPERS_H
#define CONFIG_CLI_HELPERS_H

#include "config/common_types.h"
#include <optional>
#include <string>
#include <utility>

namespace CLIConfigHelpers {

void trimInPlace(std::string& s);
std::optional<StatisticConfig> parseStatisticConfig(const std::string& statStr);
std::pair<std::string, std::string> parseFieldAlias(const std::string& fieldStr);

} // namespace CLIConfigHelpers

#endif // CONFIG_CLI_HELPERS_H
