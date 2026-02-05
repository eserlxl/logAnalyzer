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

// New: Represents a single filtering condition (leaf node in the filter tree)
struct FilterCondition {
    LogEntryField field = LogEntryField::UNKNOWN;        // The LogEntry field to apply the filter to.
    FilterOperator op;
    
    // The value to compare against. Always stored as a string.
    // For NUMERIC and DATETIME valueTypes, this string representation will be parsed
    // into the respective type at the point of filter evaluation, not at construction.
    std::string value;
    
    FilterValueType valueType = FilterValueType::STRING; // How to interpret 'value'.
    bool caseSensitive = false; // Whether comparison is case-sensitive for string operations.

    // For DATETIME valueType, this provides the format for parsing 'value'.
    std::optional<std::string> datetimeFormat;

    // For CUSTOM field, this specifies the key in the customFields map.
    std::optional<std::string> customField;

    // Default constructor
    FilterCondition() = default;

    // Constructor for string-based conditions
    FilterCondition(LogEntryField f, FilterOperator o, std::string v, bool cs = false)
        : field(f), op(o), value(std::move(v)), valueType(FilterValueType::STRING), caseSensitive(cs) {}

    // Constructor for numeric conditions
    FilterCondition(LogEntryField f, FilterOperator o, std::string v, FilterValueType vt)
        : field(f), op(o), value(std::move(v)), valueType(vt) {
        if (vt == FilterValueType::DATETIME) {
            throw std::invalid_argument("FilterCondition: DATETIME valueType requires a datetimeFormat.");
        }
    }

    // Constructor for datetime conditions with format
    FilterCondition(LogEntryField f, FilterOperator o, std::string v, const std::string& dtFormat)
        : field(f), op(o), value(std::move(v)), valueType(FilterValueType::DATETIME), datetimeFormat(dtFormat) {}
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
    auto make_error = [&](Code c, const std::string& msg, const std::string& field_name) {
        std::string path = current_path.empty() || current_path == "/" ? "/" + field_name : current_path + "/" + field_name;
        return ErrorCode::Error(c, msg, path);
    };

    if (!j.contains("field") || !j.at("field").is_string()) {
        return std::unexpected(make_error(Code::InvalidArgument, "FilterCondition is missing or has invalid 'field'.", "field"));
    }
    std::string fieldStr = j.at("field").get<std::string>();
    LogEntryField standardField = Utils::stringToLogEntryField(fieldStr);
    
    if (standardField == LogEntryField::UNKNOWN) {
        fc.field = LogEntryField::CUSTOM;
        fc.customField = fieldStr;
    } else {
        fc.field = standardField;
    }
    
    if (fc.field == LogEntryField::CUSTOM && !fc.customField.has_value() && !j.contains("customField")) {
        return std::unexpected(make_error(Code::InvalidArgument, "FilterCondition with field 'CUSTOM' requires a 'customField' key.", "customField"));
    }

    if (fc.field == LogEntryField::CUSTOM && j.contains("customField")) {
        if (j.at("customField").is_string()) {
            fc.customField = j.at("customField").get<std::string>();
        } else if (j.at("customField").is_null()) {
            fc.customField = std::nullopt;
        } else {
            return std::unexpected(make_error(Code::InvalidArgument, "FilterCondition 'customField' must be a string or null.", "customField"));
        }
    }

    if (!j.contains("op") || !j.at("op").is_string()) {
        return std::unexpected(make_error(Code::InvalidArgument, "FilterCondition is missing or has invalid 'op'.", "op"));
    }
    auto opOpt = fromStringToFilterOperator(j.at("op").get<std::string>());
    if (!opOpt) {
        return std::unexpected(make_error(Code::InvalidArgument, "FilterCondition has an unrecognized 'op' string: " + j.at("op").get<std::string>(), "op"));
    }
    fc.op = *opOpt;

    if (!j.contains("value") || !j.at("value").is_string()) {
        return std::unexpected(make_error(Code::InvalidArgument, "FilterCondition is missing or has invalid 'value'.", "value"));
    }
    fc.value = j.at("value").get<std::string>();

    if (!j.contains("value_type")) {
        return std::unexpected(make_error(Code::InvalidArgument, "FilterCondition is missing 'value_type'.", "value_type"));
    }

    if (j.at("value_type").is_string()) {
        auto typeOpt = fromStringToFilterValueType(j.at("value_type").get<std::string>());
        if (!typeOpt) {
            return std::unexpected(make_error(Code::InvalidArgument, "FilterCondition has an unrecognized 'value_type' string: " + j.at("value_type").get<std::string>(), "value_type"));
        }
        fc.valueType = *typeOpt;
    } else if (j.at("value_type").is_number_integer()) {
        int vt_int = j.at("value_type").get<int>();
            if (vt_int >= static_cast<int>(FilterValueType::STRING) && vt_int <= static_cast<int>(FilterValueType::IP_ADDRESS)) {
                fc.valueType = static_cast<FilterValueType>(vt_int);
            } else {
                return std::unexpected(make_error(Code::InvalidArgument, "FilterCondition has an invalid integer for 'value_type'. Must be between " + std::to_string(static_cast<int>(FilterValueType::STRING)) + " and " + std::to_string(static_cast<int>(FilterValueType::IP_ADDRESS)) + ".", "value_type"));
            }
    } else {
        return std::unexpected(make_error(Code::InvalidArgument, "FilterCondition 'value_type' must be a string or integer.", "value_type"));
    }

    if (j.contains("caseSensitive")) {
        if (!j.at("caseSensitive").is_boolean()) {
            return std::unexpected(make_error(Code::InvalidArgument, "'caseSensitive' must be a boolean.", "caseSensitive"));
        }
        fc.caseSensitive = j.at("caseSensitive").get<bool>();
    } else {
        fc.caseSensitive = false; // Default to false if not present
    }

    if (j.contains("datetimeFormat")) {
        if (j.at("datetimeFormat").is_string()) {
            fc.datetimeFormat = j.at("datetimeFormat").get<std::string>();
        } else if (!j.at("datetimeFormat").is_null()) {
            return std::unexpected(make_error(Code::InvalidArgument, "FilterCondition 'datetimeFormat' must be a string or null.", "datetimeFormat"));
        }
    }

    if (fc.valueType == FilterValueType::DATETIME && !fc.datetimeFormat.has_value()) {
        return std::unexpected(make_error(Code::InvalidArgument, "FilterCondition with DATETIME 'value_type' requires a 'datetimeFormat'.", "datetimeFormat"));
    }

    return {}; // Success
}

#endif // FILTER_CONDITION_H
