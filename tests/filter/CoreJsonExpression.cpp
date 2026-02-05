#include <gtest/gtest.h>
#include "filter/Core.h"
#include "core/LogTypes.h"
#include <nlohmann/json.hpp>

class FilterExpressionJsonTest : public ::testing::Test {
protected:
    FilterCondition createCondition(LogEntryField field, FilterOperator op, const std::string& value) {
        FilterCondition cond;
        cond.field = field;
        cond.op = op;
        cond.value = value;
        cond.valueType = FilterValueType::STRING;
        return cond;
    }
};

TEST_F(FilterExpressionJsonTest, SingleConditionExpressionToJson) {
    FilterExpression expr(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "error"));
    nlohmann::json j;
    to_json(j, expr);

    ASSERT_TRUE(j.contains("condition"));
    EXPECT_EQ(j["condition"]["field"], "MESSAGE");
    EXPECT_EQ(j["condition"]["value"], "error");
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
    EXPECT_EQ(cond->value, "error");
}

TEST_F(FilterExpressionJsonTest, AndExpressionFromJson) {
    nlohmann::json j = {
        {"operator", "AND"},
        {"operands", [
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
        ]}
    };

    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    EXPECT_TRUE(expr.isLogical());
    EXPECT_EQ(*expr.getLogicalOperator(), FilterLogicalOperator::AND);
    EXPECT_EQ(expr.getExpressions().size(), 2);
}

TEST_F(FilterExpressionJsonTest, NestedExpressionFromJson) {
    nlohmann::json j = {
        {"operator", "OR"},
        {"operands", [
            {{"condition", {
                {"field", "LEVEL"},
                {"op", "EQUALS"},
                {"value", "FATAL"},
                {"value_type", "STRING"}
            }}},
            {{"operator", "AND"},
             {"operands", [
                {{"condition", {
                    {"field", "MESSAGE"},
                    {"op", "CONTAINS"},
                    {"value", "timeout"},
                    {"value_type", "STRING"}
                }}},
                {{"expression", {
                    {"operator", "NOT"},
                    {"operands", [
                        {{"condition", {
                            {"field", "SOURCE"},
                            {"op", "EQUALS"},
                            {"value", "test_suite"},
                            {"value_type", "STRING"}
                        }}}
                    ]}
                }}}
             ]}
            }
        ]}
    };

    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_TRUE(result.has_value()) << result.error().toString();
    
    EXPECT_TRUE(expr.isLogical());
    EXPECT_EQ(*expr.getLogicalOperator(), FilterLogicalOperator::OR);
    EXPECT_EQ(expr.getExpressions().size(), 2);

    const auto& nestedAnd = expr.getExpressions()[1];
    EXPECT_TRUE(nestedAnd.isLogical());
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
    EXPECT_EQ(j["operands"][0]["condition"]["value"], "ERROR");
    EXPECT_EQ(j["operands"][1]["condition"]["value"], "database");
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
    EXPECT_EQ(j["condition"]["value"], "success");
    ASSERT_TRUE(j.contains("negated"));
    EXPECT_TRUE(j["negated"].get<bool>());
}

TEST_F(FilterExpressionJsonTest, NestedExpressionToJson) {
    FilterExpression nestedAnd(FilterLogicalOperator::AND, {
        FilterExpression(createCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "timeout")),
        FilterExpression(createCondition(LogEntryField::SOURCE, FilterOperator::EQUALS, "backend"), true) // Negated
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
    EXPECT_EQ(nested["operands"][0]["condition"]["value"], "timeout");
    EXPECT_TRUE(nested["operands"][1].contains("negated"));
    EXPECT_TRUE(nested["operpans"][1]["negated"].get<bool>());
}

TEST_F(FilterExpressionJsonTest, FromJsonInvalidOperator) {
    nlohmann::json j = {
        {"operator", "XOR"},
        {"operands", []}
    };

    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Unknown filter logical operator"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/operator");
}

TEST_F(FilterExpressionJsonTest, FromJsonMissingOperands) {
    nlohmann::json j = {
        {"operator", "AND"}
    };

    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("must contain 'operands' array"), std::string::npos);
    EXPECT_EQ(result.error().jsonPath, "/operands");
}

TEST_F(FilterExpressionJsonTest, FromJsonNestedErrorPath) {
    nlohmann::json j = {
        {"operator", "AND"},
        {"operands", [
            {{"condition", {
                {"field", "LEVEL"},
                {"op", "EQUALS"},
                {"value", "ERROR"},
                {"value_type", "STRING"}
            }}},
            {{"operator", "OR"},
             {"operands", [
                {{"condition", {
                    {"field", "MESSAGE"},
                    {"op", "INVALID_OP"},
                    {"value", "timeout"},
                    {"value_type", "STRING"}
                }}}
             ]}
            }
        ]}
    };

    FilterExpression expr;
    auto result = from_json(j, expr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_EQ(result.error().jsonPath, "/operands[1]/operands[0]/condition/op");
}
