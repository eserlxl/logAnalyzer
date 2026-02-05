// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include <gtest/gtest.h>
#include "filter/Core.h"
#include "core/LogTypes.h"
#include "filter/Expression.h"
#include "filter/EnumStringConversions.h"
#include "utils/Version.h"
#include "utils/IpAddress.h"
#include "utils/Time.h"
#include <chrono>
#include <map>
#include <optional>
#include <regex>
#include <nlohmann/json.hpp>

// Consolidated test fixture for FilterExpression tests
class FilterTestFixture : public ::testing::Test {
protected:
    size_t nextId = 1;

    // Helper to create a LogEntry with common fields
    LogEntry createLogEntry(
        LogLevel level,
        const std::string& message,
        const std::string& sourceFile = "test.log",
        const std::map<std::string, std::string>& customFields = {},
        std::optional<size_t> id = std::nullopt,
        std::optional<std::chrono::system_clock::time_point> timestamp = std::nullopt,
        std::optional<unsigned int> sourceLineNumber = std::nullopt,
        std::optional<std::string> threadId = std::nullopt,
        std::optional<std::string> module = std::nullopt,
        std::optional<std::string> host = std::nullopt
    ) {
        LogEntry entry;
        entry.id = id.value_or(nextId++);
        entry.sourceFile = sourceFile;
        entry.timestamp = timestamp.value_or(std::chrono::system_clock::now());
        entry.level = level;
        entry.message = message;
        entry.customFields = customFields;
        entry.sourceLineNumber = sourceLineNumber;
        entry.threadId = threadId;
        entry.module = module;
        entry.host = host;
        return entry;
    }

    // Helper to create a FilterCondition
    FilterCondition createCondition(
        LogEntryField field,
        FilterOperator op,
        const std::string& value,
        FilterValueType valueType = FilterValueType::STRING,
        bool caseSensitive = true,
        std::optional<std::string> customField = std::nullopt
    ) {
        FilterCondition fc;
        fc.field = field;
        fc.op = op;
        fc.value = value;
        fc.valueType = valueType;
        fc.caseSensitive = caseSensitive;
        fc.customField = customField;
        return fc;
    }

    // Helper to create a FilterExpression from a Condition
    FilterExpression createExpr(
        LogEntryField field,
        FilterOperator op,
        const std::string& value,
        FilterValueType valueType = FilterValueType::STRING,
        bool caseSensitive = true,
        std::optional<std::string> customField = std::nullopt
    ) {
        return FilterExpression::create(createCondition(field, op, value, valueType, caseSensitive, customField));
    }

    // Helper for testing version comparisons
    void testVersionComparison(const std::string& v1, const std::string& v2, FilterOperator op, bool expected) {
        LogEntry entry = createLogEntry(LogLevel::INFO, "Version test", "ver.log", {{"version_field", v1}});
        FilterCondition cond = createCondition(LogEntryField::CUSTOM, op, v2, FilterValueType::VERSION, true, "version_field");
        FilterExpression expr = FilterExpression::create(cond);
        auto res = expr.evaluate(entry);
        ASSERT_TRUE(res.has_value()) << "Comparison: '" << v1 << "' " << toString(op) << " '" << v2 << "' failed with error: " << res.error().toString();
        EXPECT_EQ(*res, expected) << "Comparison: '" << v1 << "' " << toString(op) << " '" << v2 << "'";
    }

    // Helper for testing IP address comparisons
    void testIpComparison(const std::string& ip1, const std::string& ip2, FilterOperator op, bool expected) {
        LogEntry entry = createLogEntry(LogLevel::INFO, "IP test", "ip.log", {{"ip_field", ip1}});
        FilterCondition cond = createCondition(LogEntryField::CUSTOM, op, ip2, FilterValueType::IP_ADDRESS, true, "ip_field");
        FilterExpression expr = FilterExpression::create(cond);
        auto res = expr.evaluate(entry);
        ASSERT_TRUE(res.has_value()) << "Comparison: '" << ip1 << "' " << toString(op) << " '" << ip2 << "' failed with error: " << res.error().toString();
        EXPECT_EQ(*res, expected) << "Comparison: '" << ip1 << "' " << toString(op) << " '" << ip2 << "'";
    }
};
