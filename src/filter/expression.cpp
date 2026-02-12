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
        } else if constexpr (std::is_same_v<T, int64_t>) {
            valStr = std::to_string(arg);
        } else if constexpr (std::is_same_v<T, double>) {
            valStr = std::to_string(arg);
        } else if constexpr (std::is_same_v<T, bool>) {
            valStr = arg ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            valStr = "[";
            for (size_t i = 0; i < arg.size(); ++i) {
                valStr += "\"" + arg[i] + "\"";
                if (i < arg.size() - 1) valStr += ", ";
            }
            valStr += "]";
        } else if constexpr (std::is_same_v<T, std::monostate>) {
            valStr = "NULL"; // Or some other representation for monostate
        }
    }, cond.value);
    
    return fieldStr + " " + opStr + " " + valStr;
}

// --- Other FilterExpression methods ---
FilterExpression FilterExpression::makeEmpty(bool negated) {
    FilterExpression e;
    e.negated_ = negated;
    return e;
}
FilterExpression FilterExpression::makeCondition(FilterCondition condition, bool negated) {
    return FilterExpression(std::move(condition), negated);
}
FilterExpression FilterExpression::makeAnd(std::vector<FilterExpression> expressions, bool negated) {
    return FilterExpression(FilterLogicalOperator::AND, std::move(expressions), negated).simplify();
}
FilterExpression FilterExpression::makeOr(std::vector<FilterExpression> expressions, bool negated) {
    return FilterExpression(FilterLogicalOperator::OR, std::move(expressions), negated).simplify();
}
FilterExpression FilterExpression::makeNot(FilterExpression expr) {
    expr.negated_ = !expr.negated_;
    return expr;
}

} // namespace filter
