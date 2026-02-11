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

    // This overload was causing issues because it was hiding the more comprehensive one
    // when called with specific arguments. Renaming it to clarify its purpose
    // and make it distinct from the comprehensive helper above.
    // It is primarily used by FilterTestFixture member functions below.
    LogEntry createLogEntryInternal(
        size_t id,
        const std::string& sourceFile,
        const std::chrono::system_clock::time_point& timestamp,
        LogLevel level,
        const std::string& message,
        const std::map<std::string, std::string>& customFields = {}
    ) {
        LogEntry entry;
        entry.id = id;
        entry.sourceFile = sourceFile;
        entry.timestamp = timestamp;
        entry.level = level;
        entry.message = message;
        entry.customFields = customFields;
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
        LogEntry entry = createLogEntryInternal(1, "ver.log", std::chrono::system_clock::now(), LogLevel::INFO, "Version test", {{"version_field", v1}});
        filter::FilterCondition cond = createCondition(LogEntryField::CUSTOM, op, v2, filter::FilterValueType::VERSION, true, "version_field");
        filter::FilterExpression expr = filter::FilterExpression::create(cond);
        auto res = expr.evaluate(entry);
        ASSERT_TRUE(res.has_value()) << "Comparison: '" << v1 << "' " << filter::toString(op) << " '" << v2 << "' failed with error: " << res.error().toString();
        EXPECT_EQ(*res, expected) << "Comparison: '" << v1 << "' " << filter::toString(op) << " '" << v2 << "'";
    }

    // Helper for testing IP address comparisons
    void testIpComparison(const std::string& ip1, const std::string& ip2, filter::FilterOperator op, bool expected) {
        LogEntry entry = createLogEntryInternal(1, "ip.log", std::chrono::system_clock::now(), LogLevel::INFO, "IP test", {{"ip_field", ip1}});
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
    const std::string& sourceFile,
    const std::chrono::system_clock::time_point& timestamp,
    LogLevel level,
    const std::string& message,
    const std::map<std::string, std::string>& customFields = {}
) {
    LogEntry entry;
    entry.id = id;
    entry.sourceFile = sourceFile;
    entry.timestamp = timestamp;
    entry.level = level;
    entry.message = message;
    entry.customFields = customFields;
    return entry;
}

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
    entry.timestamp = timestamp;
    for (const auto& field : customFields) {
        entry.customFields[field.first] = field.second;
    }
    entry.sourceFile = sourceFile;
    entry.sourceLineNumber = sourceLineNumber;
    return entry;
}
