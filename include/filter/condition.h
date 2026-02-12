// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include <string>
#include <optional>
#include <variant>
#include <vector>
#include <regex>
#include <nlohmann/json.hpp>
#include "core/error.h"
#include "core/log/types.h"
#include "filter/types.h"
#include "filter/enum_string_conversions.h"
#include "filter/json_utils.h"
#include "utils/core.h"
#include "utils/string.h"
#include "utils/ip_address.h"
#include "utils/version.h"

namespace filter {

struct FilterCondition {
    using ValueVariant = std::variant<std::monostate, std::string, int64_t, double, bool, std::vector<std::string>>;

    LogEntryField field = LogEntryField::UNKNOWN;
    FilterOperator op = FilterOperator::EQUALS;
    ValueVariant value;
    FilterValueType valueType = FilterValueType::STRING;
    bool caseSensitive = false;
    std::optional<std::string> datetimeFormat;
    std::optional<std::string> customField;
    std::optional<std::regex> compiledRegex;

    FilterCondition() = default;

    // Public constructor
    FilterCondition(LogEntryField f, FilterOperator o, ValueVariant v, FilterValueType vt, bool cs = false,
                    std::optional<std::string> dtFormat = std::nullopt, std::optional<std::string> cf = std::nullopt)
        : field(f), op(o), value(std::move(v)), valueType(vt), caseSensitive(cs),
          datetimeFormat(std::move(dtFormat)), customField(std::move(cf)) {}
};

// --- JSON Serialization ---

inline void to_json(nlohmann::json& j, const FilterCondition& fc) {
    if (fc.field == LogEntryField::CUSTOM && fc.customField) {
        j["field"] = *fc.customField;
    } else {
        j["field"] = Utils::logEntryFieldToString(fc.field);
    }
    
    j["op"] = toString(fc.op);
    
    if (fc.op != FilterOperator::IS_PRESENT && fc.op != FilterOperator::IS_ABSENT && fc.op != FilterOperator::IS_NULL && fc.op != FilterOperator::IS_NOT_NULL) {
        std::visit([&j](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                j["value"] = nullptr;
            } else {
                j["value"] = arg;
            }
        }, fc.value);
    }

    j["value_type"] = toString(fc.valueType);
    if (fc.caseSensitive) {
        j["caseSensitive"] = fc.caseSensitive;
    }
    if (fc.datetimeFormat) {
        j["datetimeFormat"] = *fc.datetimeFormat;
    }
}

