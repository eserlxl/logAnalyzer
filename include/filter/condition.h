// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include <string>
#include <optional>
#include <variant>
#include <vector>
#include <regex>
#include <sstream>
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

    // Factory methods
    static ErrorCode::Result<FilterCondition> createString(LogEntryField field, FilterOperator op, std::string value, bool caseSensitive = false) {
        if (field == LogEntryField::CUSTOM) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Use createCustomString for CUSTOM fields."));
        }
        FilterCondition fc(field, op, std::move(value), FilterValueType::STRING, caseSensitive);
        auto res = fc.validate();
        if (!res) return std::unexpected(res.error());
        return fc;
    }

    static ErrorCode::Result<FilterCondition> createTyped(LogEntryField field, FilterOperator op, std::string value, FilterValueType type, bool caseSensitive = false) {
        if (field == LogEntryField::CUSTOM) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Use createCustomTyped for CUSTOM fields."));
        }
        if (type == FilterValueType::DATETIME) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Use createDatetime for DATETIME valueType."));
        }
        FilterCondition fc(field, op, std::move(value), type, caseSensitive);
        auto res = fc.validate();
        if (!res) return std::unexpected(res.error());
        return fc;
    }

    static ErrorCode::Result<FilterCondition> createCustomString(std::string customField, FilterOperator op, std::string value, bool caseSensitive = false) {
        if (customField.empty()) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Custom field name cannot be empty."));
        }
        FilterCondition fc(LogEntryField::CUSTOM, op, std::move(value), FilterValueType::STRING, caseSensitive, std::nullopt, std::move(customField));
        auto res = fc.validate();
        if (!res) return std::unexpected(res.error());
        return fc;
    }

    static ErrorCode::Result<FilterCondition> createCustomTyped(std::string customField, FilterOperator op, std::string value, FilterValueType type, bool caseSensitive = false) {
        if (customField.empty()) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Custom field name cannot be empty."));
        }
        FilterCondition fc(LogEntryField::CUSTOM, op, std::move(value), type, caseSensitive, std::nullopt, std::move(customField));
        auto res = fc.validate();
        if (!res) return std::unexpected(res.error());
        return fc;
    }

    static ErrorCode::Result<FilterCondition> createDatetime(LogEntryField field, FilterOperator op, std::string value, std::string format) {
        if (field == LogEntryField::CUSTOM) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Use createCustomDatetime for CUSTOM fields."));
        }
        if (format.empty()) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "datetimeFormat cannot be empty for DATETIME type."));
        }
        FilterCondition fc(field, op, std::move(value), FilterValueType::DATETIME, false, std::move(format));
        auto res = fc.validate();
        if (!res) return std::unexpected(res.error());
        return fc;
    }

    static ErrorCode::Result<FilterCondition> createCustomDatetime(std::string customField, FilterOperator op, std::string value, std::string format) {
        if (customField.empty()) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Custom field name cannot be empty."));
        }
        FilterCondition fc(LogEntryField::CUSTOM, op, std::move(value), FilterValueType::DATETIME, false, std::move(format), std::move(customField));
        auto res = fc.validate();
        if (!res) return std::unexpected(res.error());
        return fc;
    }

    static ErrorCode::Result<FilterCondition> createSet(LogEntryField field, FilterOperator op, std::vector<std::string> values) {
        FilterCondition fc(field, op, std::move(values), FilterValueType::STRING);
        auto res = fc.validate();
        if (!res) return std::unexpected(res.error());
        return fc;
    }

    ErrorCode::Result<void> validate() const {
        if (field == LogEntryField::CUSTOM && (!customField || customField->empty())) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Custom field name cannot be empty."));
        }
        if (valueType == FilterValueType::DATETIME) {
            if (!datetimeFormat || datetimeFormat->empty()) {
                return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "DATETIME value_type requires a non-empty 'datetimeFormat'."));
            }
        }
        return {};
    }
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
    j["caseSensitive"] = fc.caseSensitive;
    if (fc.datetimeFormat) {
        j["datetimeFormat"] = *fc.datetimeFormat;
    }
}

