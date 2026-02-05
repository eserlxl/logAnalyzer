// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "tests/filter/TestUtils.h"

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
    testVersionComparison("1.0.0", "1.0.0", FilterOperator::EQUALS, true);
    testVersionComparison("1.0.0", "1.0.1", FilterOperator::EQUALS, false);
    testVersionComparison("1.0.0-alpha", "1.0.0-alpha", FilterOperator::EQUALS, true);
    testVersionComparison("1.0.0-alpha", "1.0.0-beta", FilterOperator::EQUALS, false);
    testVersionComparison("1.0.0+build123", "1.0.0+build123", FilterOperator::EQUALS, true);
    testVersionComparison("1.0.0+build123", "1.0.0+build456", FilterOperator::EQUALS, false); // Build metadata is considered for strict equality
}

TEST_F(FilterTestFixture, EvaluateVersionNotEquals) {
    testVersionComparison("1.0.0", "1.0.1", FilterOperator::NOT_EQUALS, true);
    testVersionComparison("1.0.0", "1.0.0", FilterOperator::NOT_EQUALS, false);
}

TEST_F(FilterTestFixture, EvaluateVersionGreaterThan) {
    testVersionComparison("1.0.1", "1.0.0", FilterOperator::GREATER_THAN, true);
    testVersionComparison("1.1.0", "1.0.0", FilterOperator::GREATER_THAN, true);
    testVersionComparison("2.0.0", "1.0.0", FilterOperator::GREATER_THAN, true);
    testVersionComparison("1.0.0", "1.0.0", FilterOperator::GREATER_THAN, false);
    testVersionComparison("1.0.0-beta", "1.0.0-alpha", FilterOperator::GREATER_THAN, true);
    testVersionComparison("1.0.0", "1.0.0-rc.1", FilterOperator::GREATER_THAN, true); // No prerelease > prerelease
}

TEST_F(FilterTestFixture, EvaluateVersionLessThan) {
    testVersionComparison("1.0.0", "1.0.1", FilterOperator::LESS_THAN, true);
    testVersionComparison("1.0.0-alpha", "1.0.0-beta", FilterOperator::LESS_THAN, true);
    testVersionComparison("1.0.0-rc.1", "1.0.0", FilterOperator::LESS_THAN, true); // Prerelease < no prerelease
    testVersionComparison("1.0.0", "1.0.0", FilterOperator::LESS_THAN, false);
}

TEST_F(FilterTestFixture, EvaluateVersionGreaterThanOrEqual) {
    testVersionComparison("1.0.0", "1.0.0", FilterOperator::GREATER_THAN_OR_EQUAL, true);
    testVersionComparison("1.0.1", "1.0.0", FilterOperator::GREATER_THAN_OR_EQUAL, true);
    testVersionComparison("1.0.0", "1.0.1", FilterOperator::GREATER_THAN_OR_EQUAL, false);
}

TEST_F(FilterTestFixture, EvaluateVersionLessThanOrEqual) {
    testVersionComparison("1.0.0", "1.0.0", FilterOperator::LESS_THAN_OR_EQUAL, true);
    testVersionComparison("1.0.0", "1.0.1", FilterOperator::LESS_THAN_OR_EQUAL, true);
    testVersionComparison("1.0.1", "1.0.0", FilterOperator::LESS_THAN_OR_EQUAL, false);
}

TEST_F(FilterTestFixture, EvaluateVersionPrereleasePrecedence) {
    testVersionComparison("1.0.0-alpha.1", "1.0.0-alpha", FilterOperator::LESS_THAN, false); 
    testVersionComparison("1.0.0-alpha", "1.0.0-alpha.1", FilterOperator::LESS_THAN, true);
    testVersionComparison("1.0.0-beta", "1.0.0-alpha", FilterOperator::GREATER_THAN, true);
    testVersionComparison("1.0.0-rc.1", "1.0.0-beta.2", FilterOperator::GREATER_THAN, true);
    testVersionComparison("1.0.0-alpha.0.0.1", "1.0.0-alpha.0.0", FilterOperator::GREATER_THAN, true);
    testVersionComparison("1.0.0-alpha.0.0", "1.0.0-alpha.0.0.1", FilterOperator::LESS_THAN, true);

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
