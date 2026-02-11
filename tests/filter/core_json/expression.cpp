// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "filter/core.h"
#include "core/log/types.h"
#include <nlohmann/json.hpp>

using namespace filter;

class FilterExpressionJsonTest : public ::testing::Test {
protected:
    // Generic helper for creating conditions with different value types
    template<typename T>
    static FilterCondition createCondition(LogEntryField field, FilterOperator op, T value, FilterValueType valueType) {
        FilterCondition cond;
        cond.field = field;
        cond.op = op;
        cond.value = value;
        cond.valueType = valueType;
        return cond;
    }

    // Overload for string literals to default to STRING type
    static FilterCondition createCondition(LogEntryField field, FilterOperator op, const char* value) {
        return createCondition(field, op, std::string(value), FilterValueType::STRING);
    }
};

TEST_F(FilterExpressionJsonTest, SingleConditionExpressionToJson) {
    FilterExpression expr(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "error"));
    nlohmann::json j;
    to_json(j, expr);

    ASSERT_TRUE(j.contains("condition"));
    EXPECT_EQ(j["condition"]["field"], "MESSAGE");
    EXPECT_EQ(j["condition"]["value"].get<std::string>(), "error");
}

TEST_F(FilterExpressionJsonTest, SingleConditionExpressionFromJson) {
    nlohmann::json j = {
        {"condition", {
            {"field", "MESSAGE"},
            {"op", "CONTAINS"},
            {"value", "error"},
            {"value_type", "STRING"}
        }}
    };

    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    EXPECT_TRUE(expr.isCondition());
    const auto& cond = expr.getCondition();
    ASSERT_TRUE(cond.has_value());
    EXPECT_EQ(cond->field, LogEntryField::MESSAGE);
    EXPECT_EQ(std::get<std::string>(cond->value), "error");
}

TEST_F(FilterExpressionJsonTest, AndExpressionFromJson) {
    nlohmann::json j = {
        {"operator", "AND"},
        {"operands", {
            {{"condition", {
                {"field", "LEVEL"},
                {"op", "EQUALS"},
                {"value", "ERROR"},
                {"value_type", "STRING"}
            }}},
            {{"condition", {
                {"field", "MESSAGE"},
                {"op", "CONTAINS"},
                {"value", "database"},
                {"value_type", "STRING"}
            }}}
        }}
    };

    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    EXPECT_TRUE(expr.isLogical());
    ASSERT_TRUE(expr.getLogicalOperator().has_value());
    EXPECT_EQ(*expr.getLogicalOperator(), FilterLogicalOperator::AND);
    EXPECT_EQ(expr.getExpressions().size(), 2);
}

TEST_F(FilterExpressionJsonTest, NestedExpressionFromJson) {
    nlohmann::json j = {
        {"operator", "OR"},
        {"operands", {
            {{"condition", {
                {"field", "LEVEL"},
                {"op", "EQUALS"},
                {"value", "FATAL"},
                {"value_type", "STRING"}
            }}},
            {{"operator", "AND"},
             {"operands", {
                {{"condition", {
                    {"field", "MESSAGE"},
                    {"op", "CONTAINS"},
                    {"value", "timeout"},
                    {"value_type", "STRING"}
                }}},
                {
                    {"condition", {
                        {"field", "SOURCE_FILE"},
                        {"op", "EQUALS"},
                        {"value", "test_suite"},
                        {"value_type", "STRING"}
                    }},
                    {"negated", true}
                }
             }}
            }
        }}
    };

    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_TRUE(result.has_value()) << result.error().toString();
    
    EXPECT_TRUE(expr.isLogical());
    ASSERT_TRUE(expr.getLogicalOperator().has_value());
    EXPECT_EQ(*expr.getLogicalOperator(), FilterLogicalOperator::OR);
    EXPECT_EQ(expr.getExpressions().size(), 2);

    const auto& nestedAnd = expr.getExpressions()[1];
    EXPECT_TRUE(nestedAnd.isLogical());
    ASSERT_TRUE(nestedAnd.getLogicalOperator().has_value());
    EXPECT_EQ(*nestedAnd.getLogicalOperator(), FilterLogicalOperator::AND);
    EXPECT_EQ(nestedAnd.getExpressions().size(), 2);
}

TEST_F(FilterExpressionJsonTest, AndExpressionToJson) {
    FilterExpression expr = FilterExpression(FilterLogicalOperator::AND, {
        FilterExpression(createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "ERROR")),
        FilterExpression(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "database"))
    });

    nlohmann::json j;
    to_json(j, expr);

    EXPECT_EQ(j["operator"], "AND");
    ASSERT_TRUE(j["operands"].is_array());
    EXPECT_EQ(j["operands"].size(), 2);
    EXPECT_EQ(j["operands"][0]["condition"]["value"].get<std::string>(), "ERROR");
    EXPECT_EQ(j["operands"][1]["condition"]["value"].get<std::string>(), "database");
}

