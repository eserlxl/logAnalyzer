// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "TestUtils.h"

// --- Tests for VERSION type ---

TEST_F(FilterTestFixture, EvaluateVersionComparison) {
    auto entry_v100 = createLogEntry(LogLevel::INFO, "Version 1.0.0", "ver.log", {{"app_version", "1.0.0"}});
    auto entry_v110 = createLogEntry(LogLevel::INFO, "Version 1.1.0", "ver.log", {{"app_version", "1.1.0"}});
    auto entry_v110_patch = createLogEntry(LogLevel::INFO, "Version 1.1.0-patch", "ver.log", {{"app_version", "1.1.0-patch"}});
    auto entry_v200 = createLogEntry(LogLevel::INFO, "Version 2.0.0", "ver.log", {{"app_version", "2.0.0"}});
    auto entry_v_invalid = createLogEntry(LogLevel::INFO, "Invalid version", "ver.log", {{"app_version", "invalid-version"}});

    // EQUALS
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "1.0.0", FilterValueType::VERSION, true, "app_version").evaluate(entry_v100).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "1.1.0", FilterValueType::VERSION, true, "app_version").evaluate(entry_v100).value_or(true));

    // GREATER_THAN
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "1.0.0", FilterValueType::VERSION, true, "app_version").evaluate(entry_v110).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "1.1.0-patch", FilterValueType::VERSION, true, "app_version").evaluate(entry_v110).value_or(false)); // 1.1.0 is greater than 1.1.0-patch
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "1.1.0", FilterValueType::VERSION, true, "app_version").evaluate(entry_v200).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "2.0.0", FilterValueType::VERSION, true, "app_version").evaluate(entry_v200).value_or(true));

    // LESS_THAN_OR_EQUAL
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "1.1.0", FilterValueType::VERSION, true, "app_version").evaluate(entry_v100).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "1.1.0-patch", FilterValueType::VERSION, true, "app_version").evaluate(entry_v110_patch).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "2.0.0", FilterValueType::VERSION, true, "app_version").evaluate(entry_v200).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "1.0.0", FilterValueType::VERSION, true, "app_version").evaluate(entry_v110).value_or(true));

    // Test with invalid version strings (should fail comparison)
    ASSERT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "1.0.0", FilterValueType::VERSION, true, "app_version").evaluate(entry_v_invalid).has_value());
    ASSERT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "invalid-version", FilterValueType::VERSION, true, "app_version").evaluate(entry_v100).has_value());
}

TEST_F(FilterTestFixture, EvaluateVersionEquals) {
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "1.0.0", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0"}})).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "1.0.1", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0"}})).value_or(true));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "1.0.0-alpha", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0-alpha"}})).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "1.0.0-beta", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0-alpha"}})).value_or(true));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "1.0.0+build123", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0+build123"}})).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "1.0.0+build456", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0+build123"}})).value_or(true)); // Build metadata is considered for strict equality
}

TEST_F(FilterTestFixture, EvaluateVersionNotEquals) {
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::NOT_EQUALS, "1.0.1", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0"}})).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::NOT_EQUALS, "1.0.0", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0"}})).value_or(true));
}

TEST_F(FilterTestFixture, EvaluateVersionGreaterThan) {
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "1.0.0", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.1"}})).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "1.0.0-alpha", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0-beta"}})).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "1.0.0-rc.1", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0"}})).value_or(false)); // Prerelease < no prerelease
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "1.0.0", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0"}})).value_or(true));
}

TEST_F(FilterTestFixture, EvaluateVersionLessThan) {
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN, "1.0.1", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0"}})).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN, "1.0.0-beta", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0-alpha"}})).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN, "1.0.0", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0-rc.1"}})).value_or(false)); // Prerelease < no prerelease
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN, "1.0.0", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0"}})).value_or(true));
}

TEST_F(FilterTestFixture, EvaluateVersionGreaterThanOrEqual) {
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN_OR_EQUAL, "1.0.0", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0"}})).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN_OR_EQUAL, "1.0.0", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.1"}})).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN_OR_EQUAL, "1.0.1", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0"}})).value_or(true));
}

TEST_F(FilterTestFixture, EvaluateVersionLessThanOrEqual) {
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "1.0.0", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0"}})).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "1.0.1", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0"}})).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "1.0.0", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.1"}})).value_or(true));
}

TEST_F(FilterTestFixture, EvaluateVersionPrereleasePrecedence) {
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN, "1.0.0-alpha.1", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0-alpha"}})).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "1.0.0-alpha", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0-beta"}})).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "1.0.0-beta.2", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0-rc.1"}})).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "1.0.0-alpha.0.0", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0-alpha.0.0.1"}})).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN, "1.0.0-alpha.0.0.1", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0-alpha.0.0"}})).value_or(false));
}
TEST_F(FilterTestFixture, EvaluateVersionInvalidInput) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "Version test", "ver.log", {{"version_field", "not-a-version"}});
    FilterCondition cond = createCondition(LogEntryField::CUSTOM, FilterOperator::EQUALS, "1.0.0", FilterValueType::VERSION, true, "version_field");
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry).has_value());

    LogEntry entry2 = createLogEntry(LogLevel::INFO, "Version test", "ver.log", {{"version_field", "1.0.0"}});
    cond = createCondition(LogEntryField::CUSTOM, FilterOperator::EQUALS, "not-a-version", FilterValueType::VERSION, true, "version_field");
    expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry2).has_value());
}
