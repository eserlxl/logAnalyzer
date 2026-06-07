// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include "core/log/types.h"
#include "utils/core.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace stats::detail {

inline bool tryParseStrictPositiveInt(std::string_view value, int& parsedValue) {
    if (value.empty()) {
        return false;
    }
    int parsed = 0;
    const char* begin = value.data();
    const char* end = begin + value.size();
    auto [ptr, ec] = std::from_chars(begin, end, parsed);
    if (ec != std::errc{} || ptr != end || parsed <= 0) {
        return false;
    }
    parsedValue = parsed;
    return true;
}

inline std::optional<std::string> normalizeTargetFieldName(std::string_view rawField) {
    if (rawField.empty()) {
        return std::nullopt;
    }
    std::string field(rawField);
    const auto first = field.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return std::nullopt;
    }
    const auto last = field.find_last_not_of(" \t");
    field = field.substr(first, last - first + 1);
    std::transform(field.begin(), field.end(), field.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (field == "level") return "level";
    if (field == "message") return "message";
    if (field == "source" || field == "source_file" || field == "sourcefile") return "sourceFile";
    if (field == "timestamp" || field == "time") return "timestamp";
    if (field == "line" || field == "line_number" || field == "linenumber") return "lineNumber";
    if (field == "thread" || field == "thread_id" || field == "threadid" || field == "tid") return "threadId";
    if (field == "module") return "module";
    if (field == "host") return "host";
    if (field == "custom" || field == "custom_fields" || field == "customfields") return "customFields";
    if (field == "id") return "id";
    return std::nullopt;
}

inline void trimInPlace(std::string& value) {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string::npos) {
        value.clear();
        return;
    }
    const auto last = value.find_last_not_of(" \t");
    value = value.substr(first, last - first + 1);
}

// Parse a comma- or semicolon-separated list of percentiles (each strictly in
// the (0, 100] range) into `out`. Returns false on an empty list, an empty/blank
// token, a non-numeric token, or any value outside (0, 100]. Leading/trailing
// whitespace around each token is ignored.
inline bool parsePercentileList(std::string_view value, std::vector<double>& out) {
    out.clear();
    const auto flush = [&out](std::string token) -> bool {
        const auto first = token.find_first_not_of(" \t");
        if (first == std::string::npos) {
            return false;
        }
        const auto last = token.find_last_not_of(" \t");
        token = token.substr(first, last - first + 1);
        const char* begin = token.c_str();
        char* end = nullptr;
        const double parsed = std::strtod(begin, &end);
        if (end != begin + token.size()) {
            return false;
        }
        if (!(parsed > 0.0 && parsed <= 100.0)) {
            return false;
        }
        out.push_back(parsed);
        return true;
    };
    std::string current;
    for (const char c : value) {
        if (c == ',' || c == ';') {
            if (!flush(current)) {
                return false;
            }
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!flush(current)) {
        return false;
    }
    return !out.empty();
}

// Format a percentile value as a report key, e.g. 50 -> "p50", 99.9 -> "p99.9".
// Default stream formatting drops trailing zeros so whole numbers stay compact.
inline std::string percentileKey(double percentile) {
    std::ostringstream oss;
    oss << percentile;
    return "p" + oss.str();
}

inline std::string extractFieldValue(const LogEntry& entry,
                                     std::string_view targetField,
                                     std::string_view customFieldKey) {
    if (targetField == "level") {
        return Utils::logLevelToString(entry.level);
    } else if (targetField == "message") {
        return entry.message;
    } else if (targetField == "sourceFile") {
        return entry.sourceFile;
    } else if (targetField == "timestamp" && entry.timestamp) {
        return Utils::formatTimestamp(*entry.timestamp);
    } else if (targetField == "lineNumber" && entry.sourceLineNumber) {
        return std::to_string(*entry.sourceLineNumber);
    } else if (targetField == "threadId" && entry.threadId) {
        return *entry.threadId;
    } else if (targetField == "module" && entry.module) {
        return *entry.module;
    } else if (targetField == "host" && entry.host) {
        return *entry.host;
    } else if (targetField == "customFields" && !customFieldKey.empty()) {
        auto it = entry.customFields.find(std::string(customFieldKey));
        if (it != entry.customFields.end()) {
            return it->second;
        }
    } else if (targetField == "id" && entry.id) {
        return std::to_string(*entry.id);
    }
    return "";
}

} // namespace stats::detail
