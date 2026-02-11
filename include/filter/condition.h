// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include <string>
#include <optional>
#include <stdexcept>
#include <charconv>
#include <cmath>
#include <cctype>
#include <nlohmann/json.hpp>
#include "core/error.h"
#include "core/log/types.h"
#include "utils/core.h"
#include "utils/string.h"
#include "utils/ip_address.h"
#include "utils/version.h"
#include "filter/types.h"
#include "filter/enum_string_conversions.h"
#include "filter/json_utils.h"
#include <iostream> // Added for std::cerr warning

namespace filter {

// Represents a single filtering condition (leaf node in the filter tree)
struct FilterCondition {
    LogEntryField field = LogEntryField::UNKNOWN;
    FilterOperator op = FilterOperator::EQUALS;
    
    // New value representation
    using ValueVariant = std::variant<std::monostate, std::string, int64_t, double, bool, std::vector<std::string>>;
    ValueVariant value;
    
    FilterValueType valueType = FilterValueType::STRING;
    bool caseSensitive = false;

    std::optional<std::string> datetimeFormat;
    std::optional<std::string> customField; // Added based on audit report
    
    // Caching members for performance optimization
    mutable std::optional<std::regex> compiledRegex;
    mutable std::optional<FilterValueType> inferredValueType;
    mutable std::optional<std::variant<std::vector<std::string>, nlohmann::json>> parsedValue;


    FilterCondition() = default;

    // --- Factory Functions ---

    static ErrorCode::Result<FilterCondition> createString(LogEntryField f, FilterOperator o, std::string v, bool cs = false) {
        if (f == LogEntryField::CUSTOM) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Use createCustomString for CUSTOM fields."));
        }
        return FilterCondition(f, o, std::move(v), FilterValueType::STRING, cs, std::nullopt, std::nullopt);
    }

    static ErrorCode::Result<FilterCondition> createCustomString(std::string customFieldName, FilterOperator o, std::string v, bool cs = false) {
        if (customFieldName.empty()) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Custom field name cannot be empty."));
        }
        return FilterCondition(LogEntryField::CUSTOM, o, std::move(v), FilterValueType::STRING, cs, std::nullopt, std::move(customFieldName));
    }

    static ErrorCode::Result<FilterCondition> createTyped(LogEntryField f, FilterOperator o, std::string v, FilterValueType vt) {
        if (f == LogEntryField::CUSTOM) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Use createCustomTyped for CUSTOM fields."));
        }
        if (vt == FilterValueType::DATETIME) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Use createDatetime for DATETIME valueType."));
        }
        return FilterCondition(f, o, std::move(v), vt, false, std::nullopt, std::nullopt);
    }
    
    static ErrorCode::Result<FilterCondition> createCustomTyped(std::string customFieldName, FilterOperator o, std::string v, FilterValueType vt) {
        if (customFieldName.empty()) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Custom field name cannot be empty."));
        }
        if (vt == FilterValueType::DATETIME) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Use createCustomDatetime for DATETIME valueType."));
        }
        return FilterCondition(LogEntryField::CUSTOM, o, std::move(v), vt, false, std::nullopt, std::move(customFieldName));
    }

    static ErrorCode::Result<FilterCondition> createDatetime(LogEntryField f, FilterOperator o, std::string v, std::string dtFormat) {
        if (f == LogEntryField::CUSTOM) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Use createCustomDatetime for CUSTOM fields."));
        }
        if (dtFormat.empty()) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "datetimeFormat cannot be empty for DATETIME type."));
        }
        return FilterCondition(f, o, std::move(v), FilterValueType::DATETIME, false, std::move(dtFormat), std::nullopt);
    }
    
    static ErrorCode::Result<FilterCondition> createCustomDatetime(std::string customFieldName, FilterOperator o, std::string v, std::string dtFormat) {
        if (customFieldName.empty()) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Custom field name cannot be empty."));
        }
        if (dtFormat.empty()) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "datetimeFormat cannot be empty for DATETIME type."));
        }
        return FilterCondition(LogEntryField::CUSTOM, o, std::move(v), FilterValueType::DATETIME, false, std::move(dtFormat), std::move(customFieldName));
    }
    
    static ErrorCode::Result<FilterCondition> createSet(LogEntryField f, FilterOperator o, std::vector<std::string> v) {
        if (o != FilterOperator::IN && o != FilterOperator::NOT_IN) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Set-based creation only valid for IN/NOT_IN operators."));
        }
        if (f == LogEntryField::CUSTOM) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Use createCustomSet for CUSTOM fields."));
        }
        return FilterCondition(f, o, std::move(v), FilterValueType::STRING, false, std::nullopt, std::nullopt);
    }

    static ErrorCode::Result<FilterCondition> createCustomSet(std::string customFieldName, FilterOperator o, std::vector<std::string> v) {
        if (o != FilterOperator::IN && o != FilterOperator::NOT_IN) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Set-based creation only valid for IN/NOT_IN operators."));
        }
        if (customFieldName.empty()) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Custom field name cannot be empty."));
        }
        return FilterCondition(LogEntryField::CUSTOM, o, std::move(v), FilterValueType::STRING, false, std::nullopt, std::move(customFieldName));
    }

    // --- Deprecated Public Constructors ---

    [[deprecated("Use createString instead.")]]
    FilterCondition(LogEntryField f, FilterOperator o, std::string v, bool cs = false)
        : field(f), op(o), value(std::move(v)), valueType(FilterValueType::STRING), caseSensitive(cs) {}

    [[deprecated("Use createTyped or createCustomTyped instead.")]]
    FilterCondition(LogEntryField f, FilterOperator o, std::string v, FilterValueType vt)
        : field(f), op(o), value(std::move(v)), valueType(vt) {}

    [[deprecated("Use createDatetime or createCustomDatetime instead.")]]
    FilterCondition(LogEntryField f, FilterOperator o, std::string v, const std::string& dtFormat)
        : field(f), op(o), value(std::move(v)), valueType(FilterValueType::DATETIME), datetimeFormat(dtFormat) {}

