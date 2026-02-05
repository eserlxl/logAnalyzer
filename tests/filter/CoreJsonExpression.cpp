#include <gtest/gtest.h>
#include "filter/Core.h"
#include "core/LogTypes.h"
#include "filter/Types.h" // Include Types.h to ensure enums are available
#include <nlohmann/json.hpp>

// Define an alias for convenience
namespace {

    using json = nlohmann::json;
}

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
        std::optional<std::string> datetimeFormat = std::nullopt,
        std::optional<std::string> customFieldName = std::nullopt // Add this parameter
    ) {
        FilterCondition fc;
        fc.field = field;
        fc.op = op;
        fc.value = value;
        fc.valueType = valueType;
        fc.caseSensitive = caseSensitive;
        fc.datetimeFormat = datetimeFormat;
        if (customFieldName) { // Only set if provided
            fc.customField = customFieldName;
        }
        return fc;
    }

    // Helper to create FilterExpression from a condition
    FilterExpression createFilterExpression(
        LogEntryField field,
        FilterOperator op,
        const std::string& value,
        FilterValueType valueType = FilterValueType::STRING,
        bool caseSensitive = false,
        std::optional<std::string> datetimeFormat = std::nullopt,
        std::optional<std::string> customFieldName = std::nullopt // Add this parameter
    ) {
        return FilterExpression(createFilterCondition(field, op, value, valueType, caseSensitive, datetimeFormat, customFieldName));
    }
};

