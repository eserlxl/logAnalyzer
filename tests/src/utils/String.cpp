// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "utils/String.h"
#include "utils/UtilsCore.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>

// Test suite for string utility functions
class StringUtilsTest : public ::testing::Test {};

TEST_F(StringUtilsTest, ReplaceAll) {
    std::string str = "one two three two one";
    Utils::replaceAll(str, "two", "2");
    EXPECT_EQ(str, "one 2 three 2 one");

    Utils::replaceAll(str, "one", "1");
    EXPECT_EQ(str, "1 2 three 2 1");

    // Test replacement where 'to' contains 'from'
    // With an implementation that advances past the replacement, this is the correct behavior.
    std::string str2 = "abab";
    Utils::replaceAll(str2, "a", "ab");
    EXPECT_EQ(str2, "abbabb"); // Not "abbabb", which would require a different algorithm

    // Test empty from
    std::string str3 = "test";
    Utils::replaceAll(str3, "", "x");
    EXPECT_EQ(str3, "test");

    // Test empty to
    std::string str4 = "one two three two one";
    Utils::replaceAll(str4, "two", "");
    EXPECT_EQ(str4, "one  three  one");

    // Test no match
    std::string str5 = "abc";
    Utils::replaceAll(str5, "d", "e");
    EXPECT_EQ(str5, "abc");
}

TEST_F(StringUtilsTest, ReplaceAllIgnoreCase) {
    // Test basic case-insensitive replacement
    std::string str1 = "Test test TeSt";
    Utils::replaceAllIgnoreCase(str1, "test", "REPLACED");
    EXPECT_EQ(str1, "REPLACED REPLACED REPLACED");

    // Test with mixed case in 'from'
    std::string str2 = "Test test TeSt";
    Utils::replaceAllIgnoreCase(str2, "TeSt", "REPLACED");
    EXPECT_EQ(str2, "REPLACED REPLACED REPLACED");

    // Corrected overlapping case from audit.
    // The new algorithm finds 'a' at pos 0, replaces with 'AB', str becomes 'ABbab'.
    // It continues search from pos 1, finds 'a' at pos 3, replaces with 'AB', str becomes 'ABbABb'.
    std::string str3 = "abab";
    Utils::replaceAllIgnoreCase(str3, "a", "AB");
    EXPECT_EQ(str3, "ABbABb");

    // Test with empty from string
    std::string str4 = "test";
    Utils::replaceAllIgnoreCase(str4, "", "x");
    EXPECT_EQ(str4, "test");

    // Test with empty to string
    std::string str5 = "One Two ONE";
    Utils::replaceAllIgnoreCase(str5, "one", "");
    EXPECT_EQ(str5, " Two ");

    // Test no match
    std::string str6 = "abc";
    Utils::replaceAllIgnoreCase(str6, "d", "e");
    EXPECT_EQ(str6, "abc");
}


TEST_F(StringUtilsTest, Trim) {
    std::string_view whitespace = " \t\n\r\f\v";
    EXPECT_EQ(Utils::trim("   hello world   ", whitespace), "hello world");
    EXPECT_EQ(Utils::trim("hello world   ", whitespace), "hello world");
    EXPECT_EQ(Utils::trim("   hello world", whitespace), "hello world");
    EXPECT_EQ(Utils::trim("hello world", whitespace), "hello world");
    EXPECT_EQ(Utils::trim("   ", whitespace), "");
    EXPECT_EQ(Utils::trim("", whitespace), "");
    EXPECT_EQ(Utils::trim("\t\n hello \n\t", whitespace), "hello");
}

TEST_F(StringUtilsTest, Split) {
    std::string str1 = "a,b,c";
    std::vector<std::string> expected1 = {"a", "b", "c"};
    EXPECT_EQ(Utils::split(str1, ','), expected1);

    std::string str2 = "a,,b";
    std::vector<std::string> expected2 = {"a", "", "b"};
    EXPECT_EQ(Utils::split(str2, ','), expected2);

    std::string str3 = ",a,b";
    std::vector<std::string> expected3 = {"", "a", "b"};
    EXPECT_EQ(Utils::split(str3, ','), expected3);

    std::string str4 = "a,b,";
    std::vector<std::string> expected4 = {"a", "b", ""};
    EXPECT_EQ(Utils::split(str4, ','), expected4);

    std::string str5 = "abc";
    std::vector<std::string> expected5 = {"abc"};
    EXPECT_EQ(Utils::split(str5, ','), expected5);

    std::string str6 = "";
    std::vector<std::string> expected6 = {};
    EXPECT_EQ(Utils::split(str6, ','), expected6);
}

TEST_F(StringUtilsTest, ToLower) {
    EXPECT_EQ(Utils::toLower("Hello World"), "hello world");
    EXPECT_EQ(Utils::toLower("ALREADY lower"), "already lower");
    EXPECT_EQ(Utils::toLower("123!@#"), "123!@#");
    EXPECT_EQ(Utils::toLower(""), "");
}

TEST_F(StringUtilsTest, ToUpper) {
    EXPECT_EQ(Utils::toUpper("Hello World"), "HELLO WORLD");
    EXPECT_EQ(Utils::toUpper("ALREADY UPPER"), "ALREADY UPPER");
    EXPECT_EQ(Utils::toUpper("123!@#"), "123!@#");
    EXPECT_EQ(Utils::toUpper(""), "");
}

