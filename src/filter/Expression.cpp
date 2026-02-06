// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "filter/Expression.h"
#include "filter/EnumStringConversions.h"
#include "utils/Core.h"
#include "utils/Time.h"
#include "utils/String.h"
#include "utils/Version.h"
#include "utils/IpAddress.h"
#include <regex>
#include <chrono>
#include <limits>
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <numeric>
#include <set>

namespace { // Unnamed namespace for internal helper functions

// Helper to convert string to bool
std::optional<bool> stringToBool(const std::string& s) {
    std::string lowerS = Utils::toLower(s);
    if (lowerS == "true" || lowerS == "1" || lowerS == "t" || lowerS == "yes") return true;
    if (lowerS == "false" || lowerS == "0" || lowerS == "f" || lowerS == "no") return false;
    return std::nullopt;
}

std::string join(const std::vector<std::string>& elements, const std::string& delimiter) {
    if (elements.empty()) {
        return "";
    }
    return std::accumulate(std::next(elements.begin()), elements.end(), elements[0],
        [&delimiter](const std::string& a, const std::string& b) {
            return a + delimiter + b;
        });
}

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
        return std::unexpected(ErrorCode::Error(Code::FieldNotFound, "Field not found in log entry."));
    }
    const std::string& fieldValue = *fieldValueOpt;

    std::vector<std::string> valueSet;
    if (!std::holds_alternative<std::string>(cond.value) && (cond.op != FilterOperator::IN && cond.op != FilterOperator::NOT_IN)) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Value for this operator must be a single string."));
    }

    auto effectiveValueType = cond.valueType;
    if (std::holds_alternative<std::string>(cond.value)) {
        const std::string& condValue = std::get<std::string>(cond.value);
        if (effectiveValueType == FilterValueType::AUTO) {
            // Basic type inference
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
        }
    }

    if (cond.op == FilterOperator::IN || cond.op == FilterOperator::NOT_IN) {
        std::vector<std::string> valueSet;
        if (std::holds_alternative<std::vector<std::string>>(cond.value)) {
            valueSet = std::get<std::vector<std::string>>(cond.value);
        } else {
            // Attempt to parse string as JSON array for backward compatibility
            const std::string& jsonStr = std::get<std::string>(cond.value);
            try {
                auto j = nlohmann::json::parse(jsonStr);
                if (j.is_array()) {
                    for (const auto& item : j) {
                        if (effectiveValueType == FilterValueType::STRING || effectiveValueType == FilterValueType::AUTO) {
                            if (item.is_string()) valueSet.push_back(item.get<std::string>());
                        } 
                        
                        if (effectiveValueType == FilterValueType::INT || effectiveValueType == FilterValueType::AUTO) {
                            if (item.is_number_integer()) valueSet.push_back(std::to_string(item.get<int64_t>()));
                        }

                        if (effectiveValueType == FilterValueType::DOUBLE || effectiveValueType == FilterValueType::FLOAT || effectiveValueType == FilterValueType::AUTO) {
                            if (item.is_number() && !item.is_number_integer()) {
                                std::ostringstream ss;
                                ss << item.get<double>();
                                valueSet.push_back(ss.str());
                            }
                        }

                        if (effectiveValueType == FilterValueType::BOOL || effectiveValueType == FilterValueType::AUTO) {
                            if (item.is_boolean()) valueSet.push_back(item.get<bool>() ? "true" : "false");
                        }
                    }
                } else {
                    return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Operator IN/NOT_IN requires a JSON array."));
                }
            } catch (...) {
                return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Operator IN/NOT_IN requires a valid JSON array string."));
            }
        }
        
        bool found = false;
        for (const auto& val : valueSet) {
            if (cond.caseSensitive ? (fieldValue == val) : Utils::caseInsensitiveEquals(fieldValue, val)) {
                found = true;
                break;
            }
        }
        return (cond.op == FilterOperator::IN) ? found : !found;
    }

    const std::string& condValue = std::get<std::string>(cond.value);

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
                    try {
                        auto flags = cond.caseSensitive ? std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase;
                        std::regex re(condValue, flags);
                        return std::regex_search(fieldValue, re);
                    } catch (const std::regex_error& e) {
                        return std::unexpected(ErrorCode::Error(Code::InvalidRegex, "Invalid regex pattern '" + condValue + "': " + e.what()));
                    }
                }
                default: return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Invalid operator for STRING type."));
            }
            break;
        }
        case FilterValueType::INT: {
            long long fieldNum, condNum;
            try {
                fieldNum = std::stoll(fieldValue);
                condNum = std::stoll(condValue);
            } catch (...) {
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
            long double fieldNum, condNum;
            try {
                fieldNum = std::stold(fieldValue);
                condNum = std::stold(condValue);
            } catch (...) {
                return std::unexpected(ErrorCode::Error(Code::ConversionError, "Floating-point conversion failed."));
            }
            
            auto areAlmostEqual = [](long double a, long double b) {
                constexpr long double epsilon = 1e-9L;
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
            std::expected<std::chrono::system_clock::time_point, ErrorCode::Error> fieldTime, condTime;
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

} // Unnamed namespace

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
                    if (!subResult) return subResult;
                    if (!*subResult) { result = false; break; }
                }
            } else { // OR
                result = false;
                for (const auto& expr : expressions_) {
                    auto subResult = expr.evaluate(entry);
                    if (!subResult) return subResult;
                    if (*subResult) { result = true; break; }
                }
            }
            break;
    }

    if (negated_) {
        if (!result) return result;
        return !*result;
    }
    return result;
}

