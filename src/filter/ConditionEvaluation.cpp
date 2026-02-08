// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "filter/ConditionEvaluation.h"
#include "filter/EnumStringConversions.h"
#include "utils/Core.h"
#include "utils/Time.h"
#include "utils/String.h"
#include "utils/Version.h"
#include "utils/IpAddress.h"
#include <charconv>
#include <regex>
#include <chrono>
#include <limits>
#include <cmath>
#include <numeric>
#include <nlohmann/json.hpp>
#include <set>

namespace filter {

namespace {
// Helper to convert string to bool
std::optional<bool> stringToBool(const std::string& s) {
    std::string lowerS = Utils::toLower(s);
    if (lowerS == "true" || lowerS == "1" || lowerS == "t" || lowerS == "yes") return true;
    if (lowerS == "false" || lowerS == "0" || lowerS == "f" || lowerS == "no") return false;
    return std::nullopt;
}

bool tryParseStrictInt64(std::string_view value, long long& out) {
    if (value.empty()) {
        return false;
    }
    auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), out);
    return ec == std::errc{} && ptr == value.data() + value.size();
}

bool tryParseStrictLongDouble(std::string_view value, long double& out) {
    if (value.empty()) {
        return false;
    }
    try {
        size_t idx = 0;
        const auto tmp = std::stold(std::string(value), &idx);
        if (idx != value.size()) {
            return false;
        }
        if (!std::isfinite(tmp)) {
            return false;
        }
        out = tmp;
        return true;
    } catch (...) {
        return false;
    }
}
} // namespace

std::optional<std::string> getFieldValue(const LogEntry& entry, const FilterCondition& cond) {
    switch (cond.field) {
        case LogEntryField::ID: return entry.id.has_value() ? std::optional(std::to_string(*entry.id)) : std::nullopt;
        case LogEntryField::TIMESTAMP: return entry.timestamp.has_value() ? std::optional(Utils::formatTimestamp(*entry.timestamp)) : std::nullopt;
        case LogEntryField::LEVEL: return Utils::logLevelToString(entry.level);
        case LogEntryField::SOURCE_FILE: return entry.sourceFile;
        case LogEntryField::LINE_NUMBER: return entry.sourceLineNumber.has_value() ? std::optional(std::to_string(*entry.sourceLineNumber)) : std::nullopt;
        case LogEntryField::THREAD_ID: return entry.threadId;
        case LogEntryField::MESSAGE: return entry.message;
        case LogEntryField::MODULE: return entry.module;
        case LogEntryField::HOST: return entry.host;
        case LogEntryField::CUSTOM:
            if (cond.customField) {
                auto it = entry.customFields.find(*cond.customField);
                if (it != entry.customFields.end()) return it->second;
            }
            return std::nullopt;
        default: return std::nullopt;
    }
}