TEST_F(StringUtilsTest, CaseInsensitiveEquals) {
    EXPECT_TRUE(Utils::caseInsensitiveEquals(std::string("hello"), std::string("HELLO")));
    EXPECT_TRUE(Utils::caseInsensitiveEquals(std::string("HeLlO"), std::string("hElLo")));
    EXPECT_TRUE(Utils::caseInsensitiveEquals(std::string(""), std::string("")));
    EXPECT_FALSE(Utils::caseInsensitiveEquals(std::string("hello"), std::string("world")));
    EXPECT_FALSE(Utils::caseInsensitiveEquals(std::string("hello"), std::string("hell")));
    EXPECT_FALSE(Utils::caseInsensitiveEquals(std::string("hell"), std::string("hello")));
    EXPECT_FALSE(Utils::caseInsensitiveEquals(std::string("hello"), std::string("")));
}

TEST_F(StringUtilsTest, CaseInsensitiveSearch) {
    EXPECT_TRUE(Utils::caseInsensitiveSearch(std::string("Hello World"), std::string("world")));
    EXPECT_TRUE(Utils::caseInsensitiveSearch(std::string("Hello World"), std::string("WORLD")));
    EXPECT_TRUE(Utils::caseInsensitiveSearch(std::string("Hello World"), std::string("lo Wo")));
    EXPECT_TRUE(Utils::caseInsensitiveSearch(std::string("abc"), std::string(""))); // Empty pattern
    EXPECT_FALSE(Utils::caseInsensitiveSearch(std::string("Hello World"), std::string("goodbye")));
    EXPECT_FALSE(Utils::caseInsensitiveSearch(std::string(""), std::string("a"))); // Empty text
}

TEST_F(StringUtilsTest, CaseInsensitiveStarts) {
    EXPECT_TRUE(Utils::caseInsensitiveStarts("Hello World", "Hello"));
    EXPECT_TRUE(Utils::caseInsensitiveStarts("Hello World", "hElLo"));
    EXPECT_TRUE(Utils::caseInsensitiveStarts("Test", "Test"));
    EXPECT_TRUE(Utils::caseInsensitiveStarts("Test", ""));
    EXPECT_FALSE(Utils::caseInsensitiveStarts("Hello World", "World"));
    EXPECT_FALSE(Utils::caseInsensitiveStarts("Hello", "HelloWorld"));
    EXPECT_FALSE(Utils::caseInsensitiveStarts("", "a"));
}

TEST_F(StringUtilsTest, CaseInsensitiveEnds) {
    EXPECT_TRUE(Utils::caseInsensitiveEnds("Hello World", "World"));
    EXPECT_TRUE(Utils::caseInsensitiveEnds("Hello World", "wOrLd"));
    EXPECT_TRUE(Utils::caseInsensitiveEnds("Test", "Test"));
    EXPECT_TRUE(Utils::caseInsensitiveEnds("Test", ""));
    EXPECT_FALSE(Utils::caseInsensitiveEnds("Hello World", "Hello"));
    EXPECT_FALSE(Utils::caseInsensitiveEnds("World", "HelloWorld"));
    EXPECT_FALSE(Utils::caseInsensitiveEnds("", "a"));
}

TEST_F(StringUtilsTest, EscapeJsonString) {
    // Basic cases
    EXPECT_EQ(Utils::escapeJsonString(""), "");
    EXPECT_EQ(Utils::escapeJsonString("hello"), "hello");
    // Standard escapes
    EXPECT_EQ(Utils::escapeJsonString("\" \\ \b \f \n \r \t"), "\\\" \\\\ \\b \\f \\n \\r \\t");
    // Control characters
    EXPECT_EQ(Utils::escapeJsonString("\x01\x1F"), "\\u0001\\u001F");
    // UTF-8 multi-byte characters
    EXPECT_EQ(Utils::escapeJsonString("€"), "\\u20AC"); // 3-byte Euro sign
    EXPECT_EQ(Utils::escapeJsonString("你好"), "\\u4F60\\u597D"); // "Nǐ hǎo"
    EXPECT_EQ(Utils::escapeJsonString("😂"), "\\uD83D\\uDE02"); // 4-byte emoji (surrogate pair)
    // Mixed
    EXPECT_EQ(Utils::escapeJsonString("Text with \"quotes\" and € symbol."), "Text with \\\"quotes\\\" and \\u20AC symbol.");
    // Invalid UTF-8 sequence (e.g., a lone continuation byte)
    // The sequence \xDE\xAD is valid UTF-8 for U+07AD (Myanmar sign ASAT).
    // \xBE is a lone continuation byte, escaped individually.
    // \xEF is an incomplete 3-byte sequence (needs 2 continuation bytes), so it's escaped individually.
    EXPECT_EQ(Utils::escapeJsonString("\xDE\xAD\xBE\xEF"), "\\u07AD\\u00BE\\u00EF");
}

TEST_F(StringUtilsTest, GlobToRegex) {
    EXPECT_EQ(Utils::globToRegex("file*.txt"), "^file.*\\.txt$");
    EXPECT_EQ(Utils::globToRegex("file?.log"), "^file.\\.log$");
    EXPECT_EQ(Utils::globToRegex("config.json"), "^config\\.json$");
    EXPECT_EQ(Utils::globToRegex("special_chars-^$()[]{}|\\"), R"(^special_chars\-\^\$\(\)\[\]\{\}\|\\$)");
    EXPECT_EQ(Utils::globToRegex(""), "^$"); // Empty glob
}
