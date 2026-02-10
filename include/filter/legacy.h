// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include <vector>
#include <string>
#include <set>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

#include "core/error.h" // For ErrorCode::Result
#include "core/log/types.h" // For LogLevel, PatternType etc.
#include "utils/core.h"    // For ci_less
#include "filter/enum_string_conversions.h" // For enum to string conversions
#include "filter/json_utils.h"

namespace filter {

// Transitional: Represents a simplified filter rule for legacy JSON formats.
// This is part of a transitional phase and should not be used for new development.
// It will be fully replaced by FilterCondition and the FilterExpression system.
// Its role is to provide a temporary bridge for backward compatibility during migration.
struct FilterRule {
    LogEntryField field = LogEntryField::UNKNOWN;
    FilterOperator op;
    std::string value;
    bool caseSensitive = false;
};

// Legacy: A wrapper for older filter settings.
// This struct exists for backward compatibility and is being phased out.
// All new filtering logic must use the FilterExpression system. This struct and its
// related logic will be removed once the migration to FilterExpression is complete.
struct FilterCriteria {
    std::vector<LogLevel> levels;
    std::string keyword;
    bool keywordCaseSensitive = false;
    std::string regexPattern;
    std::optional<std::chrono::system_clock::time_point> startTime;
    std::optional<std::chrono::system_clock::time_point> endTime;

    // The 'rules' member is part of the legacy system. It is commented out
    // as its functionality is being replaced by FilterExpression. It was intended
    // for a future iteration (e.g., Iteration 5) but that plan has been superseded
    // by the more robust FilterExpression architecture.
    // std::vector<FilterRule> rules;
    // std::optional<FilterExpression> expression; // Future integration
};


// JSON conversion for FilterRule
inline void to_json(nlohmann::json& j, const FilterRule& fr) {
    j["field"] = Utils::logEntryFieldToString(fr.field);
    j["op"] = toString(fr.op);
    j["value"] = fr.value;
    j["caseSensitive"] = fr.caseSensitive;
}

// The `current_path` parameter is kept for backward compatibility but is unused.
inline ErrorCode::Result<FilterRule> from_json(const nlohmann::json& j, [[maybe_unused]] const std::string& current_path = "/") {
    using namespace FilterJsonUtils;

    FilterRule fr;

    auto fieldRes = getRequired<std::string>(j, "field", current_path);
    if (!fieldRes) return std::unexpected(fieldRes.error());
    
    fr.field = Utils::stringToLogEntryField(*fieldRes);
    if (fr.field == LogEntryField::UNKNOWN) {
        return std::unexpected(makeError(Code::InvalidArgument, "Unrecognized field: " + *fieldRes, current_path, "field"));
    }

    auto opRes = getRequired<std::string>(j, "op", current_path);
    if (!opRes) return std::unexpected(opRes.error());
    
    auto opOpt = fromStringToFilterOperator(*opRes);
    if (!opOpt) {
        return std::unexpected(makeError(Code::InvalidArgument, "Unrecognized operator: " + *opRes, current_path, "op"));
    }
    fr.op = *opOpt;

    auto valRes = getRequired<std::string>(j, "value", current_path);
    if (!valRes) return std::unexpected(valRes.error());
    fr.value = *valRes;

    fr.caseSensitive = getOptional<bool>(j, "caseSensitive").value_or(false);

    return fr; // Success
}

} // namespace filter
