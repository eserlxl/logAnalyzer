#include <gtest/gtest.h>
#include "../include/Filter.h"
#include "../include/LogTypes.h"
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

// FilterRule JSON tests
TEST_F(FilterJsonTest, FilterRuleToJson) {
    FilterRule fr;
    fr.field = LogEntryField::MESSAGE;
    fr.op = FilterOperator::CONTAINS;
    fr.value = "error";
    fr.caseSensitive = true;

    nlohmann::json j;
    to_json(j, fr);

    EXPECT_EQ(j["field"], "message");
    EXPECT_EQ(j["op"], "CONTAINS");
    EXPECT_EQ(j["value"], "error");
    EXPECT_EQ(j["caseSensitive"], true);
}

TEST_F(FilterJsonTest, FilterRuleFromJsonSuccess) {
    nlohmann::json j = {
        {"field", "level"},
        {"op", "EQUALS"},
        {"value", "INFO"},
        {"caseSensitive", false}
    };

    FilterRule fr;
    auto result = from_json(j, fr);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_EQ(fr.field, LogEntryField::LEVEL);
    EXPECT_EQ(fr.op, FilterOperator::EQUALS);
    EXPECT_EQ(fr.value, "INFO");
    EXPECT_EQ(fr.caseSensitive, false);
}

TEST_F(FilterJsonTest, FilterRuleFromJsonInvalidField) {
    nlohmann::json j = {
        {"field", "non_existent_field"},
        {"op", "EQUALS"},
        {"value", "INFO"}
    };

    FilterRule fr;
    auto result = from_json(j, fr);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("unrecognized field"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterRuleFromJsonMissingField) {
    nlohmann::json j = {
        {"op", "EQUALS"},
        {"value", "INFO"}
    };

    FilterRule fr;
    auto result = from_json(j, fr);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("missing or has invalid 'field'"), std::string::npos);
}

// FilterCondition JSON tests
TEST_F(FilterJsonTest, FilterConditionToJson) {
    FilterCondition fc = createFilterCondition(
        LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-01-01T00:00:00Z",
        FilterValueType::DATETIME, false, "%Y-%m-%dT%H:%M:%SZ"
    );

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "timestamp");
    EXPECT_EQ(j["op"], "GREATER_THAN");
    EXPECT_EQ(j["value"], "2023-01-01T00:00:00Z");
    EXPECT_EQ(j["value_type"], "DATETIME");
    EXPECT_EQ(j["caseSensitive"], false);
    EXPECT_EQ(j["datetimeFormat"], "%Y-%m-%dT%H:%M:%SZ");
}

TEST_F(FilterJsonTest, FilterConditionFromJsonSuccess) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "warning"},
        {"value_type", "STRING"},
        {"caseSensitive", true}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::MESSAGE);
    EXPECT_EQ(fc.op, FilterOperator::CONTAINS);
    EXPECT_EQ(fc.value, "warning");
    EXPECT_EQ(fc.valueType, FilterValueType::STRING);
    EXPECT_EQ(fc.caseSensitive, true);
    EXPECT_FALSE(fc.datetimeFormat.has_value());
}

TEST_F(FilterJsonTest, FilterConditionFromJsonDatetimeSuccess) {
    nlohmann::json j = {
        {"field", "timestamp"},
        {"op", "LESS_THAN"},
        {"value", "2023-12-31 23:59:59"},
        {"value_type", "DATETIME"},
        {"caseSensitive", false},
        {"datetimeFormat", "%Y-%m-%d %H:%M:%S"}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::TIMESTAMP);
    EXPECT_EQ(fc.op, FilterOperator::LESS_THAN);
    EXPECT_EQ(fc.value, "2023-12-31 23:59:59");
    EXPECT_EQ(fc.valueType, FilterValueType::DATETIME);
    EXPECT_TRUE(fc.datetimeFormat.has_value());
    EXPECT_EQ(*fc.datetimeFormat, "%Y-%m-%d %H:%M:%S");
}

