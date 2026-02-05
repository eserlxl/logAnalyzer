#ifndef FILTER_CONDITION_H
#define FILTER_CONDITION_H

#include <string>
#include <optional>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "core/Error.h"
#include "core/LogTypes.h"
#include "utils/UtilsCore.h"
#include "filter/Types.h"
#include "filter/EnumStringConversions.h"
#include "filter/JsonUtils.h"
#include <iostream> // Added for std::cerr warning

// Represents a single filtering condition (leaf node in the filter tree)
struct FilterCondition {
    LogEntryField field = LogEntryField::UNKNOWN;
    FilterOperator op = FilterOperator::EQUALS;
    
    std::string value;
    
    FilterValueType valueType = FilterValueType::STRING;
    bool caseSensitive = false;

    std::optional<std::string> datetimeFormat;
    std::optional<std::string> customField; // Added based on audit report

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
    FilterCondition(LogEntryField f, FilterOperator o, std::string v, FilterValueType vt, bool cs, 
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
    j["value"] = fc.value;
    j["value_type"] = toString(fc.valueType);
    j["caseSensitive"] = fc.caseSensitive;

    if (fc.datetimeFormat) {
        j["datetimeFormat"] = *fc.datetimeFormat;
    }
}

inline ErrorCode::Result<void> from_json(const nlohmann::json& j, FilterCondition& fc, const std::string& current_path = "/") {
    auto fieldStrRes = FilterJsonUtils::getRequired<std::string>(j, "field", current_path);
    if (!fieldStrRes) return std::unexpected(fieldStrRes.error());
    std::string fieldStr = *fieldStrRes;

    LogEntryField standardField = Utils::stringToLogEntryField(fieldStr);
    if (standardField == LogEntryField::UNKNOWN) {
        if (fieldStr.empty()) {
            return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Field name cannot be empty.", current_path, "field"));
        }
        fc.field = LogEntryField::CUSTOM;
        fc.customField = fieldStr;
    } else {
        fc.field = standardField;
    }

    auto opStrRes = FilterJsonUtils::getRequired<std::string>(j, "op", current_path);
    if (!opStrRes) return std::unexpected(opStrRes.error());
    auto opOpt = fromStringToFilterOperator(*opStrRes);
    if (!opOpt) {
        return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Unrecognized operator: " + *opStrRes, current_path, "op"));
    }
    fc.op = *opOpt;

    auto valRes = FilterJsonUtils::getRequired<std::string>(j, "value", current_path);
    if (!valRes) return std::unexpected(valRes.error());
    fc.value = *valRes;

    auto vtRes = FilterJsonUtils::getRequired<nlohmann::json>(j, "value_type", current_path);
    if (!vtRes) return std::unexpected(vtRes.error());
    const auto& vtJson = *vtRes;

    if (vtJson.is_string()) {
        auto typeOpt = fromStringToFilterValueType(vtJson.get<std::string>());
        if (!typeOpt) {
            return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Unrecognized value_type: " + vtJson.get<std::string>(), current_path, "value_type"));
        }
        fc.valueType = *typeOpt;
    } else if (vtJson.is_number_integer()) {
        // For backward compatibility
        std::cerr << "Warning: Using integer for 'value_type' is deprecated and may be removed in future versions. Please use string representations.\n";
        int vt_int = vtJson.get<int>();
        if (vt_int >= static_cast<int>(FilterValueType::STRING) && vt_int <= static_cast<int>(FilterValueType::IP_ADDRESS)) {
            fc.valueType = static_cast<FilterValueType>(vt_int);
        } else {
            return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Invalid integer for 'value_type'.", current_path, "value_type"));
        }
    } else {
        return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "'value_type' must be a string or integer.", current_path, "value_type"));
    }

    fc.caseSensitive = FilterJsonUtils::getOptional<bool>(j, "caseSensitive").value_or(false);
    fc.datetimeFormat = FilterJsonUtils::getOptional<std::string>(j, "datetimeFormat");

    if (fc.valueType == FilterValueType::DATETIME && !fc.datetimeFormat) {
        return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "DATETIME value_type requires 'datetimeFormat'.", current_path, "datetimeFormat"));
    }

    return {}; // Success
}

#endif // FILTER_CONDITION_H
