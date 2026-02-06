// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "filter/Expression.h"
#include "filter/EnumStringConversions.h" // For new enum to string conversions
#include "utils/Core.h"
#include "utils/Time.h" // For datetime parsing
#include "utils/String.h" // For string utility functions
#include "utils/Version.h" // For SemanticVersion parsing and comparison
#include "utils/IpAddress.h" // For IpAddress parsing and comparison
#include <regex>
#include <chrono>   // For std::chrono::system_point
#include <limits>   // For std::numeric_limits
#include <cmath>    // For std::abs with doubles
#include <stdexcept> // For std::stod, std::stoll exceptions
#include <iostream> // For temporary logging to cerr

namespace { // Unnamed namespace for internal helper functions


// Helper to convert string to bool
std::optional<bool> stringToBool(const std::string& s) {
    std::string lowerS = Utils::toLower(s);
    if (lowerS == "true" || lowerS == "1" || lowerS == "t" || lowerS == "yes") return true;
    if (lowerS == "false" || lowerS == "0" || lowerS == "f" || lowerS == "no") return false;
    return std::nullopt;
}

// Struct to hold a parsed value and its inferred type
struct ParsedValue {
    FilterValueType type;
    std::variant<std::string, long long, double, bool,
                 std::chrono::system_clock::time_point,
                 Utils::SemanticVersion, Utils::IpAddress> value;

    // Helper to get the type precedence for auto-detection
    int getTypePrecedence() const {
        switch (type) {
            case FilterValueType::IP_ADDRESS: return 7;
            case FilterValueType::VERSION: return 6;
            case FilterValueType::DATETIME: return 5;
            case FilterValueType::DOUBLE: return 4;
            case FilterValueType::INT: return 3;
            case FilterValueType::BOOL: return 2;
            case FilterValueType::STRING: return 1;
            default: return 0; // UNKNOWN and AUTO (shouldn't happen here)
        }
    }
};

// Attempts to infer the type of a string and parse it accordingly
std::optional<ParsedValue> inferTypeAndParse(const std::string& s) {
    // Try IP_ADDRESS
    if (auto ip = Utils::parseIpAddress(s)) {
        return ParsedValue{FilterValueType::IP_ADDRESS, *ip};
    }
    // Try VERSION
    if (auto ver = Utils::parseSemanticVersion(s)) {
        return ParsedValue{FilterValueType::VERSION, *ver};
    }
    // Try DATETIME
    if (auto time = Utils::parseTime(s)) {
        return ParsedValue{FilterValueType::DATETIME, *time};
    }
    // Try BOOL
    if (auto b = stringToBool(s)) {
        return ParsedValue{FilterValueType::BOOL, *b};
    }
    // Try INT
    try {
        size_t pos;
        long long ll = std::stoll(s, &pos);
        if (pos == s.length()) { // Successfully parsed entire string as INT
            return ParsedValue{FilterValueType::INT, ll};
        }
    } catch (...) {}
    // Try DOUBLE
    try {
        size_t pos;
        double d = std::stod(s, &pos);
        if (pos == s.length()) { // Successfully parsed entire string as DOUBLE
            return ParsedValue{FilterValueType::DOUBLE, d};
        }
    } catch (...) {}

    // Fallback to STRING if no other type matches
    return ParsedValue{FilterValueType::STRING, s};
}

} // Unnamed namespace