TEST_F(FilterJsonTest, FilterConditionFromJsonInvalidField) {
    nlohmann::json j = {
        {"field", "bad_field"},
        {"op", "EQUALS"},
        {"value", "value"},
        {"value_type", static_cast<int>(FilterValueType::STRING)}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("unrecognized 'field' string"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterConditionFromJsonMissingOp) {
    nlohmann::json j = {
        {"field", "message"},
        {"value", "value"},
        {"value_type", static_cast<int>(FilterValueType::STRING)}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("missing or has invalid 'op'"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterConditionFromJsonInvalidValueTypeString) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "warning"},
        {"value_type", "BAD_TYPE"}, // Invalid value_type string
        {"caseSensitive", true}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("unrecognized 'value_type' string"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterConditionFromJsonNumericSuccess) {
    nlohmann::json j = {
        {"field", "thread_id"},
        {"op", "GREATER_THAN"},
        {"value", "100"},
        {"value_type", "NUMERIC"},
        {"caseSensitive", false}
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_EQ(fc.field, LogEntryField::THREAD_ID);
    EXPECT_EQ(fc.op, FilterOperator::GREATER_THAN);
    EXPECT_EQ(fc.value, "100");
    EXPECT_EQ(fc.valueType, FilterValueType::NUMERIC);
    EXPECT_FALSE(fc.datetimeFormat.has_value());
}

TEST_F(FilterJsonTest, FilterConditionFromJsonDatetimeMissingFormat) {
    nlohmann::json j = {
        {"field", "timestamp"},
        {"op", "LESS_THAN"},
        {"value", "2023-12-31 23:59:59"},
        {"value_type", "DATETIME"},
        {"caseSensitive", false}
        // datetimeFormat is missing
    };

    FilterCondition fc;
    auto result = from_json(j, fc);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("with DATETIME 'value_type' requires a 'datetimeFormat'"), std::string::npos);
}

// FilterCondition datetime constructor validation
TEST_F(FilterJsonTest, FilterConditionDatetimeConstructorValidation) {
    // Should throw if DATETIME type is used without format
    EXPECT_THROW(
        FilterCondition(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-01-01", FilterValueType::DATETIME),
        std::invalid_argument
    );

    // Should not throw if DATETIME type is used with format
    EXPECT_NO_THROW(
        FilterCondition(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-01-01", "%Y-%m-%d")
    );

    // Should not throw for other value types
    EXPECT_NO_THROW(
        FilterCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "error", FilterValueType::STRING)
    );
}

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

TEST_F(FilterJsonTest, FilterRuleFromJsonMissingOp) {
    nlohmann::json j = {
        {"field", "level"},
        {"value", "INFO"}
    };

    FilterRule fr;
    auto result = from_json(j, fr);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("missing or has invalid 'op'"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterRuleFromJsonMissingValue) {
    nlohmann::json j = {
        {"field", "level"},
        {"op", "EQUALS"}
    };

    FilterRule fr;
    auto result = from_json(j, fr);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("missing or has invalid 'value'"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterRuleFromJsonInvalidOp) {
    nlohmann::json j = {
        {"field", "level"},
        {"op", "INVALID_OP"},
        {"value", "INFO"}
    };

    FilterRule fr;
    auto result = from_json(j, fr);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("unrecognized 'op' string"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterConditionFromJsonMissingValue) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value_type", "STRING"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("missing or has invalid 'value'"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterConditionFromJsonMissingValueType) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "some_value"}
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("missing 'value_type'"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterConditionFromJsonMalformedDatetimeFormat) {
    nlohmann::json j = {
        {"field", "timestamp"},
        {"op", "LESS_THAN"},
        {"value", "2023-12-31"},
        {"value_type", "DATETIME"},
        {"datetimeFormat", 12345} // Invalid type, should be string
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("'datetimeFormat' must be a string or null"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterConditionFromJsonInvalidValueTypeInt) {
    nlohmann::json j = {
        {"field", "message"},
        {"op", "CONTAINS"},
        {"value", "warning"},
        {"value_type", 99}, // Invalid integer value
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("invalid integer for 'value_type'"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterConditionFromJsonLegacyValueTypeInt) {
    nlohmann::json j = {
        {"field", "thread_id"},
        {"op", "EQUALS"},
        {"value", "42"},
        {"value_type", 1}, // Legacy integer for NUMERIC
    };
    FilterCondition fc;
    auto result = from_json(j, fc);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(fc.valueType, FilterValueType::NUMERIC);
}

TEST_F(FilterJsonTest, FilterExpressionDefaultConstructor) {
    FilterExpression fe;
    EXPECT_EQ(fe.getType(), FilterExpression::ExpressionType::EMPTY);
    EXPECT_FALSE(fe.getCondition().has_value());
    EXPECT_FALSE(fe.getLogicalOperator().has_value());
    EXPECT_TRUE(fe.getExpressions().empty());
}
