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
