// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sstream>
#include <vector>
#include <limits>
#include <chrono>
#include <optional>
#include <map>
#include <regex>
#include <string>
#include <cstddef>
#include <nlohmann/json.hpp>

// Includes from the project
#include "core/log/types.h"
#include "utils/time.h"
#include "utils/core.h"
#include "utils/version.h"
#include "utils/ip_address.h"
#include "export/core.h"
#include "filter/core.h"
#include "filter/expression.h"
#include "filter/enum_string_conversions.h"

// A fixed point in time for deterministic tests
inline std::chrono::system_clock::time_point getFixedTimestamp() {
    return Utils::parseISO8601("2024-01-01T12:00:00Z").value();
}

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
        entry.timestamp = timestamp.value_or(getFixedTimestamp());
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
    filter::FilterCondition createCondition(
        LogEntryField field,
        filter::FilterOperator op,
        const std::string& value,
        filter::FilterValueType valueType = filter::FilterValueType::STRING,
        bool caseSensitive = true,
        std::optional<std::string> customField = std::nullopt
    ) {
        filter::FilterCondition fc;
        fc.field = field;
        fc.op = op;
        fc.value = value;
        fc.valueType = valueType;
        fc.caseSensitive = caseSensitive;
        fc.customField = customField;
        return fc;
    }

    // Helper to create a FilterExpression from a Condition
    filter::FilterExpression createExpr(
        LogEntryField field,
        filter::FilterOperator op,
        const std::string& value,
        filter::FilterValueType valueType = filter::FilterValueType::STRING,
        bool caseSensitive = true,
        std::optional<std::string> customField = std::nullopt,
        std::optional<std::string> datetimeFormat = std::nullopt
    ) {
        auto fc = createCondition(field, op, value, valueType, caseSensitive, customField);
        fc.datetimeFormat = datetimeFormat;
        return filter::FilterExpression::create(fc);
    }

    filter::FilterExpression createVectorExpr(
        LogEntryField field,
        filter::FilterOperator op,
        const std::vector<std::string>& value,
        filter::FilterValueType valueType = filter::FilterValueType::STRING,
        bool caseSensitive = true,
        std::optional<std::string> customField = std::nullopt
    ) {
        filter::FilterCondition fc;
        fc.field = field;
        fc.op = op;
        fc.value = value;
        fc.valueType = valueType;
        fc.caseSensitive = caseSensitive;
        fc.customField = customField;
        return filter::FilterExpression::create(fc);
    }

    // Helper for testing version comparisons
    void testVersionComparison(const std::string& v1, const std::string& v2, filter::FilterOperator op, bool expected) {
        LogEntry entry = this->createLogEntry(LogLevel::INFO, "Version test", "ver.log", {{"version_field", v1}}, 1, getFixedTimestamp());
        filter::FilterCondition cond = createCondition(LogEntryField::CUSTOM, op, v2, filter::FilterValueType::VERSION, true, "version_field");
        filter::FilterExpression expr = filter::FilterExpression::create(cond);
        auto res = expr.evaluate(entry);
        ASSERT_TRUE(res.has_value()) << "Comparison: '" << v1 << "' " << filter::toString(op) << " '" << v2 << "' failed with error: " << res.error().toString();
        EXPECT_EQ(*res, expected) << "Comparison: '" << v1 << "' " << filter::toString(op) << " '" << v2 << "'";
    }

    // Helper for testing IP address comparisons
    void testIpComparison(const std::string& ip1, const std::string& ip2, filter::FilterOperator op, bool expected) {
        LogEntry entry = this->createLogEntry(LogLevel::INFO, "IP test", "ip.log", {{"ip_field", ip1}}, 1, getFixedTimestamp());
        filter::FilterCondition cond = createCondition(LogEntryField::CUSTOM, op, ip2, filter::FilterValueType::IP_ADDRESS, true, "ip_field");
        filter::FilterExpression expr = filter::FilterExpression::create(cond);
        auto res = expr.evaluate(entry);
        ASSERT_TRUE(res.has_value()) << "Comparison: '" << ip1 << "' " << filter::toString(op) << " '" << ip2 << "' failed with error: " << res.error().toString();
        EXPECT_EQ(*res, expected) << "Comparison: '" << ip1 << "' " << filter::toString(op) << " '" << ip2 << "'";
    }
};

// Global helper function to create a LogEntry for tests that don't inherit from FilterTestFixture
inline LogEntry createLogEntry(
    size_t id,
    LogLevel level,
    const std::string& message,
    std::optional<std::chrono::system_clock::time_point> timestamp = std::nullopt,
    const std::map<std::string, std::string>& customFields = {},
    const std::string& sourceFile = "",
    std::optional<unsigned int> sourceLineNumber = std::nullopt
) {
    LogEntry entry;
    entry.id = id;
    entry.level = level;
    entry.message = message;
    entry.timestamp = timestamp.value_or(getFixedTimestamp());
    entry.customFields = customFields;
    entry.sourceFile = sourceFile;
    entry.sourceLineNumber = sourceLineNumber;
    return entry;
}