// Helper to evaluate IN and NOT_IN operators
ErrorCode::Result<bool> evaluateInNotIn(const std::string& fieldValue, const std::string& condValue, FilterValueType type, bool caseSensitive, FilterOperator op) {
    try {
        auto jsonArray = nlohmann::json::parse(condValue);
        if (!jsonArray.is_array()) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Operator IN/NOT_IN requires a JSON array."));
        }

        bool found = false;

        switch (type) {
            case FilterValueType::INT: {
                long long fieldVal;
                auto [ptr, ec] = std::from_chars(fieldValue.data(), fieldValue.data() + fieldValue.size(), fieldVal);
                if (ec != std::errc()) return std::unexpected(ErrorCode::Error(Code::ConversionError, "Invalid INT field value '" + fieldValue + "'"));

                for (const auto& item : jsonArray) {
                    if (item.is_number_integer()) {
                        if (item.get<long long>() == fieldVal) { found = true; break; }
                    } else if (item.is_string()) {
                         long long itemVal;
                         auto s = item.get<std::string>();
                         auto [p, e] = std::from_chars(s.data(), s.data() + s.size(), itemVal);
                         if (e == std::errc() && itemVal == fieldVal) { found = true; break; }
                    }
                }
                break;
            }
            case FilterValueType::DOUBLE: {
                double fieldVal;
                auto [ptr, ec] = std::from_chars(fieldValue.data(), fieldValue.data() + fieldValue.size(), fieldVal);
                if (ec != std::errc()) return std::unexpected(ErrorCode::Error(Code::ConversionError, "Invalid DOUBLE field value '" + fieldValue + "'"));
                
                const double epsilon = 1e-9;
                for (const auto& item : jsonArray) {
                    double itemVal = 0.0;
                    bool parsed = false;
                    if (item.is_number()) {
                        itemVal = item.get<double>();
                        parsed = true;
                    } else if (item.is_string()) {
                        auto s = item.get<std::string>();
                        auto [p, e] = std::from_chars(s.data(), s.data() + s.size(), itemVal);
                        if (e == std::errc()) parsed = true;
                    }
                    
                    if (parsed && std::abs(itemVal - fieldVal) < epsilon) { found = true; break; }
                }
                break;
            }
            case FilterValueType::BOOL: {
                auto fieldVal = stringToBool(fieldValue);
                if (!fieldVal) return std::unexpected(ErrorCode::Error(Code::ConversionError, "Invalid BOOL field value '" + fieldValue + "'"));

                for (const auto& item : jsonArray) {
                    std::optional<bool> itemVal;
                    if (item.is_boolean()) {
                        itemVal = item.get<bool>();
                    } else if (item.is_string()) {
                        itemVal = stringToBool(item.get<std::string>());
                    } else if (item.is_number_integer()) {
                         // 0 or 1
                         long long i = item.get<long long>();
                         if (i == 0) itemVal = false;
                         else if (i == 1) itemVal = true;
                    }

                    if (itemVal && *itemVal == *fieldVal) { found = true; break; }
                }
                break;
            }
            case FilterValueType::DATETIME: {
                auto fieldVal = Utils::parseTime(fieldValue);
                if (!fieldVal) return std::unexpected(ErrorCode::Error(Code::ConversionError, "Invalid DATETIME field value '" + fieldValue + "'"));

                for (const auto& item : jsonArray) {
                    if (item.is_string()) {
                        if (auto itemVal = Utils::parseTime(item.get<std::string>())) {
                            if (*itemVal == *fieldVal) { found = true; break; }
                        }
                    }
                }
                break;
            }
            case FilterValueType::VERSION: {
                auto fieldVal = Utils::parseSemanticVersion(fieldValue);
                if (!fieldVal) return std::unexpected(ErrorCode::Error(Code::ConversionError, "Invalid VERSION field value '" + fieldValue + "'"));

                for (const auto& item : jsonArray) {
                    if (item.is_string()) {
                        if (auto itemVal = Utils::parseSemanticVersion(item.get<std::string>())) {
                            if (*itemVal == *fieldVal) { found = true; break; }
                        }
                    }
                }
                break;
            }
            case FilterValueType::IP_ADDRESS: {
                auto fieldVal = Utils::parseIpAddress(fieldValue);
                if (!fieldVal) return std::unexpected(ErrorCode::Error(Code::ConversionError, "Invalid IP_ADDRESS field value '" + fieldValue + "'"));

                for (const auto& item : jsonArray) {
                    if (item.is_string()) {
                        if (auto itemVal = Utils::parseIpAddress(item.get<std::string>())) {
                            if (*itemVal == *fieldVal) { found = true; break; }
                        }
                    }
                }
                break;
            }
            case FilterValueType::REGEX: {
                for (const auto& item : jsonArray) {
                    if (item.is_string()) {
                        std::string pattern = item.get<std::string>();
                        try {
                            auto flags = caseSensitive ? std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase;
                            std::regex re(pattern, flags);
                            if (std::regex_search(fieldValue, re)) {
                                found = true;
                                break;
                            }
                        } catch (const std::regex_error& e) {
                            std::cerr << "Warning: Invalid regex pattern in IN/NOT_IN list: " << pattern << " - " << e.what() << std::endl;
                        }
                    }
                }
                break;
            }
            case FilterValueType::STRING:
            case FilterValueType::UNKNOWN:
            case FilterValueType::AUTO: { // AUTO falls back to STRING
                 for (const auto& item : jsonArray) {
                    if (item.is_string()) {
                        std::string s = item.get<std::string>();
                        if (caseSensitive ? (fieldValue == s) : Utils::caseInsensitiveEquals(fieldValue, s)) {
                            found = true;
                            break;
                        }
                    }
                }
                break;
            }
        }

        return (op == FilterOperator::IN) ? found : !found;

    } catch (const nlohmann::json::parse_error& e) {
        return std::unexpected(ErrorCode::Error(Code::JsonParseError, "Failed to parse JSON for IN/NOT_IN operator: " + std::string(e.what())));
    }
}