ErrorCode::Result<bool> evaluateCondition(const FilterCondition& cond, const LogEntry& entry) {
    auto fieldValueOpt = getFieldValue(entry, cond);

    if (cond.op == FilterOperator::IS_PRESENT || cond.op == FilterOperator::IS_NOT_NULL) {
        return fieldValueOpt.has_value();
    }
    if (cond.op == FilterOperator::IS_ABSENT || cond.op == FilterOperator::IS_NULL) {
        return !fieldValueOpt.has_value();
    }

    if (!fieldValueOpt) {
        return false; // Return false instead of error for missing fields
    }
    const std::string& fieldValue = *fieldValueOpt;

    if (!std::holds_alternative<std::string>(cond.value) && (cond.op != FilterOperator::IN && cond.op != FilterOperator::NOT_IN)) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Value for this operator must be a single string."));
    }

    auto effectiveValueType = cond.inferredValueType.value_or(cond.valueType);
    if (std::holds_alternative<std::string>(cond.value) && effectiveValueType == FilterValueType::AUTO) {
        const auto& condValue = std::get<std::string>(cond.value);
        // Basic type inference if not already cached
        if (condValue == "true" || condValue == "false") {
            effectiveValueType = FilterValueType::BOOL;
        } else if (Utils::isNumeric(condValue)) {
            effectiveValueType = condValue.find('.') != std::string::npos ? FilterValueType::DOUBLE : FilterValueType::INT;
        } else if (Utils::parseIpAddress(condValue)) {
            effectiveValueType = FilterValueType::IP_ADDRESS;
        } else if (Utils::parseSemanticVersion(condValue)) {
            effectiveValueType = FilterValueType::VERSION;
        } else {
            effectiveValueType = FilterValueType::STRING;
        }
        cond.inferredValueType = effectiveValueType; // Cache it
    }

    if (cond.op == FilterOperator::IN || cond.op == FilterOperator::NOT_IN) {
        const std::vector<std::string>* valueSetPtr = nullptr;
        if (cond.parsedValue && std::holds_alternative<std::vector<std::string>>(*cond.parsedValue)) {
            valueSetPtr = &std::get<std::vector<std::string>>(*cond.parsedValue);
        } else if (std::holds_alternative<std::vector<std::string>>(cond.value)) {
            valueSetPtr = &std::get<std::vector<std::string>>(cond.value);
        } else {
            // Attempt to parse string as JSON array for backward compatibility
            const auto& jsonStr = std::get<std::string>(cond.value);
            try {
                auto j = nlohmann::json::parse(jsonStr);
                if (j.is_array()) {
                    std::vector<std::string> parsedSet;
                    for (const auto& item : j) {
                        if (item.is_string()) parsedSet.push_back(item.get<std::string>());
                        else if (item.is_number_integer()) parsedSet.push_back(std::to_string(item.get<long long>()));
                        else if (item.is_number()) parsedSet.push_back(std::to_string(item.get<double>()));
                        else if (item.is_boolean()) parsedSet.emplace_back(item.get<bool>() ? "true" : "false");
                    }
                    cond.parsedValue = std::move(parsedSet);
                    valueSetPtr = &std::get<std::vector<std::string>>(*cond.parsedValue);
                } else {
                    return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Operator IN/NOT_IN requires a JSON array."));
                }
            } catch (...) {
                return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Operator IN/NOT_IN requires a valid JSON array string."));
            }
        }
        
        bool found = false;
        for (const auto& val : *valueSetPtr) {
            if (cond.caseSensitive ? (fieldValue == val) : Utils::caseInsensitiveEquals(fieldValue, val)) {
                found = true;
                break;
            }
        }
        return (cond.op == FilterOperator::IN) ? found : !found;
    }

    const auto& condValue = std::get<std::string>(cond.value);

    switch (effectiveValueType) {
        case FilterValueType::STRING:
        {
             switch (cond.op) {
                case FilterOperator::EQUALS: return cond.caseSensitive ? (fieldValue == condValue) : Utils::caseInsensitiveEquals(fieldValue, condValue);
                case FilterOperator::EQUALS_I: return Utils::caseInsensitiveEquals(fieldValue, condValue);
                case FilterOperator::NOT_EQUALS: return cond.caseSensitive ? (fieldValue != condValue) : !Utils::caseInsensitiveEquals(fieldValue, condValue);
                case FilterOperator::NOT_EQUALS_I: return !Utils::caseInsensitiveEquals(fieldValue, condValue);
                case FilterOperator::CONTAINS: return cond.caseSensitive ? (fieldValue.find(condValue) != std::string::npos) : Utils::caseInsensitiveSearch(fieldValue, condValue);
                case FilterOperator::CONTAINS_I: return Utils::caseInsensitiveSearch(fieldValue, condValue);
                case FilterOperator::NOT_CONTAINS: return cond.caseSensitive ? (fieldValue.find(condValue) == std::string::npos) : !Utils::caseInsensitiveSearch(fieldValue, condValue);
                case FilterOperator::NOT_CONTAINS_I: return !Utils::caseInsensitiveSearch(fieldValue, condValue);
                case FilterOperator::STARTS_WITH: return cond.caseSensitive ? fieldValue.starts_with(condValue) : Utils::caseInsensitiveStarts(fieldValue, condValue);
                case FilterOperator::STARTS_WITH_I: return Utils::caseInsensitiveStarts(fieldValue, condValue);
                case FilterOperator::ENDS_WITH: return cond.caseSensitive ? fieldValue.ends_with(condValue) : Utils::caseInsensitiveEnds(fieldValue, condValue);
                case FilterOperator::ENDS_WITH_I: return Utils::caseInsensitiveEnds(fieldValue, condValue);
                case FilterOperator::REGEX: {
                    if (!cond.compiledRegex) {
                        try {
                            auto flags = cond.caseSensitive ? std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase;
                            cond.compiledRegex = std::regex(condValue, flags);
                        } catch (const std::regex_error& e) {
                            return std::unexpected(ErrorCode::Error(Code::InvalidRegex, "Invalid regex pattern '" + condValue + "': " + e.what()));
                        }
                    }
                    return std::regex_search(fieldValue, *cond.compiledRegex);
                }
                default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for STRING type."));
            }
            break;
        }
        case FilterValueType::INT: {
            long long fieldNum;
            long long condNum;
            if (!tryParseStrictInt64(fieldValue, fieldNum) || !tryParseStrictInt64(condValue, condNum)) {
                return std::unexpected(ErrorCode::Error(Code::ConversionError, "Integer conversion failed."));
            }
            switch (cond.op) {
                case FilterOperator::EQUALS: return fieldNum == condNum;
                case FilterOperator::NOT_EQUALS: return fieldNum != condNum;
                case FilterOperator::GREATER_THAN: return fieldNum > condNum;
                case FilterOperator::LESS_THAN: return fieldNum < condNum;
                case FilterOperator::GREATER_THAN_OR_EQUAL: return fieldNum >= condNum;
                case FilterOperator::LESS_THAN_OR_EQUAL: return fieldNum <= condNum;
                default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for INT type."));
            }
            break;
        }
        case FilterValueType::FLOAT:
        case FilterValueType::DOUBLE: {
            long double fieldNum;
            long double condNum;
            if (!tryParseStrictLongDouble(fieldValue, fieldNum) || !tryParseStrictLongDouble(condValue, condNum)) {
                return std::unexpected(ErrorCode::Error(Code::ConversionError, "Floating-point conversion failed."));
            }
            
            auto areAlmostEqual = [](long double a, long double b) {
                constexpr long double epsilon = std::numeric_limits<long double>::epsilon();
                // Use a relative epsilon for larger numbers, absolute for near-zero
                return std::fabsl(a - b) <= epsilon * std::max({1.0L, std::fabsl(a), std::fabsl(b)});
            };

            switch (cond.op) {
                case FilterOperator::EQUALS: return areAlmostEqual(fieldNum, condNum);
                case FilterOperator::NOT_EQUALS: return !areAlmostEqual(fieldNum, condNum);
                case FilterOperator::GREATER_THAN: return fieldNum > condNum && !areAlmostEqual(fieldNum, condNum);
                case FilterOperator::LESS_THAN: return fieldNum < condNum && !areAlmostEqual(fieldNum, condNum);
                case FilterOperator::GREATER_THAN_OR_EQUAL: return fieldNum > condNum || areAlmostEqual(fieldNum, condNum);
                case FilterOperator::LESS_THAN_OR_EQUAL: return fieldNum < condNum || areAlmostEqual(fieldNum, condNum);
                default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for numeric type."));
            }
            break;
        }
        case FilterValueType::BOOL: {
            auto fieldBool = stringToBool(fieldValue);
            auto condBool = stringToBool(condValue);
            if (!fieldBool.has_value() || !condBool.has_value()) {
                return std::unexpected(ErrorCode::Error(Code::ConversionError, "Invalid boolean value."));
            }
            switch (cond.op) {
                case FilterOperator::EQUALS: return *fieldBool == *condBool;
                case FilterOperator::NOT_EQUALS: return *fieldBool != *condBool;
                default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for BOOL type."));
            }
            break;
        }
        case FilterValueType::DATETIME: {
            std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> fieldTime;
            std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> condTime;
            if (cond.datetimeFormat.has_value() && !cond.datetimeFormat->empty()) {
                fieldTime = Utils::parseTimeWithFormats(fieldValue, {*cond.datetimeFormat});
                condTime = Utils::parseTimeWithFormats(condValue, {*cond.datetimeFormat});
            } else {
                fieldTime = Utils::parseTime(fieldValue);
                condTime = Utils::parseTime(condValue);
            }

            if (!fieldTime.has_value() || !condTime.has_value()) {
                 return std::unexpected(ErrorCode::Error(Code::ConversionError, "Invalid datetime value or format."));
            }
            switch (cond.op) {
                case FilterOperator::EQUALS: return *fieldTime == *condTime;
                case FilterOperator::NOT_EQUALS: return *fieldTime != *condTime;
                case FilterOperator::GREATER_THAN: return *fieldTime > *condTime;
                case FilterOperator::LESS_THAN: return *fieldTime < *condTime;
                case FilterOperator::GREATER_THAN_OR_EQUAL: return *fieldTime >= *condTime;
                case FilterOperator::LESS_THAN_OR_EQUAL: return *fieldTime <= *condTime;
                default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for DATETIME type."));
            }
            break;
        }
        case FilterValueType::IP_ADDRESS: {
            auto fieldIp = Utils::parseIpAddress(fieldValue);
            auto condIp = Utils::parseIpAddress(condValue);
            if (!fieldIp || !condIp) {
                return std::unexpected(ErrorCode::Error(Code::ConversionError, "Invalid IP address string."));
            }
            switch (cond.op) {
                case FilterOperator::EQUALS: return *fieldIp == *condIp;
                case FilterOperator::NOT_EQUALS: return *fieldIp != *condIp;
                case FilterOperator::GREATER_THAN: return *fieldIp > *condIp;
                case FilterOperator::LESS_THAN: return *fieldIp < *condIp;
                case FilterOperator::GREATER_THAN_OR_EQUAL: return *fieldIp >= *condIp;
                case FilterOperator::LESS_THAN_OR_EQUAL: return *fieldIp <= *condIp;
                default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for IP_ADDRESS type."));
            }
            break;
        }
        case FilterValueType::VERSION: {
            auto fieldVer = Utils::parseSemanticVersion(fieldValue);
            auto condVer = Utils::parseSemanticVersion(condValue);
            if (!fieldVer || !condVer) {
                return std::unexpected(ErrorCode::Error(Code::ConversionError, "Invalid version string."));
            }
            switch (cond.op) {
                case FilterOperator::EQUALS: return *fieldVer == *condVer;
                case FilterOperator::NOT_EQUALS: return *fieldVer != *condVer;
                case FilterOperator::GREATER_THAN: return *fieldVer > *condVer;
                case FilterOperator::LESS_THAN: return *fieldVer < *condVer;
                case FilterOperator::GREATER_THAN_OR_EQUAL: return *fieldVer >= *condVer;
                case FilterOperator::LESS_THAN_OR_EQUAL: return *fieldVer <= *condVer;
                default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for VERSION type."));
            }
        }
        case FilterValueType::LOG_LEVEL: {
            auto fieldLevel = Utils::stringToLogLevel(fieldValue);
            auto condLevel = Utils::stringToLogLevel(condValue);
            switch (cond.op) {
                case FilterOperator::EQUALS: return fieldLevel == condLevel;
                case FilterOperator::NOT_EQUALS: return fieldLevel != condLevel;
                case FilterOperator::GREATER_THAN: return fieldLevel > condLevel;
                case FilterOperator::LESS_THAN: return fieldLevel < condLevel;
                case FilterOperator::GREATER_THAN_OR_EQUAL: return fieldLevel >= condLevel;
                case FilterOperator::LESS_THAN_OR_EQUAL: return fieldLevel <= condLevel;
                default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for LOG_LEVEL type."));
            }
        }
        default:
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Unsupported value type."));
    }
}

} // namespace filter
