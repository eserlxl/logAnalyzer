// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2024 Eser KUBALI

#include <gtest/gtest.h>
#include "filter/condition.h"
#include "filter/types.h"
#include "core/log/types.h"
#include <nlohmann/json.hpp>
#include <optional>
#include "filterjsonfixture.h"

using namespace filter;

// FilterCondition JSON tests
TEST_F(FilterJsonTest, FilterConditionToJson) {
    FilterCondition fc = FilterCondition::createDatetime(
        LogEntryField::TIMESTAMP, FilterOperator::GREATER_THAN, "2023-01-01T00:00:00Z", "%Y-%m-%dT%H:%M:%SZ"
    ).value();

    nlohmann::json j;
    to_json(j, fc);

    EXPECT_EQ(j["field"], "TIMESTAMP");
    EXPECT_EQ(j["op"], "GREATER_THAN");
    EXPECT_EQ(j["value"].get<std::string>(), "2023-01-01T00:00:00Z");
    EXPECT_EQ(j["value_type"], "DATETIME");
    EXPECT_EQ(j["caseSensitive"], false);
    EXPECT_EQ(j["datetimeFormat"], "%Y-%m-%dT%H:%M:%SZ");
}
