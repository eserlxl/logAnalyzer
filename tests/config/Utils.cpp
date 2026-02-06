// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "config/Utils.h"
#include <variant>

TEST(ConfigUtilsTest, ParseFieldMappingString_ValidKnownField) {
    auto result = ConfigUtils::parseFieldMappingString("1=timestamp:%Y-%m-%d");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->groupIndex.value_or(0), 1u);
    ASSERT_TRUE(std::holds_alternative<LogEntryField>(result->field));
    EXPECT_EQ(std::get<LogEntryField>(result->field), LogEntryField::TIMESTAMP);
    ASSERT_EQ(result->formats.size(), 1u);
    EXPECT_EQ(result->formats[0], "%Y-%m-%d");
}

TEST(ConfigUtilsTest, ParseFieldMappingString_ValidKnownField_NoFormat) {
    auto result = ConfigUtils::parseFieldMappingString("2=level");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->groupIndex.value_or(0), 2u);
    ASSERT_TRUE(std::holds_alternative<LogEntryField>(result->field));
    EXPECT_EQ(std::get<LogEntryField>(result->field), LogEntryField::LEVEL);
    EXPECT_TRUE(result->formats.empty());
}

TEST(ConfigUtilsTest, ParseFieldMappingString_ValidCustomField) {
    auto result = ConfigUtils::parseFieldMappingString("3=my_custom_field");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->groupIndex.value_or(0), 3u);
    
    // ConfigUtils::parseFieldMappingString implementation checks if stringToLogEntryField returns UNKNOWN.
    // If unknown, it stores it as a custom field name (string) in the variant, 
    // OR it might store it as LogEntryField::UNKNOWN if the string was literally "unknown"? 
    // Looking at src/config/Utils.cpp logic:
    // LogEntryField field = Utils::stringToLogEntryField(fieldName);
    // if (field != LogEntryField::UNKNOWN) { ... } else { return FieldMapping(fieldName ...); }
    // So it should be a string in the variant.
    
    ASSERT_TRUE(std::holds_alternative<std::string>(result->field));
    EXPECT_EQ(std::get<std::string>(result->field), "my_custom_field");
    EXPECT_TRUE(result->formats.empty());
}

TEST(ConfigUtilsTest, ParseFieldMappingString_ValidCustomField_WithFormat) {
    auto result = ConfigUtils::parseFieldMappingString("4=my_field:some_format");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->groupIndex.value_or(0), 4u);
    ASSERT_TRUE(std::holds_alternative<std::string>(result->field));
    EXPECT_EQ(std::get<std::string>(result->field), "my_field");
    ASSERT_EQ(result->formats.size(), 1u);
    EXPECT_EQ(result->formats[0], "some_format");
}

TEST(ConfigUtilsTest, ParseFieldMappingString_InvalidFormat_NoEquals) {
    auto result = ConfigUtils::parseFieldMappingString("invalid");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ::Code::InvalidArgument);
}

TEST(ConfigUtilsTest, ParseFieldMappingString_InvalidFormat_NonNumericIndex) {
    auto result = ConfigUtils::parseFieldMappingString("a=timestamp");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ::Code::InvalidArgument);
}

TEST(ConfigUtilsTest, ParseFieldMappingString_InvalidFormat_EmptyField) {
    auto result = ConfigUtils::parseFieldMappingString("1=");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ::Code::InvalidArgument);
}

TEST(ConfigUtilsTest, ParseFieldMappingStrings_MultipleValid) {
    std::vector<std::string> inputs = {"1=timestamp", "2=level"};
    auto result = ConfigUtils::parseFieldMappingStrings(inputs);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 2u);
    
    EXPECT_EQ(std::get<LogEntryField>(result->at(0).field), LogEntryField::TIMESTAMP);
    EXPECT_EQ(std::get<LogEntryField>(result->at(1).field), LogEntryField::LEVEL);
}

TEST(ConfigUtilsTest, ParseFieldMappingStrings_OneInvalid) {
    std::vector<std::string> inputs = {"1=timestamp", "invalid"};
    auto result = ConfigUtils::parseFieldMappingStrings(inputs);
    EXPECT_FALSE(result.has_value());
}
