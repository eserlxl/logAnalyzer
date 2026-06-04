// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include "core/log/types.h"
#include "utils/core.h"
#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace Utils {

// Returns the dedup key for `entry` under `field`.
// `absentIdx` is incremented each time a custom field is absent, making each
// absent entry unique (never collapsed into the same bucket).
inline std::string getDedupKey(const LogEntry& entry, const std::string& field, size_t& absentIdx) {
    if (field == "level") return logLevelToString(entry.level);
    if (field == "message") return entry.message;
    if (field == "source" || field == "source_file") return entry.sourceFile;
    auto it = entry.customFields.find(field);
    if (it == entry.customFields.end()) return "__absent__" + std::to_string(absentIdx++);
    return it->second;
}

// Keeps only the first entry per unique value of `field`.
// Standard field names: "level", "message", "source" / "source_file". Any other name is
// treated as a custom field key. Entries where the custom field is absent
// are each treated as unique (never merged with each other).
inline void applyDedupField(std::vector<LogEntry>& entries, std::string_view field) {
    if (field.empty()) return;
    const std::string fieldStr(field);
    std::unordered_set<std::string> seen;
    size_t idx = 0;
    entries.erase(
        std::remove_if(entries.begin(), entries.end(),
            [&](const LogEntry& entry) {
                return !seen.insert(getDedupKey(entry, fieldStr, idx)).second;
            }),
        entries.end());
}

} // namespace Utils
