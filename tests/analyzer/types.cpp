// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "analyzer/Types.h"
#include <string>

// Test fixture for FormattingOptions tests
class FormattingOptionsTest : public ::testing::Test {
protected:
    FormattingOptions options;
};

// Test default initialization
TEST_F(FormattingOptionsTest, DefaultInitialization) {
    EXPECT_EQ(options.dateTimeFormat, "%Y-%m-%d %H:%M:%S");
    EXPECT_EQ(options.overallFormat, "{timestamp} {level}: {message}"); // Verify new default
    EXPECT_FALSE(options.useColor);
    EXPECT_TRUE(options.includeStructuredFields);
    EXPECT_EQ(options.structuredFieldDelimiter, ", ");
    EXPECT_EQ(options.structuredFieldKvDelimiter, "=");
}

// Test custom initialization using designated initializers
TEST_F(FormattingOptionsTest, DesignatedInitialization) {
    FormattingOptions customOptions{
        .dateTimeFormat = "%H:%M",
        .overallFormat = "{level} - {message}",
        .useColor = true,
        .includeStructuredFields = false,
        .structuredFieldDelimiter = ";",
        .structuredFieldKvDelimiter = ":"
    };

    EXPECT_EQ(customOptions.dateTimeFormat, "%H:%M");
    EXPECT_EQ(customOptions.overallFormat, "{level} - {message}");
    EXPECT_TRUE(customOptions.useColor);
    EXPECT_FALSE(customOptions.includeStructuredFields);
    EXPECT_EQ(customOptions.structuredFieldDelimiter, ";");
    EXPECT_EQ(customOptions.structuredFieldKvDelimiter, ":");
}

// Test partial custom initialization
TEST_F(FormattingOptionsTest, PartialDesignatedInitialization) {
    FormattingOptions customOptions{
        .useColor = true,
        .structuredFieldDelimiter = " | "
    };

    EXPECT_EQ(customOptions.dateTimeFormat, "%Y-%m-%d %H:%M:%S"); // Default
    EXPECT_EQ(customOptions.overallFormat, "{timestamp} {level}: {message}"); // Default
    EXPECT_TRUE(customOptions.useColor); // Custom
    EXPECT_TRUE(customOptions.includeStructuredFields); // Default
    EXPECT_EQ(customOptions.structuredFieldDelimiter, " | "); // Custom
    EXPECT_EQ(customOptions.structuredFieldKvDelimiter, "="); // Default
}

// Test copy constructor
TEST_F(FormattingOptionsTest, CopyConstruction) {
    FormattingOptions originalOptions{
        .dateTimeFormat = "foo",
        .overallFormat = "copy test",
        .useColor = true
    };

    FormattingOptions copiedOptions(originalOptions);

    EXPECT_EQ(copiedOptions.dateTimeFormat, "foo");
    EXPECT_EQ(copiedOptions.overallFormat, "copy test");
    EXPECT_TRUE(copiedOptions.useColor);
    EXPECT_EQ(copiedOptions.structuredFieldDelimiter, ", "); // Default from original's default part
}

// Test copy assignment
TEST_F(FormattingOptionsTest, CopyAssignment) {
    FormattingOptions originalOptions{
        .dateTimeFormat = "bar",
        .overallFormat = "assign test",
        .useColor = false,
        .includeStructuredFields = false
    };

    FormattingOptions assignedOptions;
    assignedOptions = originalOptions;

    EXPECT_EQ(assignedOptions.dateTimeFormat, "bar");
    EXPECT_EQ(assignedOptions.overallFormat, "assign test");
    EXPECT_FALSE(assignedOptions.useColor);
    EXPECT_FALSE(assignedOptions.includeStructuredFields);
}
