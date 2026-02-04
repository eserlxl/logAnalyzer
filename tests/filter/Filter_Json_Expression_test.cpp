#include <gtest/gtest.h>
#include "filter/Filter.h"
#include "core/LogTypes.h"
#include <nlohmann/json.hpp>

// New test fixture for JSON serialization/deserialization tests
class FilterJsonTest : public ::testing::Test {
protected:
    // Utility to create a FilterCondition
    FilterCondition createFilterCondition(
        LogEntryField field,
        FilterOperator op,
        const std::string& value,
        FilterValueType valueType = FilterValueType::STRING,
        bool caseSensitive = false,
        std::optional<std::string> datetimeFormat = std::nullopt
    ) {
        FilterCondition fc;
        fc.field = field;
        fc.op = op;
        fc.value = value;
        fc.valueType = valueType;
        fc.caseSensitive = caseSensitive;
        fc.datetimeFormat = datetimeFormat;
        return fc;
    }
};

// FilterExpression JSON tests
TEST_F(FilterJsonTest, FilterExpressionToJsonCondition) {
    FilterCondition fc = createFilterCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "hello");
    FilterExpression fe(fc);

    nlohmann::json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("condition"));
    EXPECT_EQ(j["condition"]["field"], "message");
    EXPECT_EQ(j["condition"]["op"], "CONTAINS");
    EXPECT_EQ(j["condition"]["value"], "hello");
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonCondition) {
    nlohmann::json j = {
        {"condition", {
            {"field", "source_file"},
            {"op", "ENDS_WITH"},
            {"value", ".log"},
            {"value_type", "STRING"}
        }}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isCondition());
    ASSERT_TRUE(fe.getCondition().has_value());
    EXPECT_EQ(fe.getCondition()->field, LogEntryField::SOURCE_FILE);
    EXPECT_EQ(fe.getCondition()->op, FilterOperator::ENDS_WITH);
    EXPECT_EQ(fe.getCondition()->value, ".log");
}

TEST_F(FilterJsonTest, FilterExpressionToJsonLogicalAND) {
    FilterExpression fe = FilterExpression::create(createFilterCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO"))
                            .And(FilterExpression::create(createFilterCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "user")));

    nlohmann::json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("operator"));
    EXPECT_EQ(j["operator"], "AND");
    ASSERT_TRUE(j.contains("operands"));
    ASSERT_EQ(j["operands"].size(), 2);

    // Check first operand
    EXPECT_EQ(j["operands"][0]["condition"]["field"], "level");
    EXPECT_EQ(j["operands"][0]["condition"]["value"], "INFO");

    // Check second operand
    EXPECT_EQ(j["operands"][1]["condition"]["field"], "message");
    EXPECT_EQ(j["operands"][1]["condition"]["value"], "user");
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonLogicalOR) {
    nlohmann::json j = {
        {"operator", "OR"},
        {"operands", nlohmann::json::array({
            {{"condition", {{"field", "level"}, {"op", "EQUALS"}, {"value", "ERROR"}, {"value_type", "STRING"}}}},
            {{"condition", {{"field", "level"}, {"op", "EQUALS"}, {"value", "FATAL"}, {"value_type", "STRING"}}}}
        })}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isLogical());
    ASSERT_TRUE(fe.getLogicalOperator().has_value());
    EXPECT_EQ(*fe.getLogicalOperator(), FilterLogicalOperator::OR);
    ASSERT_EQ(fe.getExpressions().size(), 2);

    // Check first operand
    ASSERT_TRUE(fe.getExpressions()[0].isCondition());
    EXPECT_EQ(fe.getExpressions()[0].getCondition()->field, LogEntryField::LEVEL);
    EXPECT_EQ(fe.getExpressions()[0].getCondition()->value, "ERROR");

    // Check second operand
    ASSERT_TRUE(fe.getExpressions()[1].isCondition());
    EXPECT_EQ(fe.getExpressions()[1].getCondition()->field, LogEntryField::LEVEL);
    EXPECT_EQ(fe.getExpressions()[1].getCondition()->value, "FATAL");
}

