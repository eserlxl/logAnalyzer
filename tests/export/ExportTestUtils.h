// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 eserlxl

#pragma once

#include <gtest/gtest.h>
#include <sstream>
#include <vector>
#include <nlohmann/json.hpp>
#include "export/Exporter.h"
#include "core/LogTypes.h"
#include "utils/UtilsCore.h"
#include <limits>
#include <chrono>
#include <optional>
#include <map>

// Helper function to create a LogEntry
[[nodiscard]] LogEntry createLogEntry(size_t id, LogLevel level, const std::string& message,
                                      std::optional<std::chrono::system_clock::time_point> timestamp = std::nullopt,
                                      const std::map<std::string, std::string>& customFields = {},
                                      const std::string& sourceFile = "", size_t sourceLineNumber = 0) {
    LogEntry entry;
    entry.id = id;
    entry.level = level;
    entry.message = message;
    entry.timestamp = timestamp;
    for (const auto& field : customFields) {
        entry.customFields[field.first] = field.second;
    }
    entry.sourceFile = sourceFile;
    entry.sourceLineNumber = sourceLineNumber;
    return entry;
}