TEST_F(FilterExpressionJsonTest, OrExpressionToJson) {
    FilterExpression expr = FilterExpression(FilterLogicalOperator::OR, {
        FilterExpression(createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "WARN")),
        FilterExpression(createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "FATAL"))
    });

    nlohmann::json j;
    to_json(j, expr);

    EXPECT_EQ(j["operator"], "OR");
    ASSERT_TRUE(j["operands"].is_array());
    EXPECT_EQ(j["operands"].size(), 2);
}

TEST_F(FilterExpressionJsonTest, NotExpressionToJson) {
    FilterExpression expr(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "success"), true); // Negated condition
    
    nlohmann::json j;
    to_json(j, expr);
    
    ASSERT_TRUE(j.contains("condition"));
    EXPECT_EQ(j["condition"]["value"].get<std::string>(), "success");
    ASSERT_TRUE(j.contains("negated"));
    EXPECT_TRUE(j["negated"].get<bool>());
}

TEST_F(FilterExpressionJsonTest, NotExpressionFromJson) {
    nlohmann::json j = {
        {"condition", {
            {"field", "MESSAGE"},
            {"op", "CONTAINS"},
            {"value", "success"},
            {"value_type", "STRING"}
        }},
        {"negated", true}
    };

    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_TRUE(result.has_value()) << result.error().toString();
    EXPECT_TRUE(expr.isCondition());
    EXPECT_TRUE(expr.isNegated());
}


TEST_F(FilterExpressionJsonTest, NestedExpressionToJson) {
    FilterExpression nestedAnd(FilterLogicalOperator::AND, {
        FilterExpression(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "timeout")),
        FilterExpression(createCondition(LogEntryField::SOURCE_FILE, FilterOperator::EQUALS, "backend"), true) // Negated
    });

    FilterExpression expr(FilterLogicalOperator::OR, {
        FilterExpression(createCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "FATAL")),
        nestedAnd
    });

    nlohmann::json j;
    to_json(j, expr);

    EXPECT_EQ(j["operator"], "OR");
    ASSERT_EQ(j["operands"].size(), 2);
    
    const auto& nested = j["operands"][1];
    EXPECT_EQ(nested["operator"], "AND");
    ASSERT_EQ(nested["operands"].size(), 2);
    EXPECT_EQ(nested["operands"][0]["condition"]["value"].get<std::string>(), "timeout");
    EXPECT_TRUE(nested["operands"][1].contains("negated"));
    EXPECT_TRUE(nested["operands"][1]["negated"].get<bool>());
}

TEST_F(FilterExpressionJsonTest, FromJsonInvalidLogicalOperator) {
    nlohmann::json j = {
        {"operator", "XOR"},
        {"operands", {
            {{"condition", {{"field", "MESSAGE"}, {"op", "CONTAINS"}, {"value", "test"}, {"value_type", "STRING"}}}}
        }}
    };

    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Unknown logical operator"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/operator");
}

TEST_F(FilterExpressionJsonTest, FromJsonInvalidConditionOperator) {
    nlohmann::json j = {
        {"condition", {
            {"field", "MESSAGE"},
            {"op", "IS_LIKE_TOTALLY"},
            {"value", "test"},
            {"value_type", "STRING"}
        }}
    };

    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().jsonPath, "/condition/op");
}

TEST_F(FilterExpressionJsonTest, FromJsonInvalidField) {
    nlohmann::json j = {
        {"condition", {
            {"field", "NOT_A_REAL_FIELD"},
            {"op", "EQUALS"},
            {"value", "test"},
            {"value_type", "STRING"}
        }}
    };

    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_TRUE(result.has_value());
    const auto& cond = expr.getCondition();
    ASSERT_TRUE(cond.has_value());
    EXPECT_EQ(cond->field, LogEntryField::CUSTOM);
    ASSERT_TRUE(cond->customField.has_value());
    EXPECT_EQ(cond->customField.value(), "NOT_A_REAL_FIELD");
}

TEST_F(FilterExpressionJsonTest, FromJsonMissingOperands) {
    nlohmann::json j = {
        {"operator", "AND"}
    };

    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("requires 'operands' array"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/operands");
}

TEST_F(FilterExpressionJsonTest, FromJsonMissingConditionField) {
    nlohmann::json j = {
        {"condition", {
            {"op", "EQUALS"},
            {"value", "test"},
            {"value_type", "STRING"}
        }}
    };

    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().jsonPath, "/condition/field");
}


TEST_F(FilterExpressionJsonTest, FromJsonNestedErrorPath) {
    nlohmann::json j = {
        {"operator", "AND"},
        {"operands", {
            {{"condition", {
                {"field", "LEVEL"},
                {"op", "EQUALS"},
                {"value", "ERROR"},
                {"value_type", "STRING"}
            }}},
            {{"operator", "OR"},
             {"operands", {
                {{"condition", {
                    {"field", "MESSAGE"},
                    {"op", "INVALID_OP"},
                    {"value", "timeout"},
                    {"value_type", "STRING"}
                }}}
             }}
            }
        }}
    };

    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().jsonPath, "/operands[1]/operands[0]/condition/op");
}

