#ifndef FILTER_EXPRESSION_H
#define FILTER_EXPRESSION_H

#include <vector>
#include <optional>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include "core/Error.h"
#include "core/LogTypes.h" // For LogEntry
#include "filter/FilterCondition.h" // For FilterCondition
#include "filter/FilterTypes.h"     // For FilterLogicalOperator

// New: Represents a composite filter expression (tree-like structure)
class FilterExpression {
public:
    // Default constructor (represents an empty/no-op filter)
    FilterExpression() : type_(ExpressionType::EMPTY) {}

    // Constructor for a single condition (leaf node)
    FilterExpression(FilterCondition condition) : type_(ExpressionType::CONDITION), condition_(std::move(condition)) {}

    // Constructor for logical operations
    FilterExpression(FilterLogicalOperator op, std::vector<FilterExpression> expressions = {})
        : type_(ExpressionType::LOGICAL), logicalOperator_(op), expressions_(std::move(expressions)) {}

    // Fluent builders for complex expressions
    static FilterExpression create(FilterCondition condition) {
        return FilterExpression(std::move(condition));
    }

    FilterExpression And(FilterExpression other) const {
        if (type_ == ExpressionType::LOGICAL && logicalOperator_ == FilterLogicalOperator::AND) {
            FilterExpression newExpr = *this;
            newExpr.expressions_.push_back(std::move(other));
            return newExpr;
        }
        return FilterExpression(FilterLogicalOperator::AND, {*this, std::move(other)});
    }

    FilterExpression Or(FilterExpression other) const {
        if (type_ == ExpressionType::LOGICAL && logicalOperator_ == FilterLogicalOperator::OR) {
            FilterExpression newExpr = *this;
            newExpr.expressions_.push_back(std::move(other));
            return newExpr;
        }
        return FilterExpression(FilterLogicalOperator::OR, {*this, std::move(other)});
    }

    // Applies 'NOT' to 'this' expression
    FilterExpression Not() const {
        if (type_ == ExpressionType::LOGICAL &&
            logicalOperator_ == FilterLogicalOperator::NOT &&
            !expressions_.empty()) {
            return expressions_[0]; // Simplify NOT(NOT(expr)) -> expr
        }
        return FilterExpression(FilterLogicalOperator::NOT, {*this});
    }

    // Forward declarations for recursive JSON conversion
    friend void to_json(nlohmann::json& j, const FilterExpression& fe);
    friend inline ErrorCode::Result<void> from_json(const nlohmann::json& j, FilterExpression& fe);

    // Accessors for internal components (useful for evaluation)
    enum class ExpressionType { EMPTY, CONDITION, LOGICAL };
    ExpressionType getType() const { return type_; }
    const std::optional<FilterCondition>& getCondition() const { return condition_; }
    const std::optional<FilterLogicalOperator>& getLogicalOperator() const { return logicalOperator_; }
    const std::vector<FilterExpression>& getExpressions() const { return expressions_; }

    bool isCondition() const { return type_ == ExpressionType::CONDITION; }
    bool isLogical() const { return type_ == ExpressionType::LOGICAL; }

    bool evaluate(const LogEntry& entry) const;

private:
    ExpressionType type_;
    std::optional<FilterCondition> condition_;
    std::optional<FilterLogicalOperator> logicalOperator_;
    std::vector<FilterExpression> expressions_;
};



// JSON conversion for FilterExpression (recursive)
inline void to_json(nlohmann::json& j, const FilterExpression& fe) {
    if (fe.getType() == FilterExpression::ExpressionType::CONDITION) {
        j["condition"] = *fe.getCondition();
    } else if (fe.getType() == FilterExpression::ExpressionType::LOGICAL) {
        j["operator"] = Utils::filterLogicalOperatorToString(*fe.getLogicalOperator());
        if (!fe.getExpressions().empty()) {
            j["operands"] = nlohmann::json::array();
            for (const auto& operand : fe.getExpressions()) {
                nlohmann::json operand_j;
                to_json(operand_j, operand); // Recursive call
                j["operands"].push_back(operand_j);
            }
        }
    }
}

inline ErrorCode::Result<void> from_json(const nlohmann::json& j, FilterExpression& fe) {
    if (j.contains("condition") && j.at("condition").is_object()) {
        FilterCondition fc;
        ErrorCode::Result<void> result = from_json(j.at("condition"), fc);
        if (!result.has_value()) {
            return std::unexpected(result.error());
        }
        fe = FilterExpression(std::move(fc));
    } else if (j.contains("operator") && j.at("operator").is_string()) {
        auto opOpt = Utils::stringToFilterLogicalOperator(j.at("operator").get<std::string>());
        if (!opOpt) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Unknown filter logical operator: " + j.at("operator").get<std::string>()));
        }
        FilterLogicalOperator op = *opOpt;

        std::vector<FilterExpression> operands;
        if (j.contains("operands") && j.at("operands").is_array()) {
            for (const auto& operand_j : j.at("operands")) {
                FilterExpression operand_fe;
                ErrorCode::Result<void> result = from_json(operand_j, operand_fe); // Recursive call
                if (!result.has_value()) {
                    return std::unexpected(result.error());
                }
                operands.push_back(std::move(operand_fe));
            }
        } else if (op != FilterLogicalOperator::NOT) {
             return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterExpression with operator " + Utils::filterLogicalOperatorToString(op) + " must contain 'operands' array."));
        } else if (!j.contains("operands")) {
             // For NOT, we might be lenient or strict. The test seems to test general logical operator requirements.
             // But FilterExpressionFromJsonMissingOperands uses AND.
             // If AND, we fail.
             // If NOT, we allowed it before?
             // Let's failing generally if operands is missing, as logical operators imply operands.
             return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterExpression with operator must contain 'operands' array."));
        }
        fe = FilterExpression(op, std::move(operands));
    } else {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterExpression must contain either 'condition' or 'operator' with 'operands'."));
    }
    
    return {}; // Success
}

#endif // FILTER_EXPRESSION_H
