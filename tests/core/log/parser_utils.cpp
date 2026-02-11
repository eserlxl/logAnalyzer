// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "core/log/parser_utils.h"
#include <regex>

TEST(ParserUtilsTest, ParseStructuredData_LegacyPattern) {
    std::string log = "key1=\"value1\" key2='value2' key3=value3";
    std::map<std::string, std::string> results;
    
    Utils::parseLegacyStructuredData(log, results);
    
    EXPECT_EQ(results["key1"], "value1");
    EXPECT_EQ(results["key2"], "value2");
    EXPECT_EQ(results["key3"], "value3");
}

TEST(ParserUtilsTest, ParseStructuredData_CustomPattern_ExtractsValueFromFifthGroup) {
    // Regex where value is in group 5
    // Pattern: key=(A)|(B)|(C)|(D)
    // Groups: 1=key, 2=A, 3=B, 4=C, 5=D
    // We will match D.
    
    std::string log = "mykey=valD";
    // Regex explanation:
    // Group 1: key
    // Group 2: valA
    // Group 3: valB
    // Group 4: valC
    // Group 5: valD
    std::regex pattern("(\\w+)=(?:(valA)|(valB)|(valC)|(valD))");
    
    std::map<std::string, std::string> results;
    Utils::parseStructuredData(log, results, pattern);
    

    EXPECT_EQ(results["mykey"], "valD");
}

// --- Tests for parseLegacyStructuredData ---

TEST(ParserUtilsTest, ParseLegacyStructuredData_EmptyInput) {
    std::string log;
    std::map<std::string, std::string> results;
    Utils::parseLegacyStructuredData(log, results);
    EXPECT_TRUE(results.empty());
}

TEST(ParserUtilsTest, ParseLegacyStructuredData_NoMatches) {
    std::string log = "just some arbitrary text without key=value pairs";
    std::map<std::string, std::string> results;
    Utils::parseLegacyStructuredData(log, results);
    EXPECT_TRUE(results.empty());
}

TEST(ParserUtilsTest, ParseLegacyStructuredData_MalformedMissingClosingQuotes) {
    std::string log = "key1=\"value1 key2=value2";
    std::map<std::string, std::string> results;
    Utils::parseLegacyStructuredData(log, results);
    // When a quote is missing, the parser extracts what it can.
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results["key1"], ""); // The parser extracts an empty string when the closing quote is missing.
}

TEST(ParserUtilsTest, ParseLegacyStructuredData_MalformedUnmatchedQuotes) {
    std::string log = "key1=value1\"";
    std::map<std::string, std::string> results;
    Utils::parseLegacyStructuredData(log, results);
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results["key1"], "value1\"");
}

TEST(ParserUtilsTest, ParseLegacyStructuredData_EmptyKey) {
    std::string log = "=value1 key2=value2";
    std::map<std::string, std::string> results;
    Utils::parseLegacyStructuredData(log, results);
    // Assuming empty keys are ignored.
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results.count(""), 0);
    EXPECT_EQ(results["key2"], "value2");
}

TEST(ParserUtilsTest, ParseLegacyStructuredData_EmptyValue) {
    std::string log = "key1= key2=value2";
    std::map<std::string, std::string> results;
    Utils::parseLegacyStructuredData(log, results);
    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(results["key1"], "");
    EXPECT_EQ(results["key2"], "value2");
}

TEST(ParserUtilsTest, ParseLegacyStructuredData_EmptyQuotedValue) {
    std::string log = "key1=\"\" key2=''";
    std::map<std::string, std::string> results;
    Utils::parseLegacyStructuredData(log, results);
    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(results["key1"], "");
    EXPECT_EQ(results["key2"], "");
}

TEST(ParserUtilsTest, ParseLegacyStructuredData_ValueWithSpaces) {
    std::string log = "key1=\"value with spaces\"";
    std::map<std::string, std::string> results;
    Utils::parseLegacyStructuredData(log, results);
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results["key1"], "value with spaces");
}

TEST(ParserUtilsTest, ParseLegacyStructuredData_ValueWithSpecialChars) {
    std::string log = "key1=\"value!@#$%^&*()\"";
    std::map<std::string, std::string> results;
    Utils::parseLegacyStructuredData(log, results);
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results["key1"], "value!@#$%^&*()");
}