// --- Evaluation helpers for FilterExpression ---

std::optional<std::string> getFieldValue(const LogEntry& entry, const FilterCondition& cond) {
    auto getField = [&]() -> std::optional<std::string> {
        switch (cond.field) {
            case LogEntryField::ID:
                return entry.id.has_value() ? std::optional(std::to_string(*entry.id)) : std::nullopt;
            case LogEntryField::TIMESTAMP:
                return entry.timestamp.has_value() ? std::optional(Utils::formatTimestamp(*entry.timestamp)) : std::nullopt;
            case LogEntryField::LEVEL:
                return Utils::logLevelToString(entry.level);
            case LogEntryField::SOURCE_FILE:
                return entry.sourceFile;
            case LogEntryField::LINE_NUMBER:
                return entry.sourceLineNumber.has_value() ? std::optional(std::to_string(*entry.sourceLineNumber)) : std::nullopt;
            case LogEntryField::THREAD_ID:
                return entry.threadId;
            case LogEntryField::MESSAGE:
                return entry.message;
            case LogEntryField::MODULE:
                return entry.module;
            case LogEntryField::HOST:
                return entry.host;
            case LogEntryField::CUSTOM:
                if (cond.customField) {
                    auto it = entry.customFields.find(*cond.customField);
                    if (it != entry.customFields.end()) {
                        return it->second;
                    }
                }
                return std::nullopt;
            default:
                return std::nullopt;
        }
    };
    return getField();
}

