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

// Keeps only the first entry per unique value of `field`.
// Standard field names: "level", "message", "source". Any other name is
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
                std::string key;
                if (fieldStr == "level") {
                    key = logLevelToString(entry.level);
                } else if (fieldStr == "message") {
                    key = entry.message;
                } else if (fieldStr == "source") {
                    key = entry.sourceFile;
                } else {
                    auto it = entry.customFields.find(fieldStr);
                    if (it == entry.customFields.end()) {
                        key = "__absent__" + std::to_string(idx++);
                    } else {
                        key = it->second;
                    }
                }
                return !seen.insert(key).second;
            }),
        entries.end());
}

} // namespace Utils
