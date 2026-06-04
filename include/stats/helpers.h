// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>

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

} // namespace stats::detail