TEST_F(FilterJsonTest, FilterExpressionToJsonLogicalNOT) {
    FilterExpression fe = FilterExpression::create(createFilterCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "DEBUG"))
                            .Not();

    nlohmann::json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("operator"));
    EXPECT_EQ(j["operator"], "NOT");
    ASSERT_TRUE(j.contains("operands"));
    ASSERT_EQ(j["operands"].size(), 1);

    // Check operand
    EXPECT_EQ(j["operands"][0]["condition"]["field"], "level");
    EXPECT_EQ(j["operands"][0]["condition"]["value"], "DEBUG");
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonLogicalNOT) {
    nlohmann::json j = {
        {"operator", "NOT"},
        {"operands", nlohmann::json::array({
            {{"condition", {{"field", "message"}, {"op", "STARTS_WITH"}, {"value", "Success"}, {"value_type", "STRING"}}}}
        })}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isLogical());
    ASSERT_TRUE(fe.getLogicalOperator().has_value());
    EXPECT_EQ(*fe.getLogicalOperator(), FilterLogicalOperator::NOT);
    ASSERT_EQ(fe.getExpressions().size(), 1);

    // Check operand
    ASSERT_TRUE(fe.getExpressions()[0].isCondition());
    EXPECT_EQ(fe.getExpressions()[0].getCondition()->field, LogEntryField::MESSAGE);
    EXPECT_EQ(fe.getExpressions()[0].getCondition()->value, "Success");
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonNested) {
    nlohmann::json j = {
        {"operator", "AND"},
        {"operands", nlohmann::json::array({
            nlohmann::json {{"condition", {{"field", "level"}, {"op", "EQUALS"}, {"value", "ERROR"}, {"value_type", "STRING"}}}},
            nlohmann::json {{"operator", "OR"},
             {"operands", nlohmann::json::array({
                nlohmann::json {{"condition", {{"field", "message"}, {"op", "CONTAINS"}, {"value", "database"}, {"value_type", "STRING"}}}},
                nlohmann::json {{"condition", {{"field", "message"}, {"op", "CONTAINS"}, {"value", "network"}, {"value_type", "STRING"}}}}
             })}}
        })}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isLogical());
    ASSERT_TRUE(fe.getLogicalOperator().has_value());
    EXPECT_EQ(*fe.getLogicalOperator(), FilterLogicalOperator::AND);
    ASSERT_EQ(fe.getExpressions().size(), 2);

    // Check first level operand (condition)
    ASSERT_TRUE(fe.getExpressions()[0].isCondition());
    EXPECT_EQ(fe.getExpressions()[0].getCondition()->field, LogEntryField::LEVEL);
    EXPECT_EQ(fe.getExpressions()[0].getCondition()->value, "ERROR");

    // Check second level operand (logical OR)
    ASSERT_TRUE(fe.getExpressions()[1].isLogical());
    EXPECT_EQ(*fe.getExpressions()[1].getLogicalOperator(), FilterLogicalOperator::OR);
    ASSERT_EQ(fe.getExpressions()[1].getExpressions().size(), 2);

    EXPECT_EQ(fe.getExpressions()[1].getExpressions()[0].getCondition()->field, LogEntryField::MESSAGE);
    EXPECT_EQ(fe.getExpressions()[1].getExpressions()[0].getCondition()->value, "database");
    EXPECT_EQ(fe.getExpressions()[1].getExpressions()[1].getCondition()->field, LogEntryField::MESSAGE);
    EXPECT_EQ(fe.getExpressions()[1].getExpressions()[1].getCondition()->value, "network");
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonInvalidOperand) {
    nlohmann::json j = {
        {"operator", "AND"},
        {"operands", nlohmann::json::array({
            {{"condition", {{"field", "level"}, {"op", "EQUALS"}, {"value", "ERROR"}, {"value_type", "STRING"}}}},
            {{"condition", {{"field", "message"}, {"op", "CONTAINS"}, {"value", "database"}, {"value_type", "STRING"}}}},
            // Invalid operand: missing "op"
            {{"condition", {{"field", "source_file"}, {"value", "main.cpp"}, {"value_type", "STRING"}}}} 
        })}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("missing or has invalid 'op'"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonMissingOperands) {
    nlohmann::json j = {
        {"operator", "AND"}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("must contain 'operands' array"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonUnknownOperator) {
    nlohmann::json j = {
        {"operator", "XOR"}, // Unknown operator
        {"operands", nlohmann::json::array({
            {{"condition", {{"field", "level"}, {"op", "EQUALS"}, {"value", "ERROR"}, {"value_type", "STRING"}}}}
        })}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Unknown filter logical operator"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonMissingConditionOrOperator) {
    nlohmann::json j = {
        {"invalid_key", "some_value"}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("must contain either 'condition' or 'operator'"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterExpressionDefaultConstructor) {
    FilterExpression fe;
    EXPECT_EQ(fe.getType(), FilterExpression::ExpressionType::EMPTY);
    EXPECT_FALSE(fe.getCondition().has_value());
    EXPECT_FALSE(fe.getLogicalOperator().has_value());
    EXPECT_TRUE(fe.getExpressions().empty());
}
