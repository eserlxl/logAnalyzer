#ifndef FILTER_LEGACY_H
#define FILTER_LEGACY_H

#include <vector>
#include <string>
#include <set>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

#include "core/Error.h" // For ErrorCode::Result
#include "filter/FilterTypes.h" // For FilterOperator, LogEntryField

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
    j = nlohmann::json{
        {"field", Utils::logEntryFieldToString(fr.field)},
        {"op", Utils::filterOperatorToString(fr.op)},
        {"value", fr.value},
        {"caseSensitive", fr.caseSensitive}
    };
}

inline ErrorCode::Result<void> from_json(const nlohmann::json& j, FilterRule& fr) {
    if (!j.contains("field") || !j.at("field").is_string()) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterRule is missing or has invalid 'field'."));
    }
    fr.field = Utils::stringToLogEntryField(j.at("field").get<std::string>());
    if (fr.field == LogEntryField::UNKNOWN) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterRule has an unrecognized field: " + j.at("field").get<std::string>()));
    }

    if (!j.contains("op") || !j.at("op").is_string()) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterRule is missing or has invalid 'op'."));
    }
    auto opOpt = Utils::stringToFilterOperator(j.at("op").get<std::string>());
    if (!opOpt) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterRule has an unrecognized 'op' string: " + j.at("op").get<std::string>()));
    }
    fr.op = *opOpt;

    if (!j.contains("value") || !j.at("value").is_string()) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterRule is missing or has invalid 'value'."));
    }
    fr.value = j.at("value").get<std::string>();

    fr.caseSensitive = j.value("caseSensitive", false);

    return {}; // Success
}

#endif // FILTER_LEGACY_H
