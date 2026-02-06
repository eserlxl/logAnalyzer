// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "filter/Core.h"
#include "core/LogTypes.h"
#include "filter/Expression.h"
#include "utils/Version.h"
#include "utils/IpAddress.h"
#include "utils/Time.h"
#include <chrono>
#include <map>

class FilterIteration2Test : public ::testing::Test {
protected:
    LogEntry createEntry(const std::string& msg, const std::map<std::string, std::string>& custom = {}) {
        LogEntry entry;
        entry.level = LogLevel::INFO;
        entry.message = msg;
        entry.customFields = custom;
        entry.timestamp = std::chrono::system_clock::now();
        entry.sourceFile = "test.log";
        return entry;
    }
};

// 1. AUTO type inference tests
TEST_F(FilterIteration2Test, AutoTypeInference) {
    LogEntry entry = createEntry("test message", {{"version", "1.2.3"}, {"ip", "192.168.1.1"}, {"count", "42"}, {"price", "19.99"}, {"active", "true"}});

    // Version
    auto condVer = FilterCondition::createCustomTyped("version", FilterOperator::GREATER_THAN, "1.0.0", FilterValueType::AUTO).value();
    EXPECT_TRUE(FilterExpression(condVer).evaluate(entry).value());

    // IP Address
    auto condIp = FilterCondition::createCustomTyped("ip", FilterOperator::EQUALS, "192.168.1.1", FilterValueType::AUTO).value();
    EXPECT_TRUE(FilterExpression(condIp).evaluate(entry).value());

    // INT
    auto condInt = FilterCondition::createCustomTyped("count", FilterOperator::GREATER_THAN, "40", FilterValueType::AUTO).value();
    EXPECT_TRUE(FilterExpression(condInt).evaluate(entry).value());

    // DOUBLE
    auto condDouble = FilterCondition::createCustomTyped("price", FilterOperator::LESS_THAN, "20.0", FilterValueType::AUTO).value();
    EXPECT_TRUE(FilterExpression(condDouble).evaluate(entry).value());

    // BOOL
    auto condBool = FilterCondition::createCustomTyped("active", FilterOperator::EQUALS, "true", FilterValueType::AUTO).value();
    EXPECT_TRUE(FilterExpression(condBool).evaluate(entry).value());
}

// 2. IN/NOT_IN typed array tests
TEST_F(FilterIteration2Test, InNotInTypedArrays) {
    LogEntry entry = createEntry("msg", {{"val", "42"}, {"flag", "false"}});

    // INT array
    auto condIntIn = FilterCondition::createCustomTyped("CUSTOM", FilterOperator::IN, "[40, 41, 42, 43]", FilterValueType::INT).value();
    condIntIn.customField = "val";
    EXPECT_TRUE(FilterExpression(condIntIn).evaluate(entry).value());

    auto condIntNotIn = FilterCondition::createCustomTyped("CUSTOM", FilterOperator::NOT_IN, "[1, 2, 3]", FilterValueType::INT).value();
    condIntNotIn.customField = "val";
    EXPECT_TRUE(FilterExpression(condIntNotIn).evaluate(entry).value());

    // BOOL array
    auto condBoolIn = FilterCondition::createCustomTyped("CUSTOM", FilterOperator::IN, "[false, 0, \"no\"]", FilterValueType::BOOL).value();
    condBoolIn.customField = "flag";
    EXPECT_TRUE(FilterExpression(condBoolIn).evaluate(entry).value());
}

// 3. Error propagation tests
TEST_F(FilterIteration2Test, ErrorPropagation) {
    LogEntry entry = createEntry("msg", {{"bad_int", "not_a_number"}});

    auto cond = FilterCondition::createCustomTyped("CUSTOM", FilterOperator::EQUALS, "42", FilterValueType::INT).value();
    cond.customField = "bad_int";

    auto result = FilterExpression(cond).evaluate(entry);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, Code::ConversionError);
}

// 4. DottedKey filter tests (renamed from CustomField*)
TEST_F(FilterIteration2Test, DottedKeyFilters) {
    LogEntry entry = createEntry("msg", {{"a.b.c", "target"}});

    DottedKeyFieldValueFilter filter("a.b.c", "target");
    EXPECT_TRUE(filter.matches(entry));

    DottedKeyFieldValueFilter filterFail("a.b.c", "other");
    EXPECT_FALSE(filterFail.matches(entry));
}

// 5. SourceFileFilter glob matching tests (Anchored)
TEST_F(FilterIteration2Test, SourceFileFilterGlobAnchored) {
    LogEntry entry;
    entry.sourceFile = "server.log";

    SourceFileFilter filter1("server*", PatternType::Wildcard);
    EXPECT_TRUE(filter1.matches(entry));

    SourceFileFilter filter2("*.log", PatternType::Wildcard);
    EXPECT_TRUE(filter2.matches(entry));

    entry.sourceFile = "my-server.log";
    EXPECT_FALSE(filter1.matches(entry)); // ^server.*$ does not match my-server.log
    
    SourceFileFilter filter3("*server*", PatternType::Wildcard);
    EXPECT_TRUE(filter3.matches(entry));
}
