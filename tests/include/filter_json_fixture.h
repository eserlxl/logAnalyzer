// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2024 Eser KUBALI

#ifndef LOGANALYZER_FILTER_JSON_TEST_FIXTURE_H
#define LOGANALYZER_FILTER_JSON_TEST_FIXTURE_H

#include <gtest/gtest.h>
#include "filter/core.h"
#include "filter/condition.h"
#include "filter/types.h"
#include <optional>
#include <string>

using namespace filter;

// Test fixture for JSON serialization/deserialization tests
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

#endif // LOGANALYZER_FILTER_JSON_TEST_FIXTURE_H
