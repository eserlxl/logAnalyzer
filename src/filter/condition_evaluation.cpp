// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "filter/condition_evaluation.h"
#include "filter/enum_string_conversions.h"
#include "utils/core.h"
#include "utils/time.h"
#include "utils/string.h"
#include "utils/version.h"
#include "utils/ip_address.h"
#include <charconv>
#include <regex>
#include <chrono>
#include <limits>
#include <cmath>
#include <cctype>
#include <numeric>
#include <nlohmann/json.hpp>

namespace filter {

namespace {
// Helper to convert string_view to bool
std::optional<bool> stringToBool(std::string_view s) {
    if (s.empty()) return std::nullopt;
    if (Utils::caseInsensitiveEquals(s, "true") || s == "1" || Utils::caseInsensitiveEquals(s, "t") || Utils::caseInsensitiveEquals(s, "yes")) return true;
    if (Utils::caseInsensitiveEquals(s, "false") || s == "0" || Utils::caseInsensitiveEquals(s, "f") || Utils::caseInsensitiveEquals(s, "no")) return false;
    return std::nullopt;
}

// Helper to parse a strict int64 from a string_view
bool tryParseStrictInt64(std::string_view value, int64_t& out) {
    if (value.empty()) {
        return false;
    }
    auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), out);
    return ec == std::errc{} && ptr == value.data() + value.size();
}

// Helper to parse a strict long double from a string_view
bool tryParseStrictLongDouble(std::string_view value, long double& out) {
    if (value.empty()) {
        return false;
    }
    // std::from_chars for double is available in C++17, but might not be fully implemented in all toolchains (e.g., GCC < 11).
    // Using it is still preferable to std::stold for performance and correctness.
    auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), out);
    if (ec != std::errc{}) {
        return false;
    }
    // Ensure the entire string was parsed.
    return ptr == value.data() + value.size();
}

