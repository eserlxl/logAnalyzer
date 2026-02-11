// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2024 Eser KUBALI

#include <gtest/gtest.h>
#include "filter/core.h"
#include "core/log/types.h"
#include <nlohmann/json.hpp>
#include "../core_json/filter_json_fixture.h"

using namespace filter;

// --- to_json Serialization Tests ---

TEST_F(FilterJsonTest, ToJsonStandardField) {
    auto fcResult = FilterCondition::createDatetime(
        LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-01-01T00:00:00Z", "%Y-%m-%dT%H:%M:%SZ"
    );
    ASSERT_TRUE(fcResult.has_value());
    FilterCondition fc = *fcResult;

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "TIMESTAMP");
    EXPECT_EQ(j["op"], "GREATER_THAN");
    EXPECT_EQ(j["value"].get<std::string>(), "2023-01-01T00:00:00Z");
    EXPECT_EQ(j["value_type"], "DATETIME");
    EXPECT_EQ(j["caseSensitive"], false);
    EXPECT_EQ(j["datetimeFormat"], "%Y-%m-%dT%H:%M:%SZ");
    EXPECT_FALSE(j.contains("customField")); // Should not be present
}

TEST_F(FilterJsonTest, ToJsonCustomField) {
    auto fcResult = FilterCondition::createCustomString("myCustomKey", FilterOperator::EQUALS, "my_custom_value", true);
    ASSERT_TRUE(fcResult.has_value());
    FilterCondition fc = *fcResult;

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "myCustomKey"); // Custom field name is the value of "field"
    EXPECT_EQ(j["op"], "EQUALS");
    EXPECT_EQ(j["value"].get<std::string>(), "my_custom_value");
    EXPECT_EQ(j["value_type"], "STRING");
    EXPECT_EQ(j["caseSensitive"], true);
    EXPECT_FALSE(j.contains("datetimeFormat"));
    EXPECT_FALSE(j.contains("customField")); // Redundant key should NOT be serialized
}
