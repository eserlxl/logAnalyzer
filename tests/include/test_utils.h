// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2024 Eser KUBALI

#pragma once

#include <gtest/gtest.h>
#include "analyzer/types.h"
#include "export/core.h"
#include "utils/time.h"
#include "core/log/types.h"
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

// Helper function to create a LogEntry for testing purposes.
static LogEntry createLogEntry(
    size_t id,
    LogLevel level,
    const std::string& message,
    std::optional<std::chrono::system_clock::time_point> timestamp = std::nullopt,
    const std::map<std::string, std::string>& customFields = {},
    const std::string& sourceFile = "",
    std::optional<size_t> sourceLineNumber = std::nullopt)
{
    LogEntry entry;
    entry.id = id;
    entry.level = level;
    entry.message = message;
    entry.timestamp = timestamp;
    entry.customFields = customFields;
    entry.sourceFile = sourceFile;
    entry.sourceLineNumber = sourceLineNumber;
    return entry;
}

static LogEntry createLogEntry(LogLevel level, const std::string& message) {
    return createLogEntry(0, level, message);
}

static LogEntry createLogEntry(LogLevel level, const std::string& message, const std::string& sourceFile, const std::map<std::string, std::string>& customFields) {
    return createLogEntry(0, level, message, std::nullopt, customFields, sourceFile);
}

