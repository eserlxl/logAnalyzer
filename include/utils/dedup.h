// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include "core/log/types.h"
#include "utils/core.h"
#include "utils/time.h"
#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace Utils {

// Returns the dedup key for `entry` under `field`.
// `absentIdx` is incremented each time a standard optional field is absent or
// a custom field is absent, making each absent entry unique (never collapsed).
inline std::string getDedupKey(const LogEntry& entry, const std::string& field, size_t& absentIdx) {
    if (field == "level") return logLevelToString(entry.level);
    if (field == "message") return entry.message;
    if (field == "source" || field == "source_file" || field == "sourcefile") return entry.sourceFile;
    if (field == "id") {
        return entry.id ? std::to_string(*entry.id) : "__absent__" + std::to_string(absentIdx++);
    }
    if (field == "line_number" || field == "lineNumber" || field == "line" || field == "linenumber") {
        return entry.sourceLineNumber ? std::to_string(*entry.sourceLineNumber) : "__absent__" + std::to_string(absentIdx++);
    }
    if (field == "thread_id" || field == "threadId" || field == "thread" || field == "threadid" || field == "tid") {
        return entry.threadId ? *entry.threadId : "__absent__" + std::to_string(absentIdx++);
    }
    if (field == "module") {
        return entry.module ? *entry.module : "__absent__" + std::to_string(absentIdx++);
    }
    if (field == "host") {
        return entry.host ? *entry.host : "__absent__" + std::to_string(absentIdx++);
    }
    if (field == "timestamp" || field == "time") {
        return entry.timestamp ? Utils::formatTimestamp(*entry.timestamp) : "__absent__" + std::to_string(absentIdx++);
    }
    auto it = entry.customFields.find(field);
    if (it == entry.customFields.end()) return "__absent__" + std::to_string(absentIdx++);
    return it->second;
}

// Keeps only the first entry per unique value of `field`.
// Standard field names: "level", "message", "source" / "source_file" / "sourcefile", "id",
// "lineNumber" (alias: "line_number", "line", "linenumber"),
// "threadId" (alias: "thread_id", "thread", "threadid", "tid"),
// "module", "host", "timestamp" (alias: "time"). Any other name is treated as a
// custom field key. Entries where an optional standard field or custom field is absent
// are each treated as unique (never merged with each other).
// When `keepLast` is true, the last entry per unique value is kept (instead of
// the first), preserving the original relative order of the kept entries. This
// is useful for "latest state per key" views (e.g. the last event per session).
inline void applyDedupField(std::vector<LogEntry>& entries, std::string_view field,
                            bool keepLast = false) {
    if (field.empty()) return;
    const std::string fieldStr(field);
    std::unordered_set<std::string> seen;
    size_t idx = 0;
    if (!keepLast) {
        entries.erase(
            std::remove_if(entries.begin(), entries.end(),
                [&](const LogEntry& entry) {
                    return !seen.insert(getDedupKey(entry, fieldStr, idx)).second;
                }),
            entries.end());
        return;
    }
    // keepLast: scan from the end so the first key seen is the last occurrence,
    // then compact in place keeping those entries in their original order.
    std::vector<bool> keep(entries.size(), false);
    for (std::size_t i = entries.size(); i-- > 0;) {
        if (seen.insert(getDedupKey(entries[i], fieldStr, idx)).second) {
            keep[i] = true;
        }
    }
    std::size_t write = 0;
    for (std::size_t read = 0; read < entries.size(); ++read) {
        if (keep[read]) {
            entries[write++] = std::move(entries[read]);
        }
    }
    entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(write), entries.end());
}

} // namespace Utils