ErrorCode::Result<bool> evaluateCondition(const FilterCondition& cond, const LogEntry& entry) {
    auto fieldValueOpt = getFieldValue(entry, cond);

    if (cond.op == FilterOperator::IS_PRESENT) {
        return fieldValueOpt.has_value();
    }
    if (cond.op == FilterOperator::IS_ABSENT) {
        return !fieldValueOpt.has_value();
    }

    if (!fieldValueOpt) {
        return false; // Field not present, so any other comparison is false
    }
    const std::string& fieldValue = *fieldValueOpt;
    const std::string& condValue = cond.value;

    FilterValueType typeToUse = cond.valueType;

    if (typeToUse == FilterValueType::AUTO) {
        if (auto parsed = inferTypeAndParse(condValue)) {
            typeToUse = parsed->type;
        } else {
            typeToUse = FilterValueType::STRING;
        }
    }

    if (cond.op == FilterOperator::IN || cond.op == FilterOperator::NOT_IN) {
        return evaluateInNotIn(fieldValue, condValue, typeToUse, cond.caseSensitive, cond.op);
    }

    switch (typeToUse) {
        case FilterValueType::INT: {
            long long fieldNum;
            auto [ptr_field, ec_field] = std::from_chars(fieldValue.data(), fieldValue.data() + fieldValue.size(), fieldNum);
            if (ec_field != std::errc()) {
                return std::unexpected(ErrorCode::Error(Code::ConversionError, "Failed to convert field value '" + fieldValue + "' to INT."));
            }
            long long condNum;
            auto [ptr_cond, ec_cond] = std::from_chars(condValue.data(), condValue.data() + condValue.size(), condNum);
            if (ec_cond != std::errc()) {
                return std::unexpected(ErrorCode::Error(Code::ConversionError, "Failed to convert condition value '" + condValue + "' to INT."));
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
            double fieldNum;
            auto [ptr_field, ec_field] = std::from_chars(fieldValue.data(), fieldValue.data() + fieldValue.size(), fieldNum);
            if (ec_field != std::errc()) {
                return std::unexpected(ErrorCode::Error(Code::ConversionError, "Failed to convert field value '" + fieldValue + "' to DOUBLE."));
            }
            double condNum;
            auto [ptr_cond, ec_cond] = std::from_chars(condValue.data(), condValue.data() + condValue.size(), condNum);
            if (ec_cond != std::errc()) {
                return std::unexpected(ErrorCode::Error(Code::ConversionError, "Failed to convert condition value '" + condValue + "' to DOUBLE."));
            }
            const double epsilon = 1e-9;
            switch (cond.op) {
                case FilterOperator::EQUALS: return std::abs(fieldNum - condNum) < epsilon;
                case FilterOperator::NOT_EQUALS: return std::abs(fieldNum - condNum) >= epsilon;
                case FilterOperator::GREATER_THAN: return fieldNum > condNum;
                case FilterOperator::LESS_THAN: return fieldNum < condNum;
                case FilterOperator::GREATER_THAN_OR_EQUAL: return fieldNum >= condNum;
                case FilterOperator::LESS_THAN_OR_EQUAL: return fieldNum <= condNum;
                default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for DOUBLE type."));
            }
        }
        case FilterValueType::BOOL: {
            auto fieldBool = stringToBool(fieldValue);
            auto condBool = stringToBool(condValue);
            if (!fieldBool || !condBool) {
                 std::cerr << "DEBUG: BOOL conversion failed. fieldBool: " << fieldBool.has_value() << ", condBool: " << condBool.has_value() << std::endl;
                 return std::unexpected(ErrorCode::Error(Code::ConversionError, "Failed to convert value to BOOL. Field: '" + fieldValue + "', Condition: '" + condValue + "'"));
            }
            switch (cond.op) {
                case FilterOperator::EQUALS: return *fieldBool == *condBool;
                case FilterOperator::NOT_EQUALS: return *fieldBool != *condBool;
                default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for BOOL type."));
            }
        }
        case FilterValueType::DATETIME: {
            auto fieldTime = Utils::parseTime(fieldValue);
            auto condTime = Utils::parseTime(condValue);
            if (!fieldTime) {
                return std::unexpected(ErrorCode::Error(Code::ConversionError, "Failed to parse field value '" + fieldValue + "' as DATETIME."));
            }
            if (!condTime) {
                return std::unexpected(ErrorCode::Error(Code::ConversionError, "Failed to parse condition value '" + condValue + "' as DATETIME."));
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
        }
        case FilterValueType::VERSION: {
            auto fieldVer = Utils::parseSemanticVersion(fieldValue);
            auto condVer = Utils::parseSemanticVersion(condValue);
            if (!fieldVer) {
                return std::unexpected(ErrorCode::Error(Code::ConversionError, "Failed to parse field value '" + fieldValue + "' as VERSION."));
            }
            if (!condVer) {
                return std::unexpected(ErrorCode::Error(Code::ConversionError, "Failed to parse condition value '" + condValue + "' as VERSION."));
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
        case FilterValueType::IP_ADDRESS: {
            auto fieldIp = Utils::parseIpAddress(fieldValue);
            auto condIp = Utils::parseIpAddress(condValue);
            if (!fieldIp) {
                return std::unexpected(ErrorCode::Error(Code::ConversionError, "Failed to parse field value '" + fieldValue + "' as IP_ADDRESS."));
            }
            if (!condIp) {
                return std::unexpected(ErrorCode::Error(Code::ConversionError, "Failed to parse condition value '" + condValue + "' as IP_ADDRESS."));
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
        }
        case FilterValueType::STRING:
        case FilterValueType::UNKNOWN:
        case FilterValueType::AUTO: { // AUTO falls back to STRING
             switch (cond.op) {
                case FilterOperator::EQUALS: return cond.caseSensitive ? (fieldValue == condValue) : Utils::caseInsensitiveEquals(fieldValue, condValue);
                case FilterOperator::NOT_EQUALS: return cond.caseSensitive ? (fieldValue != condValue) : !Utils::caseInsensitiveEquals(fieldValue, condValue);
                case FilterOperator::EQUALS_I: return Utils::caseInsensitiveEquals(fieldValue, condValue);
                case FilterOperator::NOT_EQUALS_I: return !Utils::caseInsensitiveEquals(fieldValue, condValue);
                case FilterOperator::CONTAINS: return cond.caseSensitive ? (fieldValue.find(condValue) != std::string::npos) : Utils::caseInsensitiveSearch(fieldValue, condValue);
                case FilterOperator::NOT_CONTAINS: return cond.caseSensitive ? (fieldValue.find(condValue) == std::string::npos) : !Utils::caseInsensitiveSearch(fieldValue, condValue);
                case FilterOperator::CONTAINS_I: return Utils::caseInsensitiveSearch(fieldValue, condValue);
                case FilterOperator::NOT_CONTAINS_I: return !Utils::caseInsensitiveSearch(fieldValue, condValue);
                case FilterOperator::STARTS_WITH: return cond.caseSensitive ? fieldValue.starts_with(condValue) : Utils::caseInsensitiveStarts(fieldValue, condValue);
                case FilterOperator::ENDS_WITH: return cond.caseSensitive ? fieldValue.ends_with(condValue) : Utils::caseInsensitiveEnds(fieldValue, condValue);
                case FilterOperator::STARTS_WITH_I: return Utils::caseInsensitiveStarts(fieldValue, condValue);
                case FilterOperator::ENDS_WITH_I: return Utils::caseInsensitiveEnds(fieldValue, condValue);
                case FilterOperator::REGEX_MATCH: {
                    try {
                        auto flags = cond.caseSensitive ? std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase;
                        std::regex re(condValue, flags);
                        return std::regex_search(fieldValue, re);
                    } catch (const std::regex_error& e) {
                        return std::unexpected(ErrorCode::Error(Code::InvalidRegex, "Invalid regex pattern '" + condValue + "': " + e.what()));
                    }
                }
                default:
                    return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for STRING type."));
            }
        }
        default:
            return std::unexpected(ErrorCode::Error(Code::NotImplemented, "Unhandled FilterValueType."));
    }
}

// FilterExpression::evaluate and FilterExpression::validate definitions
ErrorCode::Result<bool> FilterExpression::evaluate(const LogEntry& entry) const {
    ErrorCode::Result<bool> result = false;
    switch (type_) {
        case ExpressionType::EMPTY:
            result = true;
            break;
        case ExpressionType::CONDITION:
            result = evaluateCondition(*condition_, entry);
            break;
        case ExpressionType::LOGICAL:
            if (*logicalOperator_ == FilterLogicalOperator::AND) {
                result = true;
                for (const auto& expr : expressions_) {
                    auto subResult = expr.evaluate(entry);
                    if (!subResult) return subResult; // Propagate error
                    if (!*subResult) {
                        result = false;
                        break; // Short-circuit
                    }
                }
            } else { // OR
                result = false;
                for (const auto& expr : expressions_) {
                    auto subResult = expr.evaluate(entry);
                    if (!subResult) return subResult; // Propagate error
                    if (*subResult) {
                        result = true;
                        break; // Short-circuit
                    }
                }
            }
            break;
        default:
            result = false;
            break;
    }

    if (!result) {
        std::cerr << "DEBUG: FilterExpression::evaluate - Error propagating: " << result.error().message << std::endl;
        return result; // Propagate error
    }

    if (negated_) {
        std::cerr << "DEBUG: FilterExpression::evaluate - Negating " << *result << std::endl;
        return !*result;
    }
    std::cerr << "DEBUG: FilterExpression::evaluate - Final result: " << *result << std::endl;
    return result;
}

ErrorCode::Result<void> FilterExpression::validate() const {
    switch (type_) {
        case ExpressionType::EMPTY:
            return {}; // Always valid
        case ExpressionType::CONDITION:
            if (condition_->op == FilterOperator::REGEX_MATCH) {
                try {
                    auto flags = condition_->caseSensitive ? std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase;
                    std::regex re(condition_->value, flags);
                } catch (const std::regex_error& e) {
                    return std::unexpected(ErrorCode::Error(Code::InvalidRegex, "Invalid regex pattern '" + condition_->value + "': " + e.what()));
                }
            }
            return {};
        case ExpressionType::LOGICAL:
            for (const auto& expr : expressions_) {
                auto result = expr.validate();
                if (!result) {
                    return result;
                }
            }
            return {};
    }
    return {};
}