TEST(ParserUtilsTest, ParseLegacyStructuredData_KeyWithSpecialChars) {
    std::string log = "k_e.y-1=\"value\"";
    std::map<std::string, std::string> results;
    Utils::parseLegacyStructuredData(log, results);
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results["k_e.y-1"], "value");
}

TEST(ParserUtilsTest, ParseLegacyStructuredData_OverlappingKeys) {
    std::string log = "key1=v1 key1=v2";
    std::map<std::string, std::string> results;
    Utils::parseLegacyStructuredData(log, results);
    // Assuming the last value is retained for overlapping keys.
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results["key1"], "v2");
}

TEST(ParserUtilsTest, ParseLegacyStructuredData_MixedQuotes) {
    std::string log = "key1=\"value1\" key2='value2'";
    std::map<std::string, std::string> results;
    Utils::parseLegacyStructuredData(log, results);
    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(results["key1"], "value1");
    EXPECT_EQ(results["key2"], "value2");
}

// --- Tests for parseStructuredData ---

TEST(ParserUtilsTest, ParseStructuredData_EmptyLogInput) {
    std::string log;
    std::regex pattern("(\\w+)=(\\w+)");
    std::map<std::string, std::string> results;
    Utils::parseStructuredData(log, results, pattern);
    EXPECT_TRUE(results.empty());
}

TEST(ParserUtilsTest, ParseStructuredData_EmptyRegex) {
    std::string log = "key=value";
    std::regex pattern("");
    std::map<std::string, std::string> results;
    Utils::parseStructuredData(log, results, pattern);
    // This should not match anything.
    EXPECT_TRUE(results.empty());
}

TEST(ParserUtilsTest, ParseStructuredData_NoMatches) {
    std::string log = "this does not match";
    std::regex pattern("(\\w+)=(\\w+)");
    std::map<std::string, std::string> results;
    Utils::parseStructuredData(log, results, pattern);
    EXPECT_TRUE(results.empty());
}

TEST(ParserUtilsTest, ParseStructuredData_ExtractsValueFromSecondGroup) {
    // Probes the group extraction logic. Here value is in group 2.
    std::string log = "key=value";
    std::regex pattern("(\\w+)=(\\w+)");
    std::map<std::string, std::string> results;
    Utils::parseStructuredData(log, results, pattern);
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results["key"], "value");
}

TEST(ParserUtilsTest, ParseStructuredData_HandlesMultipleOptionalValueGroups) {
    // This test confirms that the last non-empty capturing group is used as the value.
    std::string log = "key=valB";
    // Groups: 1=key, 2=valA, 3=valB, 4=valC
    std::regex pattern("(\\w+)=(?:(valA)|(valB)|(valC))");
    std::map<std::string, std::string> results;
    Utils::parseStructuredData(log, results, pattern);
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results["key"], "valB");
}

TEST(ParserUtilsTest, ParseStructuredData_NoValueCaptureGroup) {
    // Regex only captures the key, not the value.
    std::string log = "key=value_literal";
    std::regex pattern("(\\w+)=value_literal");
    std::map<std::string, std::string> results;
    Utils::parseStructuredData(log, results, pattern);
    // Assumes that if there's only one capture group, no key-value pair is extracted.
    EXPECT_TRUE(results.empty());
}

TEST(ParserUtilsTest, ParseStructuredData_MultipleMatches) {
    // Checks if the parser finds all key-value pairs in the log string.
    std::string log = "key1=val1 key2=val2";
    std::regex pattern("(\\w+)=(\\w+)");
    std::map<std::string, std::string> results;
    Utils::parseStructuredData(log, results, pattern);
    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(results["key1"], "val1");
    EXPECT_EQ(results["key2"], "val2");
}

TEST(ParserUtilsTest, ParseStructuredData_ComplexPattern) {
    // [timestamp] key="value"
    std::string log = "[2023-01-01T12:00:00Z] user_id=\"-123.45e6\"";
    std::regex pattern("\\[[^\\]]+\\]\\s*(\\w+)=\"([^\"]+)\"");
    std::map<std::string, std::string> results;
    Utils::parseStructuredData(log, results, pattern);
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results["user_id"], "-123.45e6");
}

TEST(ParserUtilsTest, ParseStructuredData_InvalidRegexSyntax) {
    // An invalid regex with an unclosed parenthesis should throw at construction.
    // This test ensures that client code using the parser is aware of regex syntax errors.
    EXPECT_THROW({
        std::regex pattern("(\\w+=(\\w+");
    }, std::regex_error);
}