ErrorCode::Result<void> FilterExpression::validate() const {
    switch (type_) {
        case ExpressionType::EMPTY: return {};
        case ExpressionType::CONDITION:
            if (condition_->op == FilterOperator::IN || condition_->op == FilterOperator::NOT_IN) {
                if (!std::holds_alternative<std::vector<std::string>>(condition_->value)) {
                    return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "IN/NOT_IN operator requires an array value."));
                }
            } else if (condition_->op != FilterOperator::IS_NULL && condition_->op != FilterOperator::IS_NOT_NULL &&
                       condition_->op != FilterOperator::IS_PRESENT && condition_->op != FilterOperator::IS_ABSENT) {
                if (!std::holds_alternative<std::string>(condition_->value)) {
                    return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "This operator requires a single string value."));
                }
            }

            if (condition_->op == FilterOperator::REGEX) {
                if(auto* condValue = std::get_if<std::string>(&condition_->value)) {
                    try {
                        auto flags = condition_->caseSensitive ? std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase;
                        std::regex re(*condValue, flags);
                    } catch (const std::regex_error& e) {
                        return std::unexpected(ErrorCode::Error(Code::InvalidRegex, "Invalid regex: " + (*condValue) + " (" + e.what() + ")"));
                    }
                }
            }
            return {};
        case ExpressionType::LOGICAL:
            for (const auto& expr : expressions_) {
                auto result = expr.validate();
                if (!result) return result;
            }
            return {};
    }
    return {};
}

FilterExpression FilterExpression::simplify() const {
    if (isLogical()) {
        std::vector<FilterExpression> new_expressions;
        for (const auto& child : expressions_) {
            new_expressions.push_back(child.simplify());
        }
        // Basic simplification: flatten nested AND/OR of the same type
        std::vector<FilterExpression> flattened_expressions;
        for (auto&& expr : new_expressions) {
            if (expr.isLogical() && expr.getLogicalOperator() == logicalOperator_ && !expr.isNegated()) {
                for (auto&& sub_expr : expr.expressions_) {
                    flattened_expressions.push_back(std::move(sub_expr));
                }
            } else {
                flattened_expressions.push_back(std::move(expr));
            }
        }
        return FilterExpression(*logicalOperator_, std::move(flattened_expressions), negated_);
    }
    return *this;
}

void FilterExpression::visit(std::function<void(const FilterCondition&)> visitor) const {
    if (isCondition() && condition_) {
        visitor(*condition_);
    } else if (isLogical()) {
        for (const auto& child : expressions_) {
            child.visit(visitor);
        }
    }
}

std::string FilterExpression::toString() const {
    std::string core_str;
    switch (type_) {
        case ExpressionType::EMPTY:
            core_str = "EMPTY";
            break;
        case ExpressionType::CONDITION:
            core_str = conditionToString(*condition_);
            break;
        case ExpressionType::LOGICAL: {
            if (expressions_.empty()) {
                core_str = "EMPTY";
            } else {
                std::string op_str = " " + ::toString(*logicalOperator_) + " ";
                std::vector<std::string> parts;
                for (const auto& expr : expressions_) {
                    parts.push_back(expr.toString());
                }
                core_str = "(" + join(parts, op_str) + ")";
            }
            break;
        }
        default:
            core_str = "INVALID";
    }

    if (negated_) {
        return "NOT (" + core_str + ")";
    }
    return core_str;
}

std::string FilterExpression::conditionToString(const FilterCondition& cond) const {
    std::string fieldStr = cond.customField ? *cond.customField : Utils::logEntryFieldToString(cond.field);
    std::string opStr = ::toString(cond.op);
    
    if (cond.op == FilterOperator::IS_PRESENT || cond.op == FilterOperator::IS_ABSENT || cond.op == FilterOperator::IS_NULL || cond.op == FilterOperator::IS_NOT_NULL) {
        return fieldStr + " " + opStr;
    }
    
    std::string valStr;
    std::visit([&valStr](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::string>) {
            valStr = "\"" + arg + "\"";
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            valStr = "[";
            for (size_t i = 0; i < arg.size(); ++i) {
                valStr += "\"" + arg[i] + "\"";
                if (i < arg.size() - 1) valStr += ", ";
            }
            valStr += "]";
        }
    }, cond.value);
    
    return fieldStr + " " + opStr + " " + valStr;
}

// --- Other FilterExpression methods ---
FilterExpression FilterExpression::clone() const { return *this; }
FilterExpression FilterExpression::makeEmpty(bool negated) { FilterExpression e; e.negated_ = negated; return e; }
FilterExpression FilterExpression::makeCondition(FilterCondition condition, bool negated) { return FilterExpression(std::move(condition), negated); }
FilterExpression FilterExpression::makeAnd(std::vector<FilterExpression> expressions, bool negated) { 
    return FilterExpression(FilterLogicalOperator::AND, std::move(expressions), negated).simplify(); 
}
FilterExpression FilterExpression::makeOr(std::vector<FilterExpression> expressions, bool negated) { 
    return FilterExpression(FilterLogicalOperator::OR, std::move(expressions), negated).simplify(); 
}
FilterExpression FilterExpression::makeNot(FilterExpression expr) { expr.negated_ = !expr.negated_; return expr; }