TEST_F(FilterExpressionJsonTest, EmptyExpressionRoundTrip) {
    FilterExpression original = FilterExpression::makeEmpty();
    nlohmann::json j;
    to_json(j, original);
    
    EXPECT_TRUE(j.empty() || (j.contains("negated") && !j["negated"].get<bool>()));

    FilterExpression deserialized;
    auto result = from_json(j, deserialized);
    ASSERT_TRUE(result.has_value()) << result.error().toString();
    EXPECT_EQ(deserialized.getType(), FilterExpression::ExpressionType::EMPTY);
    EXPECT_FALSE(deserialized.isNegated());

    // Test negated empty
    FilterExpression originalNegated = FilterExpression::makeEmpty(true);
    j.clear();
    to_json(j, originalNegated);
    EXPECT_TRUE(j.contains("negated"));
    EXPECT_TRUE(j["negated"].get<bool>());

    FilterExpression deserializedNegated;
    result = from_json(j, deserializedNegated);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(deserializedNegated.getType(), FilterExpression::ExpressionType::EMPTY);
    EXPECT_TRUE(deserializedNegated.isNegated());
}

TEST_F(FilterExpressionJsonTest, MaxRecursionDepthExceeded) {
    // Create a deeply nested JSON that exceeds limit
    nlohmann::json j;
    nlohmann::json* current = &j;
    for (size_t i = 0; i < MAX_JSON_RECURSION_DEPTH + 10; ++i) {
        (*current)["operator"] = "AND";
        (*current)["operands"] = nlohmann::json::array();
        (*current)["operands"].push_back(nlohmann::json::object());
        current = &((*current)["operands"][0]);
    }
    (*current)["condition"] = {
        {"field", "MESSAGE"},
        {"op", "CONTAINS"},
        {"value", "test"},
        {"value_type", "STRING"}
    };

    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Maximum recursion depth exceeded"), std::string::npos);
}

TEST_F(FilterExpressionJsonTest, EmptyOperandsArrayFails) {
    nlohmann::json j = {
        {"operator", "AND"},
        {"operands", nlohmann::json::array()}
    };
    
    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("'operands' array cannot be empty"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/operands");
}

TEST_F(FilterExpressionJsonTest, IntegerConditionFromJson) {
    nlohmann::json j = {
        {"condition", {
            {"field", "PID"},
            {"op", "GREATER_THAN"},
            {"value", 12345},
            {"value_type", "INT"}
        }}
    };

    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    const auto& cond = expr.getCondition();
    ASSERT_TRUE(cond.has_value());
    EXPECT_EQ(cond->field, LogEntryField::ID);
    EXPECT_EQ(cond->op, FilterOperator::GREATER_THAN);
    EXPECT_EQ(std::get<int64_t>(cond->value), 12345);
}

TEST_F(FilterExpressionJsonTest, ValueTypeMismatchError) {
    nlohmann::json j = {
        {"condition", {
            {"field", "PID"},
            {"op", "EQUALS"},
            {"value", "not-a-number"},
            {"value_type", "INT"}
        }}
    };

    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("mismatch"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/condition/value");
}

TEST_F(FilterExpressionJsonTest, NonBooleanNegatedError) {
    nlohmann::json j = {
        {"condition", {
            {"field", "MESSAGE"},
            {"op", "CONTAINS"},
            {"value", "test"},
            {"value_type", "STRING"}
        }},
        {"negated", "false"} // Invalid, should be a boolean
    };

    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().jsonPath, "/negated");
}

TEST_F(FilterExpressionJsonTest, VariousTypesRoundTrip) {
    FilterExpression expr = FilterExpression(FilterLogicalOperator::OR, {
        FilterExpression(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "error")),
        FilterExpression(createCondition(LogEntryField::ID, FilterOperator::EQUALS, 42, FilterValueType::INT)),
        FilterExpression(createCondition(LogEntryField::CUSTOM, FilterOperator::LESS_THAN, 3.14, FilterValueType::DOUBLE), true)
    });

    nlohmann::json j;
    to_json(j, expr);

    FilterExpression deserialized;
    auto result = from_json(j, deserialized);
    ASSERT_TRUE(result.has_value()) << result.error().toString();
    
    EXPECT_TRUE(deserialized.isLogical());
    ASSERT_TRUE(deserialized.getLogicalOperator().has_value());
    EXPECT_EQ(*deserialized.getLogicalOperator(), FilterLogicalOperator::OR);
    
    const auto& operands = deserialized.getExpressions();
    ASSERT_EQ(operands.size(), 3);

    const auto& cond1 = operands[0].getCondition();
    ASSERT_TRUE(cond1.has_value());
    EXPECT_EQ(std::get<std::string>(cond1->value), "error");

    const auto& cond2 = operands[1].getCondition();
    ASSERT_TRUE(cond2.has_value());
    EXPECT_EQ(std::get<int64_t>(cond2->value), 42);

    const auto& cond3 = operands[2].getCondition();
    ASSERT_TRUE(cond3.has_value());
    EXPECT_DOUBLE_EQ(std::get<double>(cond3->value), 3.14);
    EXPECT_TRUE(operands[2].isNegated());
}