private:
    // Private constructor for internal use by factory functions
    FilterCondition(LogEntryField f, FilterOperator o, ValueVariant v, FilterValueType vt, bool cs, 
                    std::optional<std::string> dtFormat, std::optional<std::string> cf)
        : field(f), op(o), value(std::move(v)), valueType(vt), caseSensitive(cs), 
          datetimeFormat(std::move(dtFormat)), customField(std::move(cf)) {}

    friend ErrorCode::Result<void> from_json(const nlohmann::json& j, FilterCondition& fc, const std::string& current_path);
};

// --- JSON Serialization ---

inline void to_json(nlohmann::json& j, const FilterCondition& fc) {
    if (fc.field == LogEntryField::CUSTOM && fc.customField) {
        j["field"] = *fc.customField;
    } else {
        j["field"] = Utils::logEntryFieldToString(fc.field);
    }
    
    j["op"] = toString(fc.op);
    
    if (fc.op != FilterOperator::IS_PRESENT && fc.op != FilterOperator::IS_ABSENT) {
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

inline ErrorCode::Result<void> from_json(const nlohmann::json& j, FilterCondition& fc, const std::string& current_path = "/") {
    using namespace FilterJsonUtils;
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

    auto vtRes = getRequired<std::string>(j, "value_type", current_path);
    if (!vtRes) return std::unexpected(vtRes.error());
    auto typeOpt = fromStringToFilterValueType(*vtRes);
    if (!typeOpt) {
        return std::unexpected(makeError(Code::InvalidArgument, "Unrecognized value_type: " + *vtRes, current_path, "value_type"));
    }
                            fc.valueType = *typeOpt;
                    
                            if (fc.op == FilterOperator::IS_PRESENT || fc.op == FilterOperator::IS_ABSENT) {
                                fc.value = std::monostate{};
                            } else {
                                if (!j.contains("value")) {
                                    return std::unexpected(makeError(Code::InvalidArgument, "Missing required key: 'value'", current_path, "value"));
                                }
                                const auto& valJson = j.at("value");
                    
                                // Type checking
                                switch (fc.valueType) {
                                    case FilterValueType::STRING:
                                    case FilterValueType::DATETIME:
                                        if (!valJson.is_string()) return std::unexpected(makeError(Code::InvalidArgument, "Type mismatch: value for " + *vtRes + " must be a string.", current_path, "value"));
                                        fc.value = valJson.get<std::string>();
                                        break;
                                    case FilterValueType::INT:
                                        if (!valJson.is_number_integer()) return std::unexpected(makeError(Code::InvalidArgument, "Type mismatch: value for INT must be an integer.", current_path, "value"));
                                        fc.value = valJson.get<int64_t>();
                                        break;
                                    case FilterValueType::FLOAT:
                                    case FilterValueType::DOUBLE:
                                        if (!valJson.is_number()) return std::unexpected(makeError(Code::InvalidArgument, "Type mismatch: value for FLOAT/DOUBLE must be a number.", current_path, "value"));
                                        fc.value = valJson.get<double>();
                                        break;
                                    case FilterValueType::BOOL:
                                        if (!valJson.is_boolean()) return std::unexpected(makeError(Code::InvalidArgument, "Type mismatch: value for BOOL must be a boolean.", current_path, "value"));
                                        fc.value = valJson.get<bool>();
                                        break;
                                    case FilterValueType::LOG_LEVEL:
                                         if (!valJson.is_string()) return std::unexpected(makeError(Code::InvalidArgument, "Type mismatch: value for LOG_LEVEL must be a string.", current_path, "value"));
                                         fc.value = valJson.get<std::string>();
                                         break;
                                    case FilterValueType::AUTO: {
                                        if (valJson.is_array()) {
                                            std::vector<std::string> values;
                                            for (const auto& element : valJson) {
                                                if (element.is_string()) values.push_back(element.get<std::string>());
                                                else if (element.is_number()) values.push_back(std::to_string(element.get<double>()));
                                                else if (element.is_boolean()) values.push_back(element.get<bool>() ? "true" : "false");
                                                else return std::unexpected(makeError(Code::InvalidArgument, "Invalid type in 'value' array.", current_path, "value"));
                                            }
                                            fc.value = values;
                                        } else if (valJson.is_string()) {
                                            fc.value = valJson.get<std::string>();
                                        } else if (valJson.is_boolean()) {
                                            fc.value = valJson.get<bool>();
                                        } else if (valJson.is_number_integer()) {
                                            fc.value = valJson.get<int64_t>();
                                        } else if (valJson.is_number()) {
                                            fc.value = valJson.get<double>();
                                        } else if (valJson.is_null()) {
                                            fc.value = std::monostate{};
                                        } else {
                                            return std::unexpected(makeError(Code::InvalidArgument, "Invalid type for key: 'value'", current_path, "value"));
                                        }
                                        break;
                                    }
                                    default:
                                         return std::unexpected(makeError(Code::InvalidArgument, "Unsupported value_type.", current_path, "value_type"));
                                }
                            }

    if (j.contains("caseSensitive")) {
        const auto& cs_json = j.at("caseSensitive");
        if (!cs_json.is_boolean()) {
            return std::unexpected(makeError(Code::InvalidArgument, "Invalid type for 'caseSensitive', must be boolean.", current_path, "caseSensitive"));
        }
        fc.caseSensitive = cs_json.get<bool>();
    } else {
        fc.caseSensitive = false;
    }
    fc.datetimeFormat = getOptional<std::string>(j, "datetimeFormat");

    if (fc.valueType == FilterValueType::DATETIME && (!fc.datetimeFormat || fc.datetimeFormat->empty())) {
        return std::unexpected(makeError(Code::InvalidArgument, "DATETIME value_type requires a non-empty 'datetimeFormat'.", current_path, "datetimeFormat"));
    }
    
    if (fc.valueType == FilterValueType::DATETIME && fc.datetimeFormat && !std::get<std::string>(fc.value).empty()) {
        std::vector<std::string> formats = {*fc.datetimeFormat};
        auto validationResult = FilterJsonUtils::validateTimestamp(std::get<std::string>(fc.value), formats, current_path, "value");
        if (!validationResult) return std::unexpected(validationResult.error());
    }
    
    // Validation logic can be simplified or moved to a separate validation step/function
    // as type checking is now done during parsing.

    return {}; // Success
}

} // namespace filter
