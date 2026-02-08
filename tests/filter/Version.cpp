// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "Utils.h"
#include "filter/Expression.h" // Added
#include "filter/Condition.h" // Added
#include "filter/Types.h" // Added
#include "filter/ConcreteFilters.h" // Added for filter types used in createExpr
#include "core/Log/Types.h" // Added for LogEntry and LogLevel

using namespace filter;

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
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "1.0.0-rc.1", FilterValueType::VERSION, true, "app_version").evaluate(createLogEntry(LogLevel::INFO, "", "ver.log", {{"app_version", "1.0.0"}})).value_or(false)); // Prerelease < no prerelease
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

TEST_F(FilterTestFixture, EvaluateVersionRejectsInvalidSemverForms) {
    auto checkInvalid = [&](const std::string& rawVersion) {
        LogEntry entry = createLogEntry(LogLevel::INFO, "Version test", "ver.log", {{"version_field", rawVersion}});
        FilterCondition cond = createCondition(
            LogEntryField::CUSTOM, FilterOperator::EQUALS, "1.0.0",
            FilterValueType::VERSION, true, "version_field");
        FilterExpression expr = FilterExpression::create(cond);
        EXPECT_FALSE(expr.evaluate(entry).has_value()) << rawVersion;
    };

    checkInvalid("01.2.3");          // Leading zero in major
    checkInvalid("1.02.3");          // Leading zero in minor
    checkInvalid("1.2.03");          // Leading zero in patch
    checkInvalid("1.2.3-alpha..1");  // Empty prerelease identifier
    checkInvalid("1.2.3-alpha.");    // Trailing prerelease separator
    checkInvalid("1.2.3+build..7");  // Empty build identifier
    checkInvalid("1.2.3+build.");    // Trailing build separator
    checkInvalid("1.2.3-ä");         // Non-ASCII identifier char
}

TEST_F(FilterTestFixture, EvaluateVersionComparesLargeNumericPrereleaseIdentifiersSafely) {
    // SemVer numeric prerelease identifiers compare numerically even when they exceed
    // native integer widths.
    auto huge = createLogEntry(
        LogLevel::INFO, "Version huge", "ver.log",
        {{"app_version", "1.0.0-18446744073709551616"}});

    auto tiny = createLogEntry(
        LogLevel::INFO, "Version tiny", "ver.log",
        {{"app_version", "1.0.0-2"}});

    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "1.0.0-2",
                           FilterValueType::VERSION, true, "app_version")
                    .evaluate(huge)
                    .value_or(false));

    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN, "1.0.0-18446744073709551616",
                           FilterValueType::VERSION, true, "app_version")
                    .evaluate(tiny)
                    .value_or(false));
}
