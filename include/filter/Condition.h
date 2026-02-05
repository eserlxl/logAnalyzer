#ifndef FILTER_CONDITION_H
#define FILTER_CONDITION_H

#include <string>
#include <optional>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "core/Error.h"
#include "core/LogTypes.h"
#include "utils/UtilsCore.h"
#include "filter/Types.h" // Include the enums
#include "filter/EnumStringConversions.h" // For enum to string conversions
#include "filter/JsonUtils.h"

// New: Represents a single filtering condition (leaf node in the filter tree)
struct FilterCondition {
    LogEntryField field = LogEntryField::UNKNOWN;        // The LogEntry field to apply the filter to.
    FilterOperator op = FilterOperator::EQUALS;
    
    // The value to compare against. Always stored as a string.
    std::string value;
    
    FilterValueType valueType = FilterValueType::STRING; // How to interpret 'value'.
    bool caseSensitive = false; // Whether comparison is case-sensitive for string operations.

    // For DATETIME valueType, this provides the format for parsing 'value'.
    std::optional<std::string> datetimeFormat;

    // For CUSTOM field, this specifies the key in the customFields map.
    std::optional<std::string> customField;

    // Default constructor (needed for JSON)
    FilterCondition() = default;

    // Factory functions
    static FilterCondition createString(LogEntryField f, FilterOperator o, std::string v, bool cs = false) {
        return FilterCondition(f, o, std::move(v), FilterValueType::STRING, cs, std::nullopt, 
                               f == LogEntryField::CUSTOM ? std::optional<std::string>("") : std::nullopt);
    }

    static ErrorCode::Result<FilterCondition> createTyped(LogEntryField f, FilterOperator o, std::string v, FilterValueType vt) {
        if (vt == FilterValueType::DATETIME) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterCondition: DATETIME valueType requires a datetimeFormat."));
        }
        return FilterCondition(f, o, std::move(v), vt, false, std::nullopt,
                               f == LogEntryField::CUSTOM ? std::optional<std::string>("") : std::nullopt);
    }

    static FilterCondition createDatetime(LogEntryField f, FilterOperator o, std::string v, std::string dtFormat) {
        return FilterCondition(f, o, std::move(v), FilterValueType::DATETIME, false, std::move(dtFormat),
                               f == LogEntryField::CUSTOM ? std::optional<std::string>("") : std::nullopt);
    }

    // Public constructors for backward compatibility (non-throwing)
    FilterCondition(LogEntryField f, FilterOperator o, std::string v, bool cs = false)
        : field(f), op(o), value(std::move(v)), valueType(FilterValueType::STRING), caseSensitive(cs) {}

    FilterCondition(LogEntryField f, FilterOperator o, std::string v, FilterValueType vt)
        : field(f), op(o), value(std::move(v)), valueType(vt) {}

    FilterCondition(LogEntryField f, FilterOperator o, std::string v, const std::string& dtFormat)
        : field(f), op(o), value(std::move(v)), valueType(FilterValueType::DATETIME), datetimeFormat(dtFormat) {}

private:
    // Private constructor for internal use
    FilterCondition(LogEntryField f, FilterOperator o, std::string v, FilterValueType vt, bool cs, 
                    std::optional<std::string> dtFormat, std::optional<std::string> cf)
        : field(f), op(o), value(std::move(v)), valueType(vt), caseSensitive(cs), 
          datetimeFormat(std::move(dtFormat)), customField(std::move(cf)) {}

    friend ErrorCode::Result<void> from_json(const nlohmann::json& j, FilterCondition& fc, const std::string& current_path);
};

// Helper to convert FilterCondition to JSON
inline void to_json(nlohmann::json& j, const FilterCondition& fc) {
    // Field serialization: handle CUSTOM fields as string, others as their enum string representation
    if (fc.field == LogEntryField::CUSTOM && fc.customField) {
        j["field"] = *fc.customField;
    } else {
        j["field"] = Utils::logEntryFieldToString(fc.field);
    }
    
    j["op"] = toString(fc.op);
    j["value"] = fc.value; // Explicitly serialize value, even if empty
    j["value_type"] = toString(fc.valueType);
    j["caseSensitive"] = fc.caseSensitive; // Always serialize caseSensitive

    if (fc.datetimeFormat) {
        j["datetimeFormat"] = *fc.datetimeFormat;
    }
    // Only serialize customField if it's actually a CUSTOM field and has a value
    if (fc.field == LogEntryField::CUSTOM && fc.customField) {
        j["customField"] = *fc.customField;
    }
}

