// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

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

namespace filter {

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
    enum class ExpressionType {
        EMPTY,      ///< Represents an empty or no-op filter. An EMPTY expression (if not negated) evaluates to `true` (passes all log entries). If `negated_` is true for an EMPTY expression, it evaluates to `false` (blocks all log entries).
        CONDITION,  ///< Represents a single filter condition (leaf node).
        LOGICAL     ///< Represents a logical combination of other expressions (internal node).
    };
    ExpressionType getType() const { return type_; }
    const std::optional<FilterCondition>& getCondition() const { return condition_; }
    const std::optional<FilterLogicalOperator>& getLogicalOperator() const { return logicalOperator_; }
    const std::vector<FilterExpression>& getExpressions() const { return expressions_; }
    bool isNegated() const { return negated_; } // New accessor for negation

    bool isCondition() const { return type_ == ExpressionType::CONDITION; }
    bool isLogical() const { return type_ == ExpressionType::LOGICAL; }

    ErrorCode::Result<bool> evaluate(const LogEntry& entry) const;
    ErrorCode::Result<void> validate() const;

    /**
     * @brief Simplifies the expression tree into a canonical form.
     *
     * This method applies rules to reduce redundancy. For example:
     * - Flattens nested AND/OR groups: `(A AND (B AND C))` -> `(A AND B AND C)`
     * - Removes duplicate conditions within a group.
     * - Evaluates constant expressions (e.g., an OR group with a condition that is always true).
     *
     * @return A new, simplified FilterExpression.
     */
    FilterExpression simplify() const;

    /**
     * @brief Traverses the expression tree and applies a visitor function to each condition.
     *
     * This allows for inspecting all leaf nodes of the tree.
     *
     * @param visitor A function that will be called with a const reference to each
     *                FilterCondition in the expression.
     */
    void visit(const std::function<void(const FilterCondition&)>& visitor) const;

    /**
     * @brief Returns a human-readable string representation of the filter expression.
     *
     * The output aims to be concise and easily understandable, reflecting the structure
     * of the expression tree using parentheses for logical grouping and 'NOT' for negation.
     * This method recursively traverses the expression tree to build the string.
     *
     * Example: "((level >= WARNING AND message CONTAINS 'error') OR NOT (source = 'main.cpp'))"
     *
     * @return A string representing the filter expression.
     */
    std::string toString() const;

    /**
     * @brief Creates a deep copy of the current FilterExpression instance.
     *        This method ensures that all internal components (conditions, sub-expressions)
     *        are also deeply copied.
     * @return A new FilterExpression object that is a deep copy of *this.
     */
    FilterExpression clone() const;

    /**
     * @brief Creates an empty filter expression.
     *        An un-negated EMPTY expression evaluates to `true` (passes all log entries).
     *        If `negated` is true, the expression evaluates to `false` (blocks all log entries).
     * @param negated If true, the empty expression will act as an 'always false' filter.
     * @return An empty FilterExpression.
     */
    static FilterExpression makeEmpty(bool negated = false);

    /**
     * @brief Creates a filter expression from a single condition.
     * @param condition The FilterCondition to encapsulate.
     * @param negated If true, the result of the condition evaluation is negated.
     * @return A FilterExpression representing the condition.
     */
    static FilterExpression makeCondition(FilterCondition condition, bool negated = false);

    /**
     * @brief Creates a logical AND expression from a list of sub-expressions.
     *        Automatically flattens nested AND expressions (e.g., (A AND (B AND C)) becomes (A AND B AND C)).
     * @param expressions A vector of FilterExpression objects to be combined with AND.
     * @param negated If true, the result of the entire AND expression is negated.
     * @return A FilterExpression representing the logical AND.
     */
    static FilterExpression makeAnd(std::vector<FilterExpression> expressions, bool negated = false);

    /**
     * @brief Creates a logical OR expression from a list of sub-expressions.
     *        Automatically flattens nested OR expressions (e.g., (A OR (B OR C)) becomes (A OR B OR C)).
     * @param expressions A vector of FilterExpression objects to be combined with OR.
     * @param negated If true, the result of the entire OR expression is negated.
     * @return A FilterExpression representing the logical OR.
     */
    static FilterExpression makeOr(std::vector<FilterExpression> expressions, bool negated = false);

    /**
     * @brief Creates a negated version of an existing filter expression.
     *        If the input expression `expr` is already negated, this method effectively
     *        un-negates it (i.e., NOT (NOT A) = A).
     * @param expr The FilterExpression to negate. Its `negated_` state will be toggled.
     * @return A new FilterExpression with its negation state flipped.
     */
    static FilterExpression makeNot(FilterExpression expr);

private:
    // Helper to convert a FilterCondition to its string representation.
    static std::string conditionToString(const FilterCondition& cond) ;

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

constexpr size_t MAX_JSON_RECURSION_DEPTH = 50;

// Helper function for recursive from_json calls to manage JSON path
inline ErrorCode::Result<void> from_json_recursive(const nlohmann::json& j, FilterExpression& fe, const std::string& current_path, size_t depth = 0) {
    using namespace FilterJsonUtils;

    if (depth > MAX_JSON_RECURSION_DEPTH) {
        return std::unexpected(makeError(Code::InvalidArgument, "Maximum recursion depth exceeded.", current_path, ""));
    }

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
            auto result = from_json_recursive(operands_array[i], operand_fe, next_path, depth + 1);
            if (!result) return std::unexpected(result.error());
            operands.push_back(std::move(operand_fe));
        }
        fe = FilterExpression(op, std::move(operands), current_negated);
    } else {
        // If neither 'condition' nor 'operator' is present, treat as EMPTY expression.
        // This allows correct round-trip serialization of default-constructed FilterExpression.
        fe = FilterExpression::makeEmpty(current_negated);
    }
    
    return {}; // Success
}

// Main from_json entry point for FilterExpression
inline ErrorCode::Result<void> from_json(const nlohmann::json& j, FilterExpression& fe) {
    // Start recursive parsing with root path "/"
    return from_json_recursive(j, fe, "/", 0);
}

} // namespace filter
