// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "filter/expression.h"
#include "filter/condition_evaluation.h"
#include "filter/enum_string_conversions.h"
#include "utils/core.h"
#include "utils/time.h"
#include "utils/string.h"
#include "utils/version.h"
#include "utils/ip_address.h"
#include <regex>
#include <chrono>
#include <limits>
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <numeric>
#include <set>

namespace filter {

namespace { // Unnamed namespace for internal helper functions

std::string join(const std::vector<std::string>& elements, const std::string& delimiter) {
    if (elements.empty()) {
        return "";
    }
    size_t totalSize = 0;
    for (const auto& s : elements) totalSize += s.size();
    totalSize += delimiter.size() * (elements.size() - 1);
    
    std::string result;
    result.reserve(totalSize);
    result += elements[0];
    for (size_t i = 1; i < elements.size(); ++i) {
        result += delimiter;
        result += elements[i];
    }
    return result;
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
        case ExpressionType::CONDITION: {
            if (condition_->op == FilterOperator::REGEX) {
                if (auto* condValue = std::get_if<std::string>(&condition_->value)) {
                    try {
                        auto flags = condition_->caseSensitive ? std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase;
                        condition_->compiledRegex = std::regex(*condValue, flags); // Cache it
                    } catch (const std::regex_error& e) {
                        return std::unexpected(ErrorCode::Error(Code::InvalidRegex, "Invalid regex: " + (*condValue) + " (" + e.what() + ")"));
                    }
                }
            } else if (condition_->op == FilterOperator::IN || condition_->op == FilterOperator::NOT_IN) {
                if (std::holds_alternative<std::string>(condition_->value)) {
                     const auto& jsonStr = std::get<std::string>(condition_->value);
                     try {
                        auto j = nlohmann::json::parse(jsonStr);
                        if(j.is_array()) {
                            std::vector<std::string> parsedSet;
                            for (const auto& item : j) {
                                if (item.is_string()) parsedSet.push_back(item.get<std::string>());
                                else if (item.is_number_integer()) parsedSet.push_back(std::to_string(item.get<long long>()));
                                else if (item.is_number()) parsedSet.push_back(std::to_string(item.get<double>()));
                                else if (item.is_boolean()) parsedSet.emplace_back(item.get<bool>() ? "true" : "false");
                            }
                            condition_->parsedValue = std::move(parsedSet); // Cache it
                        } else {
                            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "IN/NOT_IN operator value must be a JSON array string."));
                        }
                     } catch(...) {
                        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Failed to parse JSON array for IN/NOT_IN operator."));
                     }
                } else if (!std::holds_alternative<std::vector<std::string>>(condition_->value)) {
                    return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "IN/NOT_IN operator requires an array value."));
                }
            }
            
            if (condition_->valueType == FilterValueType::AUTO && std::holds_alternative<std::string>(condition_->value)) {
                const auto& condValue = std::get<std::string>(condition_->value);
                if (condValue == "true" || condValue == "false") {
                    condition_->inferredValueType = FilterValueType::BOOL;
                } else if (Utils::isNumeric(condValue)) {
                    condition_->inferredValueType = condValue.find('.') != std::string::npos ? FilterValueType::DOUBLE : FilterValueType::INT;
                } else if (Utils::parseIpAddress(condValue)) {
                    condition_->inferredValueType = FilterValueType::IP_ADDRESS;
                } else if (Utils::parseSemanticVersion(condValue)) {
                    condition_->inferredValueType = FilterValueType::VERSION;
                } else {
                    condition_->inferredValueType = FilterValueType::STRING;
                }
            }
            return {};
        }
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
        std::vector<FilterExpression> newExpressions;
        newExpressions.reserve(expressions_.size());
for (const auto& child : expressions_) {
            newExpressions.push_back(child.simplify());
        }
        // Basic simplification: flatten nested AND/OR of the same type
        std::vector<FilterExpression> flattenedExpressions;
        for (auto&& expr : newExpressions) {
            if (expr.isLogical() && expr.getLogicalOperator() == logicalOperator_ && !expr.isNegated()) {
                for (auto&& subExpr : expr.expressions_) {
                    flattenedExpressions.push_back(std::move(subExpr));
                }
            } else {
                flattenedExpressions.push_back(std::move(expr));
            }
        }
        return {*logicalOperator_, std::move(flattenedExpressions), negated_};
    }
    return *this;
}

void FilterExpression::visit(const std::function<void(const FilterCondition&)>& visitor) const {
    if (isCondition() && condition_) {
        visitor(*condition_);
    } else if (isLogical()) {
        for (const auto& child : expressions_) {
            child.visit(visitor);
        }
    }
}

std::string FilterExpression::toString() const {
    std::string coreStr;
    switch (type_) {
        case ExpressionType::EMPTY:
            coreStr = "EMPTY";
            break;
        case ExpressionType::CONDITION:
            coreStr = conditionToString(*condition_);
            break;
        case ExpressionType::LOGICAL: {
            if (expressions_.empty()) {
                coreStr = "EMPTY";
            } else {
                std::string opStr = " " + filter::toString(*logicalOperator_) + " ";
                std::vector<std::string> parts;
                parts.reserve(expressions_.size());
for (const auto& expr : expressions_) {
                    parts.push_back(expr.toString());
                }
                coreStr = "(" + join(parts, opStr) + ")";
            }
            break;
        }
        default:
            coreStr = "INVALID";
    }

    if (negated_) {
        return "NOT (" + coreStr + ")";
    }
    return coreStr;
}

std::string FilterExpression::conditionToString(const FilterCondition& cond) {
    std::string fieldStr = cond.customField ? *cond.customField : Utils::logEntryFieldToString(cond.field);
    std::string opStr = filter::toString(cond.op);
    
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
FilterExpression FilterExpression::makeCondition(FilterCondition condition, bool negated) { return {std::move(condition), negated}; }
FilterExpression FilterExpression::makeAnd(std::vector<FilterExpression> expressions, bool negated) {
    return FilterExpression(FilterLogicalOperator::AND, std::move(expressions), negated).simplify();
}
FilterExpression FilterExpression::makeOr(std::vector<FilterExpression> expressions, bool negated) {
    return FilterExpression(FilterLogicalOperator::OR, std::move(expressions), negated).simplify();
}
FilterExpression FilterExpression::makeNot(FilterExpression expr) { expr.negated_ = !expr.negated_; return expr; }

} // namespace filter