inline ErrorCode::Result<void> from_json(const nlohmann::json& j, FilterCondition& fc) {
    using namespace FilterJsonUtils;
    std::string current_path = "/"; // Simplified path for errors

    auto fieldStrRes = getRequired<std::string>(j, "field", current_path);
    if (!fieldStrRes) return std::unexpected(fieldStrRes.error());
    std::string fieldStr = *fieldStrRes;

    fc.field = Utils::stringToLogEntryField(fieldStr);
    if (fc.field == LogEntryField::UNKNOWN) {
        if (fieldStr.empty()) {
            return std::unexpected(makeError(Code::InvalidArgument, "Field name cannot be empty.", current_path, "field"));
        }
        fc.field = LogEntryField::CUSTOM;
        fc.customField = fieldStr; 
    }

    auto opStrRes = getRequired<std::string>(j, "op", current_path);
    if (!opStrRes) return std::unexpected(opStrRes.error());
    auto opOpt = fromStringToFilterOperator(*opStrRes);
    if (!opOpt) {
        return std::unexpected(makeError(Code::InvalidArgument, "Unrecognized operator: " + *opStrRes, current_path, "op"));
    }
    fc.op = *opOpt;

    // Make value_type optional; default to AUTO for flexibility
    auto vtRes = getOptional<std::string>(j, "value_type");
    if(vtRes) {
        auto typeOpt = fromStringToFilterValueType(*vtRes);
        if (!typeOpt) {
            return std::unexpected(makeError(Code::InvalidArgument, "Unrecognized value_type: " + *vtRes, current_path, "value_type"));
        }
        fc.valueType = *typeOpt;
    } else {
        fc.valueType = FilterValueType::AUTO;
    }

    fc.caseSensitive = getOptional<bool>(j, "caseSensitive").value_or(false);
    fc.datetimeFormat = getOptional<std::string>(j, "datetimeFormat");

    // If operator is presence/absence, no value is needed.
    if (fc.op == FilterOperator::IS_PRESENT || fc.op == FilterOperator::IS_ABSENT || fc.op == FilterOperator::IS_NULL || fc.op == FilterOperator::IS_NOT_NULL) {
        fc.value = std::monostate{};
        return {}; // Success
    }

    // Value is required for all other operators
    if (!j.contains("value")) {
        return std::unexpected(makeError(Code::InvalidArgument, "Missing required key: 'value'", current_path, "value"));
    }
    const auto& valJson = j.at("value");

    // --- Pre-computation and Type-safe Value Parsing ---
    FilterValueType targetType = fc.valueType;

    // AUTO type inference based on JSON type
    if (targetType == FilterValueType::AUTO) {
        if (valJson.is_boolean()) targetType = FilterValueType::BOOL;
        else if (valJson.is_number_integer()) targetType = FilterValueType::INT;
        else if (valJson.is_number()) targetType = FilterValueType::DOUBLE;
        else if (valJson.is_string()) targetType = FilterValueType::STRING; // Default for strings
        else if (valJson.is_array()) targetType = FilterValueType::STRING; // For IN operator
        else if (valJson.is_null()) {
             return std::unexpected(makeError(Code::InvalidArgument, "'value' cannot be null for this operation.", current_path, "value"));
        }
    }
    
    // In AUTO mode, update the condition's valueType to the inferred type
    if (fc.valueType == FilterValueType::AUTO) {
        fc.valueType = targetType;
    }


    switch (targetType) {
        case FilterValueType::STRING:
            if (valJson.is_string()) {
                fc.value = valJson.get<std::string>();
            } else {
                 return std::unexpected(makeError(Code::InvalidArgument, "Type mismatch: value for STRING must be a string.", current_path, "value"));
            }
            break;
        case FilterValueType::INT:
            if (valJson.is_number_integer()) {
                fc.value = valJson.get<int64_t>();
            } else {
                return std::unexpected(makeError(Code::InvalidArgument, "Type mismatch: value for INT must be an integer.", current_path, "value"));
            }
            break;
        case FilterValueType::DOUBLE:
            if (valJson.is_number()) {
                fc.value = valJson.get<double>();
            } else {
                return std::unexpected(makeError(Code::InvalidArgument, "Type mismatch: value for DOUBLE must be a number.", current_path, "value"));
            }
            break;
        case FilterValueType::BOOL:
            if (valJson.is_boolean()) {
                fc.value = valJson.get<bool>();
            } else {
                return std::unexpected(makeError(Code::InvalidArgument, "Type mismatch: value for BOOL must be a boolean.", current_path, "value"));
            }
            break;
        case FilterValueType::DATETIME: {
            if (!valJson.is_string()) return std::unexpected(makeError(Code::InvalidArgument, "Type mismatch: value for DATETIME must be a string.", current_path, "value"));
            if (!fc.datetimeFormat || fc.datetimeFormat->empty()) {
                return std::unexpected(makeError(Code::InvalidArgument, "DATETIME value_type requires a non-empty 'datetimeFormat'.", current_path, "datetimeFormat"));
            }
            fc.value = valJson.get<std::string>();
            auto validationResult = FilterJsonUtils::validateTimestamp(std::get<std::string>(fc.value), {*fc.datetimeFormat}, current_path, "value");
            if (!validationResult) return std::unexpected(validationResult.error());
            break;
        }
        case FilterValueType::LOG_LEVEL:
        case FilterValueType::VERSION:
        case FilterValueType::IP_ADDRESS:
            if (!valJson.is_string()) return std::unexpected(makeError(Code::InvalidArgument, "Type mismatch: value for " + toString(targetType) + " must be a string.", current_path, "value"));
            fc.value = valJson.get<std::string>();
            break;
        case FilterValueType::AUTO: // Should have been resolved, but handle defensively
        case FilterValueType::UNKNOWN:
        default:
             return std::unexpected(makeError(Code::InvalidArgument, "Unsupported or ambiguous value_type: " + toString(fc.valueType), current_path, "value_type"));
    }

    // Handle IN operator, which requires an array
    if (fc.op == FilterOperator::IN || fc.op == FilterOperator::NOT_IN) {
        if (!valJson.is_array()) {
            return std::unexpected(makeError(Code::InvalidArgument, "Operator IN/NOT_IN requires an array value.", current_path, "value"));
        }
        std::vector<std::string> values;
        for (const auto& element : valJson) {
            if (element.is_string()) values.push_back(element.get<std::string>());
            else if (element.is_number()) values.push_back(std::to_string(element.get<double>()));
            else if (element.is_boolean()) values.push_back(element.get<bool>() ? "true" : "false");
            else return std::unexpected(makeError(Code::InvalidArgument, "Invalid type in 'value' array for IN/NOT_IN.", current_path, "value"));
        }
        fc.value = values;
    }

    // --- Pre-compile Regex ---
    if (fc.op == FilterOperator::REGEX) {
        if (!std::holds_alternative<std::string>(fc.value)) {
            return std::unexpected(makeError(Code::InvalidArgument, "Value for REGEX operator must be a string pattern.", current_path, "value"));
        }
        const auto& pattern = std::get<std::string>(fc.value);
        try {
            auto flags = fc.caseSensitive ? std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase;
            fc.compiledRegex = std::regex(pattern, flags);
        } catch (const std::regex_error& e) {
            return std::unexpected(makeError(Code::InvalidRegex, "Invalid regex pattern '" + pattern + "': " + e.what(), current_path, "value"));
        }
    }

    return {}; // Success
}

} // namespace filter
