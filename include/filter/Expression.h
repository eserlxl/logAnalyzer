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

    bool evaluate(const LogEntry& entry) const;

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
    bool current_negated = false;
    if (j.contains("negated") && j.at("negated").is_boolean()) {
        current_negated = j.at("negated").get<bool>();
    }

    if (j.contains("condition") && j.at("condition").is_object()) {
        FilterCondition fc;
        // Assume FilterCondition::from_json exists, returns ErrorCode::Result<void>, and handles jsonPath internally
        // If it returns Result<FilterCondition>, error handling below needs adjustment
        ErrorCode::Result<void> result = from_json(j.at("condition"), fc);
        if (!result.has_value()) {
            std::string prefix = (current_path == "/" ? "" : current_path);
            // Augment error with path if it doesn't already have one (or if it's a generic error)
            if (result.error().jsonPath.empty() || result.error().jsonPath == "/") {
                result.error().jsonPath = prefix + "/condition";
            } else {
                // If the error already has a path (e.g., from Condition::from_json), prepend the current path correctly.
                // Condition::from_json already prepends leading slash if needed.
                std::string sub_path = result.error().jsonPath;
                if (!sub_path.empty() && sub_path[0] == '/') {
                    result.error().jsonPath = prefix + "/condition" + sub_path;
                } else {
                    result.error().jsonPath = prefix + "/condition/" + sub_path;
                }
            }
            return std::unexpected(result.error());
        }
        fe = FilterExpression(std::move(fc), current_negated);
    } else if (j.contains("operator") && j.at("operator").is_string()) {
        auto opStr = j.at("operator").get<std::string>();
        auto opOpt = fromStringToFilterLogicalOperator(opStr); // Assumes this function exists and works
        if (!opOpt) {
            std::string prefix = (current_path == "/" ? "" : current_path);
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Unknown filter logical operator: " + opStr, prefix + "/operator"));
        }
        
        FilterLogicalOperator op = *opOpt;

        std::vector<FilterExpression> operands;
        if (j.contains("operands") && j.at("operands").is_array()) {
            const auto& operands_array = j.at("operands");
            for (size_t i = 0; i < operands_array.size(); ++i) {
                FilterExpression operand_fe;
                // Recursive call with updated path for each operand
                std::string next_path = (current_path == "/" ? "" : current_path) + "/operands[" + std::to_string(i) + "]";
                ErrorCode::Result<void> result = from_json_recursive(operands_array[i], operand_fe, next_path);
                if (!result.has_value()) {
                    // Error already contains the correct path from recursive call
                    return std::unexpected(result.error());
                }
                operands.push_back(std::move(operand_fe));
            }
        } else {
             std::string next_path = (current_path == "/" ? "" : current_path) + "/operands";
             return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterExpression with operator " + opStr + " must contain 'operands' array.", next_path));
        }
        fe = FilterExpression(op, std::move(operands), current_negated);
    } else {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterExpression must contain either 'condition' or 'operator' with 'operands'.", current_path));
    }
    
    return {}; // Success
}

// Main from_json entry point for FilterExpression
inline ErrorCode::Result<void> from_json(const nlohmann::json& j, FilterExpression& fe) {
    // Start recursive parsing with root path "/"
    return from_json_recursive(j, fe, "/");
}


#endif // FILTER_EXPRESSION_H