// Robust floating-point comparison
bool areAlmostEqual(long double a, long double b) {
    // Check for exact equality first, which handles infinities.
    if (a == b) return true;

    long double diff = std::fabsl(a - b);
    
    // For numbers close to zero, rely on an absolute epsilon.
    // DBL_EPSILON is too small for many cases. A value like 1e-9 is a common choice.
    constexpr long double absoluteEpsilon = 1e-9L;
    if (diff < absoluteEpsilon) return true;

    // For larger numbers, use a relative epsilon.
    return diff <= absoluteEpsilon * std::max(std::fabsl(a), std::fabsl(b));
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

    // --- Presence Checks ---
    if (cond.op == FilterOperator::IS_PRESENT || cond.op == FilterOperator::IS_NOT_NULL) {
        return fieldValueOpt.has_value();
    }
    if (cond.op == FilterOperator::IS_ABSENT || cond.op == FilterOperator::IS_NULL) {
        return !fieldValueOpt.has_value();
    }

    // If field is absent, all other operators evaluate to false.
    if (!fieldValueOpt) {
        return false;
    }
    const std::string& fieldValue = *fieldValueOpt;

    // --- Set Operations (IN / NOT_IN) ---
    if (cond.op == FilterOperator::IN || cond.op == FilterOperator::NOT_IN) {
        const std::vector<std::string>* valueSet = std::get_if<std::vector<std::string>>(&cond.value);
        std::vector<std::string> parsedValues;

        if (!valueSet) {
            const std::string* valStr = std::get_if<std::string>(&cond.value);
            if (valStr) {
                try {
                    auto j = nlohmann::json::parse(*valStr);
                    if (j.is_array()) {
                        for (const auto& element : j) {
                            if (element.is_string()) parsedValues.push_back(element.get<std::string>());
                            else if (element.is_number_integer()) parsedValues.push_back(std::to_string(element.get<int64_t>()));
                            else if (element.is_number()) parsedValues.push_back(std::to_string(element.get<double>()));
                            else if (element.is_boolean()) parsedValues.push_back(element.get<bool>() ? "true" : "false");
                            else return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid type in 'value' array for IN/NOT_IN."));
                        }
                        valueSet = &parsedValues;
                    } else {
                        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Value for IN/NOT_IN must be an array."));
                    }
                } catch (...) {
                    return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Value for IN/NOT_IN must be a valid JSON array string or a vector of strings."));
                }
            } else {
                return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Value for IN/NOT_IN must be an array."));
            }
        }
        
        bool found = false;
        for (const auto& val : *valueSet) {
             // The comparison here is always string-based as per the JSON parsing logic
            if (cond.caseSensitive ? (fieldValue == val) : Utils::caseInsensitiveEquals(fieldValue, val)) {
                found = true;
                break;
            }
        }
        return (cond.op == FilterOperator::IN) ? found : !found;
    }


    // --- Typed Evaluations ---
    // The visitor pattern allows us to handle the typed `cond.value` from the variant.
    return std::visit([&](auto&& condValue) -> ErrorCode::Result<bool> {
        using T = std::decay_t<decltype(condValue)>;

        // These types compare against the pre-parsed cond.value
        if constexpr (std::is_same_v<T, int64_t>) {
            int64_t fieldNum;
            if (!tryParseStrictInt64(fieldValue, fieldNum)) return false; // Not an error, just a type mismatch
            
            switch (cond.op) {
                case FilterOperator::EQUALS: return fieldNum == condValue;
                case FilterOperator::NOT_EQUALS: return fieldNum != condValue;
                case FilterOperator::GREATER_THAN: return fieldNum > condValue;
                case FilterOperator::LESS_THAN: return fieldNum < condValue;
                case FilterOperator::GREATER_THAN_OR_EQUAL: return fieldNum >= condValue;
                case FilterOperator::LESS_THAN_OR_EQUAL: return fieldNum <= condValue;
                default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for INT type."));
            }
        } 
        else if constexpr (std::is_same_v<T, double>) {
            long double fieldNum;
            // Use our improved parser. Note: `condValue` is double, we cast to long double for consistency.
            if (!tryParseStrictLongDouble(fieldValue, fieldNum)) return false;
            long double condNum = condValue;

            switch (cond.op) {
                case FilterOperator::EQUALS: return areAlmostEqual(fieldNum, condNum);
                case FilterOperator::NOT_EQUALS: return !areAlmostEqual(fieldNum, condNum);
                case FilterOperator::GREATER_THAN: return fieldNum > condNum && !areAlmostEqual(fieldNum, condNum);
                case FilterOperator::LESS_THAN: return fieldNum < condNum && !areAlmostEqual(fieldNum, condNum);
                case FilterOperator::GREATER_THAN_OR_EQUAL: return fieldNum >= condNum || areAlmostEqual(fieldNum, condNum);
                case FilterOperator::LESS_THAN_OR_EQUAL: return fieldNum < condNum || areAlmostEqual(fieldNum, condNum);
                default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for DOUBLE type."));
            }
        }
        else if constexpr (std::is_same_v<T, bool>) {
            auto fieldBool = stringToBool(fieldValue);
            if (!fieldBool.has_value()) return false;

            switch (cond.op) {
                case FilterOperator::EQUALS: return *fieldBool == condValue;
                case FilterOperator::NOT_EQUALS: return *fieldBool != condValue;
                default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for BOOL type."));
            }
        }
        // These types still require parsing the field value at evaluation time, but the condition value is a string.
        else if constexpr (std::is_same_v<T, std::string>) {
             switch (cond.valueType) {
                case FilterValueType::STRING:
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
                            std::regex re;
                            if (cond.compiledRegex) {
                                re = *cond.compiledRegex;
                            } else {
                                try {
                                    auto flags = cond.caseSensitive ? std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase;
                                    re = std::regex(condValue, flags);
                                } catch (const std::regex_error& e) {
                                    return std::unexpected(ErrorCode::Error(Code::InvalidRegex, std::string("Invalid regex in condition: ") + e.what()));
                                }
                            }
                            return std::regex_search(fieldValue, re);
                        }
                        default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for STRING type."));
                    }
                
                case FilterValueType::INT: {
                    int64_t fieldNum;
                    int64_t condNum;
                    if (!tryParseStrictInt64(fieldValue, fieldNum)) {
                        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Field value is not a valid INT: " + fieldValue));
                    }
                    if (!tryParseStrictInt64(condValue, condNum)) {
                        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Condition value is not a valid INT: " + condValue));
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
                }

                case FilterValueType::DOUBLE: {
                    long double fieldNum;
                    long double condNum;
                    if (!tryParseStrictLongDouble(fieldValue, fieldNum) || !std::isfinite(fieldNum)) {
                        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Field value is not a valid finite DOUBLE: " + fieldValue));
                    }
                    if (!tryParseStrictLongDouble(condValue, condNum) || !std::isfinite(condNum)) {
                        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Condition value is not a valid finite DOUBLE: " + condValue));
                    }

                    switch (cond.op) {
                        case FilterOperator::EQUALS: return areAlmostEqual(fieldNum, condNum);
                        case FilterOperator::NOT_EQUALS: return !areAlmostEqual(fieldNum, condNum);
                        case FilterOperator::GREATER_THAN: return fieldNum > condNum && !areAlmostEqual(fieldNum, condNum);
                        case FilterOperator::LESS_THAN: return fieldNum < condNum && !areAlmostEqual(fieldNum, condNum);
                        case FilterOperator::GREATER_THAN_OR_EQUAL: return fieldNum >= condNum || areAlmostEqual(fieldNum, condNum);
                        case FilterOperator::LESS_THAN_OR_EQUAL: return fieldNum < condNum || areAlmostEqual(fieldNum, condNum);
                        default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for DOUBLE type."));
                    }
                }

                case FilterValueType::BOOL: {
                    auto fieldBool = stringToBool(fieldValue);
                    auto condBool = stringToBool(condValue);
                    if (!fieldBool.has_value()) {
                         return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Field value is not a valid BOOL: " + fieldValue));
                    }
                    if (!condBool.has_value()) {
                         return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Condition value is not a valid BOOL: " + condValue));
                    }

                    switch (cond.op) {
                        case FilterOperator::EQUALS: return *fieldBool == *condBool;
                        case FilterOperator::NOT_EQUALS: return *fieldBool != *condBool;
                        default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for BOOL type."));
                    }
                }

                case FilterValueType::DATETIME: {
                    std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> fieldTime;
                    std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> condTime;
                    auto formats = cond.datetimeFormat.has_value() ? std::vector<std::string>{*cond.datetimeFormat} : std::vector<std::string>{};
                    
                    fieldTime = Utils::parseTimeWithFormats(fieldValue, formats);
                    condTime = Utils::parseTimeWithFormats(condValue, formats);

                    if (!fieldTime) return std::unexpected(fieldTime.error());
                    if (!condTime) return std::unexpected(condTime.error());

                    switch (cond.op) {
                        case FilterOperator::EQUALS: return *fieldTime == *condTime;
                        case FilterOperator::NOT_EQUALS: return *fieldTime != *condTime;
                        case FilterOperator::GREATER_THAN: return *fieldTime > *condTime;
                        case FilterOperator::LESS_THAN: return *fieldTime < *condTime;
                        case FilterOperator::GREATER_THAN_OR_EQUAL: return *fieldTime >= *condTime;
                        case FilterOperator::LESS_THAN_OR_EQUAL: return *fieldTime <= *condTime;
                        default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for DATETIME type."));
                    }
                }
                
                case FilterValueType::IP_ADDRESS: {
                    auto fieldIp = Utils::parseIpAddress(fieldValue);
                    if (!fieldIp) return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Field value is not a valid IP address: " + fieldValue));
                    auto condIp = Utils::parseIpAddress(condValue);
                    if (!condIp) return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Condition value is not a valid IP address: " + condValue));
                    switch (cond.op) {
                        case FilterOperator::EQUALS: return *fieldIp == *condIp;
                        case FilterOperator::NOT_EQUALS: return *fieldIp != *condIp;
                        case FilterOperator::GREATER_THAN: return *fieldIp > *condIp;
                        case FilterOperator::LESS_THAN: return *fieldIp < *condIp;
                        case FilterOperator::GREATER_THAN_OR_EQUAL: return *fieldIp >= *condIp;
                        case FilterOperator::LESS_THAN_OR_EQUAL: return *fieldIp <= *condIp;
                        default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for IP_ADDRESS type."));
                    }
                }

                case FilterValueType::VERSION: {
                    auto fieldVer = Utils::parseSemanticVersion(fieldValue);
                    if (!fieldVer) {
                        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Field value is not a valid Semantic Version: " + fieldValue));
                    }
                    auto condVer = Utils::parseSemanticVersion(condValue);
                    if (!condVer) {
                        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Condition value is not a valid Semantic Version: " + condValue));
                    }
                    switch (cond.op) {
                        case FilterOperator::EQUALS: return (*fieldVer == *condVer);
                        case FilterOperator::NOT_EQUALS: return (*fieldVer != *condVer);
                        case FilterOperator::GREATER_THAN: return (*fieldVer > *condVer);
                        case FilterOperator::LESS_THAN: return (*fieldVer < *condVer);
                        case FilterOperator::GREATER_THAN_OR_EQUAL: return (*fieldVer >= *condVer);
                        case FilterOperator::LESS_THAN_OR_EQUAL: return (*fieldVer <= *condVer);
                        default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for VERSION type."));
                    }
                }
                
                case FilterValueType::LOG_LEVEL: {
                    auto fieldLevel = Utils::stringToLogLevel(fieldValue);
                    auto condLevel = Utils::stringToLogLevel(condValue);
                    // This comparison relies on the underlying enum integer values.
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
                
                case FilterValueType::REGEX: {
                    std::regex re;
                    if (cond.compiledRegex) {
                        re = *cond.compiledRegex;
                    } else {
                        try {
                            auto flags = cond.caseSensitive ? std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase;
                            re = std::regex(condValue, flags);
                        } catch (const std::regex_error& e) {
                            return std::unexpected(ErrorCode::Error(Code::InvalidRegex, std::string("Invalid regex in condition: ") + e.what()));
                        }
                    }
                    return std::regex_search(fieldValue, re);
                }

                case FilterValueType::AUTO: {
                    // Simple inference for AUTO: try INT, then DOUBLE, then BOOL, then fallback to STRING
                    int64_t iVal;
                    if (tryParseStrictInt64(condValue, iVal)) {
                        int64_t fVal;
                        if (tryParseStrictInt64(fieldValue, fVal)) {
                            switch (cond.op) {
                                case FilterOperator::EQUALS: return fVal == iVal;
                                case FilterOperator::NOT_EQUALS: return fVal != iVal;
                                case FilterOperator::GREATER_THAN: return fVal > iVal;
                                case FilterOperator::LESS_THAN: return fVal < iVal;
                                case FilterOperator::GREATER_THAN_OR_EQUAL: return fVal >= iVal;
                                case FilterOperator::LESS_THAN_OR_EQUAL: return fVal <= iVal;
                                default: break; // Fallback to string comparison if operator not supported for INT
                            }
                        }
                    }
                    long double dVal;
                    if (tryParseStrictLongDouble(condValue, dVal)) {
                        long double fVal;
                        if (tryParseStrictLongDouble(fieldValue, fVal)) {
                            switch (cond.op) {
                                case FilterOperator::EQUALS: return areAlmostEqual(fVal, dVal);
                                case FilterOperator::NOT_EQUALS: return !areAlmostEqual(fVal, dVal);
                                case FilterOperator::GREATER_THAN: return fVal > dVal && !areAlmostEqual(fVal, dVal);
                                case FilterOperator::LESS_THAN: return fVal < dVal && !areAlmostEqual(fVal, dVal);
                                case FilterOperator::GREATER_THAN_OR_EQUAL: return fVal >= dVal || areAlmostEqual(fVal, dVal);
                                case FilterOperator::LESS_THAN_OR_EQUAL: return fVal <= dVal || areAlmostEqual(fVal, dVal);
                                default: break;
                            }
                        }
                    }
                    auto bVal = stringToBool(condValue);
                    if (bVal) {
                        auto fBVal = stringToBool(fieldValue);
                        if (fBVal) {
                            switch (cond.op) {
                                case FilterOperator::EQUALS: return *fBVal == *bVal;
                                case FilterOperator::NOT_EQUALS: return *fBVal != *bVal;
                                default: break;
                            }
                        }
                    }
                    // Fallback to STRING comparison
                    switch (cond.op) {
                        case FilterOperator::EQUALS: return cond.caseSensitive ? (fieldValue == condValue) : Utils::caseInsensitiveEquals(fieldValue, condValue);
                        case FilterOperator::EQUALS_I: return Utils::caseInsensitiveEquals(fieldValue, condValue);
                        case FilterOperator::NOT_EQUALS: return cond.caseSensitive ? (fieldValue != condValue) : !Utils::caseInsensitiveEquals(fieldValue, condValue);
                        case FilterOperator::NOT_EQUALS_I: return !Utils::caseInsensitiveEquals(fieldValue, condValue);
                        case FilterOperator::CONTAINS: return cond.caseSensitive ? (fieldValue.find(condValue) != std::string::npos) : Utils::caseInsensitiveSearch(fieldValue, condValue);
                        case FilterOperator::CONTAINS_I: return Utils::caseInsensitiveSearch(fieldValue, condValue);
                        default: return cond.caseSensitive ? (fieldValue == condValue) : Utils::caseInsensitiveEquals(fieldValue, condValue);
                    }
                }
                
                default:
                    return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Unsupported value type for string-based comparison."));
             }
        }
        else if constexpr (std::is_same_v<T, std::monostate> || std::is_same_v<T, std::vector<std::string>>) {
            // These are handled earlier (presence checks, IN operator)
             return std::unexpected(ErrorCode::Error(Code::Unexpected, "Should not be reached."));
        }
        
        return false; // Should be unreachable
    }, cond.value);
}

} // namespace filter