inline ErrorCode::Result<void> from_json(const nlohmann::json& j, FilterCondition& fc, const std::string& path = "/") {
    using namespace FilterJsonUtils;
    std::string current_path = path; 

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

    // value_type: defaults to AUTO when omitted; accepts string or integer
    if (j.contains("value_type") && !j.at("value_type").is_null()) {
        if (j.at("value_type").is_string()) {
            auto vtStr = j.at("value_type").get<std::string>();
            auto typeOpt = fromStringToFilterValueType(vtStr);
            if (!typeOpt) {
                return std::unexpected(makeError(Code::InvalidArgument, "Unrecognized value_type: " + vtStr, current_path, "value_type"));
            }
            fc.valueType = *typeOpt;
        } else if (j.at("value_type").is_number_integer()) {
            auto vtInt = j.at("value_type").get<int>();
            auto vtCast = static_cast<FilterValueType>(vtInt);
            auto vtName = toString(vtCast);
            if (vtName.find("UNKNOWN") != std::string::npos && vtCast != FilterValueType::UNKNOWN) {
                return std::unexpected(makeError(Code::InvalidArgument, "Invalid integer for 'value_type'.", current_path, "value_type"));
            }
            fc.valueType = vtCast;
        } else {
            return std::unexpected(makeError(Code::InvalidArgument, "Invalid integer for 'value_type'.", current_path, "value_type"));
        }
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

    // Skip single value parsing for IN/NOT_IN as it will be handled separately
    if (fc.op != FilterOperator::IN && fc.op != FilterOperator::NOT_IN) {
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
                } else if (valJson.is_string()) {
                    std::string s = valJson.get<std::string>();
                    int64_t val;
                    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
                    if (ec != std::errc{} || ptr != s.data() + s.size()) {
                        return std::unexpected(makeError(Code::InvalidArgument, "Type mismatch: value '" + s + "' is not a valid integer.", current_path, "value"));
                    }
                    fc.value = s; // Store as string — evaluation will parse at runtime
                } else {
                    return std::unexpected(makeError(Code::InvalidArgument, "Invalid type for 'value_type' INT.", current_path, "value_type"));
                }
                break;
            case FilterValueType::DOUBLE:
                if (valJson.is_number()) {
                    double d = valJson.get<double>();
                    if (!std::isfinite(d)) {
                         return std::unexpected(makeError(Code::InvalidArgument, "Invalid float value: " + std::to_string(d), current_path, "value"));
                    }
                    fc.value = d;
                } else if (valJson.is_string()) {
                    std::string s = valJson.get<std::string>();
                    if (s == "nan" || s == "inf" || s == "-inf") {
                         return std::unexpected(makeError(Code::InvalidArgument, "Invalid float value: " + s, current_path, "value"));
                    }
                    try {
                        size_t processed = 0;
                        double d = std::stod(s, &processed);
                        if (processed != s.size() || !std::isfinite(d)) {
                             return std::unexpected(makeError(Code::InvalidArgument, "Invalid float value: " + s, current_path, "value"));
                        }
                        fc.value = s; // Store as string — evaluation will parse at runtime
                    } catch (...) {
                         return std::unexpected(makeError(Code::InvalidArgument, "Invalid float value: " + s, current_path, "value"));
                    }
                } else {
                    return std::unexpected(makeError(Code::InvalidArgument, "Type mismatch: value for DOUBLE must be a number or string.", current_path, "value"));
                }
                break;
            case FilterValueType::BOOL:
                if (valJson.is_boolean()) {
                    fc.value = valJson.get<bool>();
                } else if (valJson.is_string()) {
                    std::string s = valJson.get<std::string>();
                    if (s == "true") fc.value = true;
                    else if (s == "false") fc.value = false;
                    else return std::unexpected(makeError(Code::InvalidArgument, "Invalid boolean value: " + s, current_path, "value"));
                } else {
                    return std::unexpected(makeError(Code::InvalidArgument, "Type mismatch: value for BOOL must be a boolean or string.", current_path, "value"));
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
            case FilterValueType::AUTO:
                if (valJson.is_boolean()) {
                    fc.valueType = FilterValueType::BOOL;
                    fc.value = valJson.get<bool>();
                } else if (valJson.is_number_integer()) {
                    fc.valueType = FilterValueType::INT;
                    fc.value = valJson.get<int64_t>();
                } else if (valJson.is_number()) {
                    fc.valueType = FilterValueType::DOUBLE;
                    fc.value = valJson.get<double>();
                } else if (valJson.is_string()) {
                    fc.valueType = FilterValueType::AUTO; 
                    fc.value = valJson.get<std::string>();
                } else {
                    return std::unexpected(makeError(Code::InvalidArgument, "Unsupported value type for AUTO deduction", current_path, "value"));
                }
                break;
            case FilterValueType::UNKNOWN:
            default:
                return std::unexpected(makeError(Code::InvalidArgument, "Unsupported or ambiguous value_type: " + toString(fc.valueType), current_path, "value_type"));
        }
    }

    // Handle IN operator, which requires an array
    if (fc.op == FilterOperator::IN || fc.op == FilterOperator::NOT_IN) {
        if (!valJson.is_array()) {
            return std::unexpected(makeError(Code::InvalidArgument, "Operator IN/NOT_IN requires an array value.", current_path, "value"));
        }
        std::vector<std::string> values;
        for (const auto& element : valJson) {
            if (element.is_string()) values.push_back(element.get<std::string>());
            else if (element.is_number_integer()) values.push_back(std::to_string(element.get<int64_t>()));
            else if (element.is_number()) {
                std::ostringstream oss;
                oss << std::defaultfloat << element.get<double>();
                values.push_back(oss.str());
            }
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