// FilterExpression JSON tests
TEST_F(FilterJsonTest, FilterExpressionToJsonCondition) {
    FilterCondition fc = createFilterCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "hello");
    FilterExpression fe(fc);

    json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("condition"));
    EXPECT_EQ(j["condition"]["field"], "MESSAGE");
    EXPECT_EQ(j["condition"]["op"], "CONTAINS");
    EXPECT_EQ(j["condition"]["value"], "hello");
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonCondition) {
    json j = {
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

    json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("operator"));
    EXPECT_EQ(j["operator"], "AND");
    ASSERT_TRUE(j.contains("operands"));
    ASSERT_EQ(j["operands"].size(), 2);

    // Check first operand
    EXPECT_EQ(j["operands"][0]["condition"]["field"], "LEVEL");
    EXPECT_EQ(j["operands"][0]["condition"]["value"], "INFO");

    // Check second operand
    EXPECT_EQ(j["operands"][1]["condition"]["field"], "MESSAGE");
    EXPECT_EQ(j["operands"][1]["condition"]["value"], "user");
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonLogicalOR) {
    json j = {
        {"operator", "OR"},
        {"operands", json::array({
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

    json j;
    to_json(j, fe);

    // After negation, the expression itself is marked as negated, not wrapped in a NOT operator.
    ASSERT_TRUE(j.contains("condition")); // The original condition is now directly in the JSON
    ASSERT_TRUE(j.contains("negated"));
    EXPECT_TRUE(j["negated"].get<bool>());

    // Check the original condition within the JSON
    EXPECT_EQ(j["condition"]["field"], "LEVEL");
    EXPECT_EQ(j["condition"]["op"], "EQUALS");
    EXPECT_EQ(j["condition"]["value"], "DEBUG");
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonLogicalNOT_Deprecated) {
    // This test case represents the old way of handling NOT, which is now deprecated and should result in an error during deserialization.
    json j = {
        {"operator", "NOT"}, // This will now cause an error during deserialization
        {"operands", json::array({
            {{"condition", {{"field", "message"}, {"op", "STARTS_WITH"}, {"value", "Success"}, {"value_type", "STRING"}}}}
        })}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    EXPECT_FALSE(result.has_value()); // Expect failure
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("Unknown filter logical operator"), std::string::npos);
}

// New test for deserializing JSON with the new "negated" flag
TEST_F(FilterJsonTest, FilterExpressionFromJsonWithNegatedFlag) {
    json j = {
        {"condition", {
            {"field", "message"},
            {"op", "STARTS_WITH"},
            {"value", "Success"},
            {"value_type", "STRING"}
        }},
        {"negated", true}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isCondition());
    EXPECT_TRUE(fe.isNegated());
    EXPECT_EQ(fe.getCondition()->field, LogEntryField::MESSAGE);
    EXPECT_EQ(fe.getCondition()->value, "Success");
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonNested) {
    json j = {
        {"operator", "AND"},
        {"operands", json::array({
            json {{"condition", {{"field", "level"}, {"op", "EQUALS"}, {"value", "ERROR"}, {"value_type", "STRING"}}}},
            json {{"operator", "OR"},
             {"operands", json::array({
                json {{"condition", {{"field", "message"}, {"op", "CONTAINS"}, {"value", "database"}, {"value_type", "STRING"}}}},
                json {{"condition", {{"field", "message"}, {"op", "CONTAINS"}, {"value", "network"}, {"value_type", "STRING"}}}}
             })}
            }
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
    json j = {
        {"operator", "AND"},
        {"operands", json::array({
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
    json j = {
        {"operator", "AND"}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::InvalidArgument);
    EXPECT_NE(result.error().message.find("must contain 'operands' array"), std::string::npos);
}

TEST_F(FilterJsonTest, FilterExpressionFromJsonUnknownOperator) {
    json j = {
        {"operator", "XOR"}, // Unknown operator
        {"operands", json::array({
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
    json j = {
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

// --- New Tests for Iteration 8 Features ---

// Tests for new FilterOperator values
TEST_F(FilterJsonTest, FilterExpressionToJson_GreaterThan) {
    FilterExpression fe = createFilterExpression(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "500", FilterValueType::INT, false, std::nullopt, "response_time");
    json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("condition"));
    EXPECT_EQ(j["condition"]["field"], "response_time"); // Expect string for custom field
    EXPECT_EQ(j["condition"]["op"], "GREATER_THAN");
    EXPECT_EQ(j["condition"]["value"], "500");
    EXPECT_EQ(j["condition"]["value_type"], "INT");
    EXPECT_EQ(j["condition"]["customField"], "response_time");
}

TEST_F(FilterJsonTest, FilterExpressionFromJson_GreaterThan) {
    json j = {
        {"condition", {
            {"field", "response_time"},
            {"op", "GREATER_THAN"},
            {"value", "500"},
            {"value_type", "INT"},
            {"customField", "response_time"}
        }}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isCondition());
    EXPECT_EQ(fe.getCondition()->field, LogEntryField::CUSTOM); // Expect CUSTOM enum
    EXPECT_EQ(fe.getCondition()->customField, "response_time"); // Expect custom field name
    EXPECT_EQ(fe.getCondition()->op, FilterOperator::GREATER_THAN);
    EXPECT_EQ(fe.getCondition()->value, "500");
    EXPECT_EQ(fe.getCondition()->valueType, FilterValueType::INT);
}

TEST_F(FilterJsonTest, FilterExpressionToJson_LessThan) {
    FilterExpression fe = createFilterExpression(LogEntryField::CUSTOM, FilterOperator::LESS_THAN, "100", FilterValueType::INT, false, std::nullopt, "response_time");
    json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("condition"));
    EXPECT_EQ(j["condition"]["field"], "response_time");
    EXPECT_EQ(j["condition"]["op"], "LESS_THAN");
    EXPECT_EQ(j["condition"]["value"], "100");
    EXPECT_EQ(j["condition"]["value_type"], "INT");
    EXPECT_EQ(j["condition"]["customField"], "response_time");
}

TEST_F(FilterJsonTest, FilterExpressionFromJson_LessThan) {
    json j = {
        {"condition", {
            {"field", "response_time"},
            {"op", "LESS_THAN"},
            {"value", "100"},
            {"value_type", "INT"},
            {"customField", "response_time"}
        }}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isCondition());
    EXPECT_EQ(fe.getCondition()->field, LogEntryField::CUSTOM);
    EXPECT_EQ(fe.getCondition()->customField, "response_time");
    EXPECT_EQ(fe.getCondition()->op, FilterOperator::LESS_THAN);
    EXPECT_EQ(fe.getCondition()->value, "100");
    EXPECT_EQ(fe.getCondition()->valueType, FilterValueType::INT);
}

TEST_F(FilterJsonTest, FilterExpressionToJson_LessThanOrEqual) {
    FilterExpression fe = createFilterExpression(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "1024", FilterValueType::INT, false, std::nullopt, "size");
    json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("condition"));
    EXPECT_EQ(j["condition"]["field"], "size");
    EXPECT_EQ(j["condition"]["op"], "LESS_THAN_OR_EQUAL");
    EXPECT_EQ(j["condition"]["value"], "1024");
    EXPECT_EQ(j["condition"]["value_type"], "INT");
    EXPECT_EQ(j["condition"]["customField"], "size");
}

TEST_F(FilterJsonTest, FilterExpressionFromJson_LessThanOrEqual) {
    json j = {
        {"condition", {
            {"field", "size"},
            {"op", "LESS_THAN_OR_EQUAL"},
            {"value", "100"},
            {"value_type", "INT"},
            {"customField", "size"}
        }}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isCondition());
    EXPECT_EQ(fe.getCondition()->field, LogEntryField::CUSTOM);
    EXPECT_EQ(fe.getCondition()->customField, "size");
    EXPECT_EQ(fe.getCondition()->op, FilterOperator::LESS_THAN_OR_EQUAL);
    EXPECT_EQ(fe.getCondition()->value, "100");
    EXPECT_EQ(fe.getCondition()->valueType, FilterValueType::INT);
}

TEST_F(FilterJsonTest, FilterExpressionToJson_GreaterThanOrEqual) {
    FilterExpression fe = createFilterExpression(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN_OR_EQUAL, "75", FilterValueType::INT, false, std::nullopt, "score");
    json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("condition"));
    EXPECT_EQ(j["condition"]["field"], "score");
    EXPECT_EQ(j["condition"]["op"], "GREATER_THAN_OR_EQUAL");
    EXPECT_EQ(j["condition"]["value"], "75");
    EXPECT_EQ(j["condition"]["value_type"], "INT");
    EXPECT_EQ(j["condition"]["customField"], "score");
}

TEST_F(FilterJsonTest, FilterExpressionFromJson_GreaterThanOrEqual) {
    json j = {
        {"condition", {
            {"field", "score"},
            {"op", "GREATER_THAN_OR_EQUAL"},
            {"value", "75"},
            {"value_type", "INT"},
            {"customField", "score"}
        }}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isCondition());
    EXPECT_EQ(fe.getCondition()->field, LogEntryField::CUSTOM);
    EXPECT_EQ(fe.getCondition()->customField, "score");
    EXPECT_EQ(fe.getCondition()->op, FilterOperator::GREATER_THAN_OR_EQUAL);
    EXPECT_EQ(fe.getCondition()->value, "75");
    EXPECT_EQ(fe.getCondition()->valueType, FilterValueType::INT);
}


TEST_F(FilterJsonTest, FilterExpressionToJson_RegexMatch) {
    FilterExpression fe = createFilterExpression(LogEntryField::MESSAGE, FilterOperator::REGEX_MATCH, "^Error \\d+.*");
    json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("condition"));
    EXPECT_EQ(j["condition"]["field"], "MESSAGE");
    EXPECT_EQ(j["condition"]["op"], "REGEX_MATCH");
    EXPECT_EQ(j["condition"]["value"], "^Error \\d+.*");
    EXPECT_EQ(j["condition"]["value_type"], "STRING"); // Regex match is typically string-based
}

TEST_F(FilterJsonTest, FilterExpressionFromJson_RegexMatch) {
    json j = {
        {"condition", {
            {"field", "message"},
            {"op", "REGEX_MATCH"},
            {"value", ".*(WARN|ERROR).*"}, // Note: Backslashes in regex might need escaping in JSON string
            {"value_type", "STRING"}
        }}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isCondition());
    EXPECT_EQ(fe.getCondition()->field, LogEntryField::MESSAGE);
    EXPECT_EQ(fe.getCondition()->op, FilterOperator::REGEX_MATCH);
    EXPECT_EQ(fe.getCondition()->value, ".*(WARN|ERROR).*");
    EXPECT_EQ(fe.getCondition()->valueType, FilterValueType::STRING);
}

TEST_F(FilterJsonTest, FilterExpressionToJson_IsPresent) {
    FilterExpression fe = createFilterExpression(LogEntryField::CUSTOM, FilterOperator::IS_PRESENT, "", FilterValueType::STRING, false, std::nullopt, "user_id"); // Value is not used for IS_PRESENT/IS_ABSENT
    json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("condition"));
    EXPECT_EQ(j["condition"]["field"], "user_id");
    EXPECT_EQ(j["condition"]["op"], "IS_PRESENT");
    EXPECT_EQ(j["condition"]["value"], ""); // Value might be empty or omitted in JSON, depends on implementation
    EXPECT_EQ(j["condition"]["value_type"], "STRING"); // Default value type, not strictly relevant
    EXPECT_EQ(j["condition"]["customField"], "user_id");
}

TEST_F(FilterJsonTest, FilterExpressionFromJson_IsPresent) {
    json j = {
        {"condition", {
            {"field", "request_id"},
            {"op", "IS_PRESENT"},
            {"value", ""}, // Value can be empty for present/absent checks
            {"value_type", "STRING"},
            {"customField", "request_id"}
        }}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isCondition());
    EXPECT_EQ(fe.getCondition()->field, LogEntryField::CUSTOM);
    EXPECT_EQ(fe.getCondition()->customField, "request_id");
    EXPECT_EQ(fe.getCondition()->op, FilterOperator::IS_PRESENT);
    EXPECT_EQ(fe.getCondition()->value, ""); // Expecting empty string if present in JSON
    EXPECT_EQ(fe.getCondition()->valueType, FilterValueType::STRING); // Type might be irrelevant for these ops
}

TEST_F(FilterJsonTest, FilterExpressionToJson_IsAbsent) {
    FilterExpression fe = createFilterExpression(LogEntryField::CUSTOM, FilterOperator::IS_ABSENT, "", FilterValueType::STRING, false, std::nullopt, "email");
    json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("condition"));
    EXPECT_EQ(j["condition"]["field"], "email");
    EXPECT_EQ(j["condition"]["op"], "IS_ABSENT");
    EXPECT_EQ(j["condition"]["value"], "");
    EXPECT_EQ(j["condition"]["value_type"], "STRING");
    EXPECT_EQ(j["condition"]["customField"], "email");
}

TEST_F(FilterJsonTest, FilterExpressionFromJson_IsAbsent) {
    json j = {
        {"condition", {
            {"field", "email"},
            {"op", "IS_ABSENT"},
            {"value", ""},
            {"value_type", "STRING"},
            {"customField", "email"}
        }}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isCondition());
    EXPECT_EQ(fe.getCondition()->field, LogEntryField::CUSTOM);
    EXPECT_EQ(fe.getCondition()->customField, "email");
    EXPECT_EQ(fe.getCondition()->op, FilterOperator::IS_ABSENT);
    EXPECT_EQ(fe.getCondition()->value, "");
    EXPECT_EQ(fe.getCondition()->valueType, FilterValueType::STRING);
}

// Tests for FilterValueType::BOOL
TEST_F(FilterJsonTest, FilterExpressionToJson_BooleanTrue) {
    FilterExpression fe = createFilterExpression(LogEntryField::CUSTOM, FilterOperator::EQUALS, "true", FilterValueType::BOOL, false, std::nullopt, "enabled");
    json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("condition"));
    EXPECT_EQ(j["condition"]["field"], "enabled");
    EXPECT_EQ(j["condition"]["op"], "EQUALS");
    EXPECT_EQ(j["condition"]["value"], "true"); 
    EXPECT_EQ(j["condition"]["value_type"], "BOOL");
    EXPECT_EQ(j["condition"]["customField"], "enabled");
}

TEST_F(FilterJsonTest, FilterExpressionFromJson_BooleanTrue) {
    json j = {
        {"condition", {
            {"field", "is_active"},
            {"op", "EQUALS"},
            {"value", "true"},
            {"value_type", "BOOL"},
            {"customField", "is_active"}
        }}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isCondition());
    EXPECT_EQ(fe.getCondition()->field, LogEntryField::CUSTOM);
    EXPECT_EQ(fe.getCondition()->customField, "is_active");
    EXPECT_EQ(fe.getCondition()->op, FilterOperator::EQUALS);
    EXPECT_EQ(fe.getCondition()->value, "true"); // Expecting stringified boolean
    EXPECT_EQ(fe.getCondition()->valueType, FilterValueType::BOOL);
}

TEST_F(FilterJsonTest, FilterExpressionToJson_BooleanFalse) {
    FilterExpression fe = createFilterExpression(LogEntryField::CUSTOM, FilterOperator::EQUALS, "false", FilterValueType::BOOL, false, std::nullopt, "admin");
    json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("condition"));
    EXPECT_EQ(j["condition"]["field"], "admin");
    EXPECT_EQ(j["condition"]["op"], "EQUALS");
    EXPECT_EQ(j["condition"]["value"], "false");
    EXPECT_EQ(j["condition"]["value_type"], "BOOL");
    EXPECT_EQ(j["condition"]["customField"], "admin");
}

TEST_F(FilterJsonTest, FilterExpressionFromJson_BooleanFalse) {
    json j = {
        {"condition", {
            {"field", "is_admin"},
            {"op", "EQUALS"},
            {"value", "false"},
            {"value_type", "BOOL"},
            {"customField", "is_admin"}
        }}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isCondition());
    EXPECT_EQ(fe.getCondition()->field, LogEntryField::CUSTOM);
    EXPECT_EQ(fe.getCondition()->customField, "is_admin");
    EXPECT_EQ(fe.getCondition()->op, FilterOperator::EQUALS);
    EXPECT_EQ(fe.getCondition()->value, "false");
    EXPECT_EQ(fe.getCondition()->valueType, FilterValueType::BOOL);
}

// Test with a mix of new and old operators/types
TEST_F(FilterJsonTest, FilterExpressionToJson_MixedOperatorsAndTypes) {
    FilterExpression fe = FilterExpression::create(createFilterCondition(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO", FilterValueType::STRING))
                            .And(createFilterExpression(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "1678886400", FilterValueType::INT))
                            .Or(createFilterExpression(LogEntryField::CUSTOM, FilterOperator::EQUALS, "true", FilterValueType::BOOL, false, std::nullopt, "is_active"))
                            .And(createFilterExpression(LogEntryField::MESSAGE, FilterOperator::REGEX_MATCH, ".*\\b(test|debug)\\b.*"));

    json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("operator"));
    EXPECT_EQ(j["operator"], "AND");
    ASSERT_EQ(j["operands"].size(), 2);

    // First operand (INFO)
    EXPECT_EQ(j["operands"][0]["condition"]["value"], "INFO");
    EXPECT_EQ(j["operands"][0]["condition"]["value_type"], "STRING");

    // Second operand (timestamp >)
    EXPECT_EQ(j["operands"][1]["condition"]["op"], "GREATER_THAN");
    EXPECT_EQ(j["operands"][1]["condition"]["value"], "1678886400");
    EXPECT_EQ(j["operands"][1]["condition"]["value_type"], "INT");

    // Third operand (IS_ACTIVE == true)
    EXPECT_EQ(j["operands"][2]["condition"]["value"], "true");
    EXPECT_EQ(j["operands"][2]["condition"]["value_type"], "BOOL");
}

TEST_F(FilterJsonTest, FilterExpressionFromJson_MixedOperatorsAndTypes) {
    json j = {
        {"operator", "OR"},
        {"operands", json::array({
            nlohmann::json {{"condition", {{"field", "level"}, {"op", "EQUALS"}, {"value", "WARN"}, {"value_type", "STRING"}}}},
            nlohmann::json {{"condition", {{"field", "status_code"}, {"op", "LESS_THAN"}, {"value", "400"}, {"value_type", "INT"}}}},
            nlohmann::json {{"condition", {{"field", "is_processed"}, {"op", "EQUALS"}, {"value", "false"}, {"value_type", "BOOL"}}}},
            nlohmann::json {{"condition", {{"field", "path"}, {"op", "REGEX_MATCH"}, {"value", "/api/v1/.*"}, {"value_type", "STRING"}}}}
        })}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isLogical());
    ASSERT_TRUE(fe.getLogicalOperator().has_value());
    EXPECT_EQ(*fe.getLogicalOperator(), FilterLogicalOperator::OR);
    ASSERT_EQ(fe.getExpressions().size(), 4);

    // Check each operand
    EXPECT_EQ(fe.getExpressions()[0].getCondition()->field, LogEntryField::LEVEL);
    EXPECT_EQ(fe.getExpressions()[0].getCondition()->value, "WARN");
    EXPECT_EQ(fe.getExpressions()[0].getCondition()->valueType, FilterValueType::STRING);

    EXPECT_EQ(fe.getExpressions()[1].getCondition()->field, LogEntryField::CUSTOM);
    EXPECT_EQ(fe.getExpressions()[1].getCondition()->customField, "status_code");
    EXPECT_EQ(fe.getExpressions()[1].getCondition()->op, FilterOperator::LESS_THAN);
    EXPECT_EQ(fe.getExpressions()[1].getCondition()->value, "400");
    EXPECT_EQ(fe.getExpressions()[1].getCondition()->valueType, FilterValueType::INT);

    EXPECT_EQ(fe.getExpressions()[2].getCondition()->field, LogEntryField::CUSTOM);
    EXPECT_EQ(fe.getExpressions()[2].getCondition()->customField, "is_processed");
    EXPECT_EQ(fe.getExpressions()[2].getCondition()->op, FilterOperator::EQUALS);
    EXPECT_EQ(fe.getExpressions()[2].getCondition()->value, "false");
    EXPECT_EQ(fe.getExpressions()[2].getCondition()->valueType, FilterValueType::BOOL);

    EXPECT_EQ(fe.getExpressions()[3].getCondition()->field, LogEntryField::CUSTOM);
    EXPECT_EQ(fe.getExpressions()[3].getCondition()->customField, "path");
    EXPECT_EQ(fe.getExpressions()[3].getCondition()->op, FilterOperator::REGEX_MATCH);
    EXPECT_EQ(fe.getExpressions()[3].getCondition()->value, "/api/v1/.*");
    EXPECT_EQ(fe.getExpressions()[3].getCondition()->valueType, FilterValueType::STRING);
}

// Test for datetime format (example with a new operator)
TEST_F(FilterJsonTest, FilterExpressionToJson_DateTimeFormat) {
    FilterExpression fe = createFilterCondition(LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-03-15T10:00:00Z",
                                                FilterValueType::DATETIME, false, "%Y-%m-%dT%H:%M:%SZ");
    json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("condition"));
    EXPECT_EQ(j["condition"]["field"], "TIMESTAMP");
    EXPECT_EQ(j["condition"]["op"], "GREATER_THAN");
    EXPECT_EQ(j["condition"]["value"], "2023-03-15T10:00:00Z");
    EXPECT_EQ(j["condition"]["value_type"], "DATETIME");
    EXPECT_EQ(j["condition"]["datetimeFormat"], "%Y-%m-%dT%H:%M:%SZ");
}

TEST_F(FilterJsonTest, FilterExpressionFromJson_DateTimeFormat) {
    json j = {
        {"condition", {
            {"field", "event_time"},
            {"op", "LESS_THAN_OR_EQUAL"},
            {"value", "2024-01-01"},
            {"value_type", "DATETIME"},
            {"datetimeFormat", "%Y-%m-%d"},
            {"customField", "event_time"}
        }}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isCondition());
    EXPECT_EQ(fe.getCondition()->field, LogEntryField::CUSTOM);
    EXPECT_EQ(fe.getCondition()->customField, "event_time");
    EXPECT_EQ(fe.getCondition()->op, FilterOperator::LESS_THAN_OR_EQUAL);
    EXPECT_EQ(fe.getCondition()->value, "2024-01-01");
    EXPECT_EQ(fe.getCondition()->valueType, FilterValueType::DATETIME);
    EXPECT_EQ(fe.getCondition()->datetimeFormat, "%Y-%m-%d");
}

// Test for case sensitivity
TEST_F(FilterJsonTest, FilterExpressionToJson_CaseSensitive) {
    FilterExpression fe = createFilterCondition(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "Case",
                                                FilterValueType::STRING, true); // caseSensitive = true
    json j;
    to_json(j, fe);

    ASSERT_TRUE(j.contains("condition"));
    EXPECT_EQ(j["condition"]["field"], "MESSAGE");
    EXPECT_EQ(j["condition"]["op"], "CONTAINS");
    EXPECT_EQ(j["condition"]["value"], "Case");
    EXPECT_EQ(j["condition"]["value_type"], "STRING");
    EXPECT_TRUE(j["condition"]["caseSensitive"]);
}

TEST_F(FilterJsonTest, FilterExpressionFromJson_CaseSensitive) {
    json j = {
        {"condition", {
            {"field", "message"},
            {"op", "CONTAINS"},
            {"value", "CaseSensitiveValue"},
            {"value_type", "STRING"},
            {"caseSensitive", true}
        }}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isCondition());
    EXPECT_EQ(fe.getCondition()->field, LogEntryField::MESSAGE);
    EXPECT_EQ(fe.getCondition()->op, FilterOperator::CONTAINS);
    EXPECT_EQ(fe.getCondition()->value, "CaseSensitiveValue");
    EXPECT_EQ(fe.getCondition()->valueType, FilterValueType::STRING);
    EXPECT_TRUE(fe.getCondition()->caseSensitive);
}

TEST_F(FilterJsonTest, FilterExpressionFromJson_CaseInsensitiveDefault) {
    json j = {
        {"condition", {
            {"field", "message"},
            {"op", "CONTAINS"},
            {"value", "lowercase"},
            {"value_type", "STRING"}
            // 'case_sensitive' is omitted, should default to false
        }}
    };

    FilterExpression fe;
    auto result = from_json(j, fe);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    ASSERT_TRUE(fe.isCondition());
    EXPECT_EQ(fe.getCondition()->field, LogEntryField::MESSAGE);
    EXPECT_EQ(fe.getCondition()->op, FilterOperator::CONTAINS);
    EXPECT_EQ(fe.getCondition()->value, "lowercase");
    EXPECT_EQ(fe.getCondition()->valueType, FilterValueType::STRING);
    EXPECT_FALSE(fe.getCondition()->caseSensitive); // Should be false by default
}