// Helper to convert JSON to FilterCondition
inline ErrorCode::Result<void> from_json(const nlohmann::json& j, FilterCondition& fc, const std::string& current_path = "/") {
    using namespace FilterJsonUtils;

    auto fieldStrRes = getRequired<std::string>(j, "field", current_path);
    if (!fieldStrRes) return std::unexpected(fieldStrRes.error());
    std::string fieldStr = *fieldStrRes;

    LogEntryField standardField = Utils::stringToLogEntryField(fieldStr);
    if (standardField == LogEntryField::UNKNOWN) {
        fc.field = LogEntryField::CUSTOM;
        fc.customField = fieldStr;
    } else {
        fc.field = standardField;
        fc.customField = std::nullopt;
    }
    
    // Explicit customField override if present, but only if we're dealing with a custom field
    if (fc.field == LogEntryField::CUSTOM && j.contains("customField")) {
        if (j.at("customField").is_string()) {
            fc.customField = j.at("customField").get<std::string>();
        } else if (j.at("customField").is_null()) {
            fc.customField = std::nullopt;
        } else {
            return std::unexpected(makeError(Code::InvalidArgument, "FilterCondition 'customField' must be a string or null.", current_path, "customField"));
        }
    }

    if (fc.field == LogEntryField::CUSTOM && (!fc.customField || fc.customField->empty())) {
         return std::unexpected(makeError(Code::InvalidArgument, "FilterCondition with field 'CUSTOM' requires a non-empty 'customField' key or field name.", current_path, "field"));
    }

    auto opStrRes = getRequired<std::string>(j, "op", current_path);
    if (!opStrRes) return std::unexpected(opStrRes.error());
    auto opOpt = fromStringToFilterOperator(*opStrRes);
    if (!opOpt) {
        return std::unexpected(makeError(Code::InvalidArgument, "Unrecognized operator: " + *opStrRes, current_path, "op"));
    }
    fc.op = *opOpt;

    auto valRes = getRequired<std::string>(j, "value", current_path);
    if (!valRes) return std::unexpected(valRes.error());
    fc.value = *valRes;

    auto vtRes = getRequired<nlohmann::json>(j, "value_type", current_path);
    if (!vtRes) return std::unexpected(vtRes.error());
    const auto& vtJson = *vtRes;

    if (vtJson.is_string()) {
        auto typeOpt = fromStringToFilterValueType(vtJson.get<std::string>());
        if (!typeOpt) {
            return std::unexpected(makeError(Code::InvalidArgument, "Unrecognized value_type: " + vtJson.get<std::string>(), current_path, "value_type"));
        }
        fc.valueType = *typeOpt;
    } else if (vtJson.is_number_integer()) {
        int vt_int = vtJson.get<int>();
        if (vt_int >= static_cast<int>(FilterValueType::STRING) && vt_int <= static_cast<int>(FilterValueType::IP_ADDRESS)) {
            fc.valueType = static_cast<FilterValueType>(vt_int);
        } else {
            return std::unexpected(makeError(Code::InvalidArgument, "Invalid integer for 'value_type'.", current_path, "value_type"));
        }
    } else {
        return std::unexpected(makeError(Code::InvalidArgument, "'value_type' must be a string or integer.", current_path, "value_type"));
    }

    fc.caseSensitive = getOptional<bool>(j, "caseSensitive").value_or(false);
    fc.datetimeFormat = getOptional<std::string>(j, "datetimeFormat");

    if (fc.valueType == FilterValueType::DATETIME && !fc.datetimeFormat) {
        return std::unexpected(makeError(Code::InvalidArgument, "DATETIME value_type requires 'datetimeFormat'.", current_path, "datetimeFormat"));
    }

    return {}; // Success
}

#endif // FILTER_CONDITION_H
