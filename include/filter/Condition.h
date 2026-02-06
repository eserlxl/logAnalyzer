#ifndef FILTER_CONDITION_H
#define FILTER_CONDITION_H

#include <string>
#include <optional>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "core/Error.h"
#include "core/LogTypes.h"
#include "utils/UtilsCore.h"
#include "utils/String.h"
#include "utils/IpAddress.h"
#include "utils/Version.h"
#include "filter/Types.h"
#include "filter/EnumStringConversions.h"
#include "filter/JsonUtils.h"
#include <iostream> // Added for std::cerr warning

// Represents a single filtering condition (leaf node in the filter tree)
struct FilterCondition {
    LogEntryField field = LogEntryField::UNKNOWN;
    FilterOperator op = FilterOperator::EQUALS;
    
    // New value representation
    using ValueVariant = std::variant<std::string, std::vector<std::string>>;
    ValueVariant value;
    
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
    
    // Serialize value based on variant type
    std::visit([&j](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::string>) {
            j["value"] = arg;
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            j["value"] = arg;
        }
    }, fc.value);

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

    fc.field = Utils::stringToLogEntryField(fieldStr); // First try standard fields

    if (fc.field == LogEntryField::UNKNOWN) {
        if (fieldStr.empty()) {
            return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Field name cannot be empty.", current_path, "field"));
        }
        fc.field = LogEntryField::CUSTOM;
        // Ensure customField is assigned a valid copy of the string
        fc.customField = fieldStr; 
    }
    // If standardField was found, fc.field is already set correctly.
    // If fc.field is CUSTOM, fc.customField is now populated.
    // The original code did this assignment but a segfault implies issues in handling.
    // Explicitly ensuring a copy assignment here.

    auto opStrRes = FilterJsonUtils::getRequired<std::string>(j, "op", current_path);
    if (!opStrRes) return std::unexpected(opStrRes.error());
    auto opOpt = fromStringToFilterOperator(*opStrRes);
    if (!opOpt) {
        return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Unrecognized operator: " + *opStrRes, current_path, "op"));
    }
    fc.op = *opOpt;

    // Handle 'value' which can be string, number, boolean, or an array.
    // Allow missing value for IS_NULL / IS_NOT_NULL checks.
    if (!j.contains("value")) {
        if (fc.op == FilterOperator::IS_NULL || fc.op == FilterOperator::IS_NOT_NULL) {
            fc.value = ""; // Default construct a string in the variant.
        } else {
            return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Missing required key: 'value'", current_path, "value"));
        }
    } else {
        const auto& valJson = j.at("value");
        if (valJson.is_array()) {
            // Only IN and NOT_IN operators support array values.
            if (fc.op != FilterOperator::IN && fc.op != FilterOperator::NOT_IN) {
                return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Array value is only supported for 'IN' and 'NOT_IN' operators.", current_path, "value"));
            }
            std::vector<std::string> values;
            for (const auto& element : valJson) {
                if (element.is_string()) {
                    values.push_back(element.get<std::string>());
                } else if (element.is_number()) {
                    values.push_back(std::to_string(element.get<double>()));
                } else if (element.is_boolean()) {
                    values.push_back(element.get<bool>() ? "true" : "false");
                } else {
                    return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Invalid type in 'value' array; only strings, numbers, and booleans are supported.", current_path, "value"));
                }
            }
            fc.value = values;
        } else if (valJson.is_string()) {
            fc.value = valJson.get<std::string>();
        } else if (valJson.is_boolean()) {
            fc.value = valJson.get<bool>() ? "true" : "false";
        } else if (valJson.is_number_integer()) {
            fc.value = std::to_string(valJson.get<int64_t>());
        } else if (valJson.is_number_unsigned()) {
            fc.value = std::to_string(valJson.get<uint64_t>());
        } else if (valJson.is_number()) {
            fc.value = std::to_string(valJson.get<double>());
        } else if (valJson.is_null()) {
            // Allow null if operator is IS_NULL or IS_NOT_NULL.
            if (fc.op == FilterOperator::IS_NULL || fc.op == FilterOperator::IS_NOT_NULL) {
                fc.value = ""; // Represents an empty value.
            } else {
                return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Invalid type for key: 'value' (null not allowed for this operator)", current_path, "value"));
            }
        } else {
            return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Invalid type for key: 'value'", current_path, "value"));
        }
    }

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
        if (vt_int >= static_cast<int>(FilterValueType::STRING) && vt_int <= static_cast<int>(FilterValueType::FLOAT)) {
            fc.valueType = static_cast<FilterValueType>(vt_int);
        } else {
            return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Invalid integer for 'value_type'.", current_path, "value_type"));
        }
    } else {
        return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "'value_type' must be a string or integer.", current_path, "value_type"));
    }

    // If op is IS_NULL or IS_NOT_NULL, the value must be an empty string.
    if (fc.op == FilterOperator::IS_NULL || fc.op == FilterOperator::IS_NOT_NULL) {
        fc.value = "";
    }
    
    // The validation logic below assumes value is a string. This needs to be adjusted.
    if (std::holds_alternative<std::string>(fc.value)) {
        const std::string& singleValue = std::get<std::string>(fc.value);
        if (fc.valueType == FilterValueType::BOOL) {
            std::string lowerVal = Utils::toLower(singleValue);
            if (lowerVal != "true" && lowerVal != "false" && lowerVal != "1" && lowerVal != "0") {
                 return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Invalid boolean value: " + singleValue, current_path, "value"));
            }
        } else if (fc.valueType == FilterValueType::INT) {
            try {
                size_t idx;
                std::stoll(singleValue, &idx);
                if (idx != singleValue.length()) {
                     return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Invalid integer value: " + singleValue, current_path, "value"));
                }
            } catch (...) {
                return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Invalid integer value: " + singleValue, current_path, "value"));
            }
        } else if (fc.valueType == FilterValueType::DOUBLE || fc.valueType == FilterValueType::FLOAT) {
            try {
                size_t idx;
                std::stod(singleValue, &idx);
                if (idx != singleValue.length()) {
                     return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Invalid float value: " + singleValue, current_path, "value"));
                }
            } catch (...) {
                return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Invalid float value: " + singleValue, current_path, "value"));
            }
        } else if (fc.valueType == FilterValueType::IP_ADDRESS) {
            if (!Utils::parseIpAddress(singleValue)) {
                return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Invalid IP address: " + singleValue, current_path, "value"));
            }
        } else if (fc.valueType == FilterValueType::VERSION) {
            if (!Utils::parseSemanticVersion(singleValue)) {
                return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Invalid version string: " + singleValue, current_path, "value"));
            }
        }
    }
    // Note: Type validation for elements within a std::vector<std::string> is not performed here.
    // The evaluation logic will handle parsing of individual elements. This is consistent
    // with how single string values are handled (validation vs. parsing at evaluation).

    if (j.contains("caseSensitive")) {
        if (!j.at("caseSensitive").is_boolean() && !j.at("caseSensitive").is_null()) {
             return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "Invalid type for key 'caseSensitive'", current_path, "caseSensitive"));
        }
        fc.caseSensitive = j.at("caseSensitive").is_null() ? false : j.at("caseSensitive").get<bool>();
    } else {
        fc.caseSensitive = false;
    }

    fc.datetimeFormat = FilterJsonUtils::getOptional<std::string>(j, "datetimeFormat");

    if (fc.valueType == FilterValueType::DATETIME && (!fc.datetimeFormat || fc.datetimeFormat->empty())) {
        return std::unexpected(FilterJsonUtils::makeError(Code::InvalidArgument, "DATETIME value_type requires a non-empty 'datetimeFormat'.", current_path, "datetimeFormat"));
    }

    return {}; // Success
}

#endif // FILTER_CONDITION_H
