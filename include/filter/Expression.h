#ifndef FILTER_EXPRESSION_H
#define FILTER_EXPRESSION_H

#include <vector>
#include <optional>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include "core/Error.h"
#include "core/LogTypes.h" // For LogEntry
#include "filter/Condition.h" // For FilterCondition
#include "filter/Types.h"     // For FilterLogicalOperator
#include "filter/EnumStringConversions.h" // New: For enum to string conversions
#include "filter/JsonUtils.h"

// New: Represents a composite filter expression (tree-like structure)
class FilterExpression {
public:
    // Default constructor (represents an empty/no-op filter)
    FilterExpression() : type_(ExpressionType::EMPTY), negated_(false) {} // Initialize negated_

    // Constructor for a single condition (leaf node)
    FilterExpression(FilterCondition condition, bool negated = false) // Add negated parameter
        : type_(ExpressionType::CONDITION), condition_(std::move(condition)), negated_(negated) {}

    // Constructor for logical operations
    FilterExpression(FilterLogicalOperator op, std::vector<FilterExpression> expressions = {}, bool negated = false) // Add negated parameter
        : type_(ExpressionType::LOGICAL), logicalOperator_(op), expressions_(std::move(expressions)), negated_(negated) {}

    // Fluent builders for complex expressions
    static FilterExpression create(const FilterCondition& cond) {
        FilterExpression expr;
        expr.type_ = ExpressionType::CONDITION;
        expr.condition_ = cond;
        std::cerr << "DEBUG: FilterExpression::create - negated_: " << expr.negated_ << std::endl;
        return expr;
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
        FilterExpression newExpr = *this; // Create a copy
        newExpr.negated_ = !newExpr.negated_; // Toggle negation
        return newExpr;
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
    bool isNegated() const { return negated_; } // New accessor for negation

    bool isCondition() const { return type_ == ExpressionType::CONDITION; }
    bool isLogical() const { return type_ == ExpressionType::LOGICAL; }

    ErrorCode::Result<bool> evaluate(const LogEntry& entry) const;
    ErrorCode::Result<void> validate() const;

private:
    ExpressionType type_;
    std::optional<FilterCondition> condition_;
    std::optional<FilterLogicalOperator> logicalOperator_; // This will now only hold AND/OR
    std::vector<FilterExpression> expressions_;
    bool negated_ = false; // New member to handle negation
};



// JSON conversion for FilterExpression (recursive)
inline void to_json(nlohmann::json& j, const FilterExpression& fe) {
    if (fe.getType() == FilterExpression::ExpressionType::CONDITION) {
        j["condition"] = *fe.getCondition();
    } else if (fe.getType() == FilterExpression::ExpressionType::LOGICAL) {
        j["operator"] = toString(*fe.getLogicalOperator());
        if (!fe.getExpressions().empty()) {
            j["operands"] = nlohmann::json::array();
            for (const auto& operand : fe.getExpressions()) {
                nlohmann::json operand_j;
                to_json(operand_j, operand); // Recursive call
                j["operands"].push_back(operand_j);
            }
        }
    }
    if (fe.isNegated()) {
        j["negated"] = true; // Add negated flag to JSON
    }
}

// Helper function for recursive from_json calls to manage JSON path
inline ErrorCode::Result<void> from_json_recursive(const nlohmann::json& j, FilterExpression& fe, const std::string& current_path) {
    using namespace FilterJsonUtils;

    bool current_negated = getOptional<bool>(j, "negated").value_or(false);

    if (j.contains("condition") && j.at("condition").is_object()) {
        FilterCondition fc;
        std::string next_path = (current_path == "/" ? "" : current_path) + "/condition";
        auto result = from_json(j.at("condition"), fc, next_path);
        if (!result) return std::unexpected(result.error());
        
        fe = FilterExpression(std::move(fc), current_negated);
    } else if (j.contains("operator") && j.at("operator").is_string()) {
        auto opStr = j.at("operator").get<std::string>();
        auto opOpt = fromStringToFilterLogicalOperator(opStr);
        if (!opOpt) {
            return std::unexpected(makeError(Code::InvalidArgument, "Unknown logical operator: " + opStr, current_path, "operator"));
        }
        
        FilterLogicalOperator op = *opOpt;

        if (!j.contains("operands") || !j.at("operands").is_array()) {
             return std::unexpected(makeError(Code::InvalidArgument, "Operator " + opStr + " requires 'operands' array.", current_path, "operands"));
        }

        const auto& operands_array = j.at("operands");
        std::vector<FilterExpression> operands;
        for (size_t i = 0; i < operands_array.size(); ++i) {
            FilterExpression operand_fe;
            std::string next_path = (current_path == "/" ? "" : current_path) + "/operands[" + std::to_string(i) + "]";
            auto result = from_json_recursive(operands_array[i], operand_fe, next_path);
            if (!result) return std::unexpected(result.error());
            operands.push_back(std::move(operand_fe));
        }
        fe = FilterExpression(op, std::move(operands), current_negated);
    } else {
        return std::unexpected(makeError(Code::InvalidArgument, "Expression must contain 'condition' or 'operator' with 'operands'.", current_path, ""));
    }
    
    return {}; // Success
}

// Main from_json entry point for FilterExpression
inline ErrorCode::Result<void> from_json(const nlohmann::json& j, FilterExpression& fe) {
    // Start recursive parsing with root path "/"
    return from_json_recursive(j, fe, "/");
}


#endif // FILTER_EXPRESSION_H
