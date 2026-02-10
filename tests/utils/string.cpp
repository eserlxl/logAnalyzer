// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "utils/String.h"
#include "utils/Core.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <limits>

// Test suite for string utility functions
class StringUtilsTest : public ::testing::Test {};

TEST_F(StringUtilsTest, ReplaceAll) {
    std::string str = "one two three two one";
    Utils::replaceAll(str, "two", "2");
    EXPECT_EQ(str, "one 2 three 2 one");

    Utils::replaceAll(str, "one", "1");
    EXPECT_EQ(str, "1 2 three 2 1");

    // Test replacement where 'to' contains 'from'
    // With an implementation that advances past the original 'from' string,
    // this is the correct behavior.
    std::string str2 = "abab";
    Utils::replaceAll(str2, "a", "ab");
    EXPECT_EQ(str2, "abbabb"); // 'a' at 0 -> 'ab' (str="abbab", start_pos=2), 'a' at 3 -> 'ab' (str="abbabb", start_pos=5)

    // Another overlapping test: replacing "a" with "aa"
    std::string str_overlap1 = "aaa";
    Utils::replaceAll(str_overlap1, "a", "aa");
    EXPECT_EQ(str_overlap1, "aaaaaa"); // a (0) -> aa (str=a"aa"aa, pos=2), a(2) -> aa (str=aaaaaa, pos=4)

    // Replacing "aa" with "a"
    std::string str_overlap2 = "aaaa";
    Utils::replaceAll(str_overlap2, "aa", "a");
    EXPECT_EQ(str_overlap2, "aa"); // aa (0) -> a (str=a"a"aa, pos=1), aa (2) -> a (str=aa, pos=2)

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

    // Overlapping case: replacing "a" with "AB" in "abab"
    std::string str3 = "abab";
    Utils::replaceAllIgnoreCase(str3, "a", "AB");
    EXPECT_EQ(str3, "ABbABb");

    // Another overlapping test: replacing "A" with "AA" in "AAA"
    std::string str_overlap1 = "AAA";
    Utils::replaceAllIgnoreCase(str_overlap1, "A", "AA");
    EXPECT_EQ(str_overlap1, "AAAAAA");

    // Replacing "AA" with "A"
    std::string str_overlap2 = "AaAa";
    Utils::replaceAllIgnoreCase(str_overlap2, "aA", "a");
    EXPECT_EQ(str_overlap2, "aa");

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
    // Original behavior (skipEmptyTokens = false by default)
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

    // Tests for multiple consecutive delimiters, leading/trailing
    std::string str7 = ",,a,b,,";
    std::vector<std::string> expected7 = {"", "", "a", "b", "", ""};
    EXPECT_EQ(Utils::split(str7, ','), expected7);

    std::string str8 = "a,,,b";
    std::vector<std::string> expected8 = {"a", "", "", "b"};
    EXPECT_EQ(Utils::split(str8, ','), expected8);
    
    std::string str9 = ",,,";
    std::vector<std::string> expected9 = {"", "", "", ""};
    EXPECT_EQ(Utils::split(str9, ','), expected9);

    std::string str10 = ",";
    std::vector<std::string> expected10 = {"", ""};
    EXPECT_EQ(Utils::split(str10, ','), expected10);
}

TEST_F(StringUtilsTest, SplitSkipEmptyTokens) {
    // With skipEmptyTokens = true
    std::string str1 = "a,b,c";
    std::vector<std::string> expected1 = {"a", "b", "c"};
    EXPECT_EQ(Utils::split(str1, ',', true), expected1);

    std::string str2 = "a,,b";
    std::vector<std::string> expected2 = {"a", "b"};
    EXPECT_EQ(Utils::split(str2, ',', true), expected2);

    std::string str3 = ",a,b";
    std::vector<std::string> expected3 = {"a", "b"};
    EXPECT_EQ(Utils::split(str3, ',', true), expected3);

    std::string str4 = "a,b,";
    std::vector<std::string> expected4 = {"a", "b"};
    EXPECT_EQ(Utils::split(str4, ',', true), expected4);

    std::string str5 = "abc";
    std::vector<std::string> expected5 = {"abc"};
    EXPECT_EQ(Utils::split(str5, ',', true), expected5);

    std::string str6 = "";
    std::vector<std::string> expected6 = {};
    EXPECT_EQ(Utils::split(str6, ',', true), expected6);

    // Tests for multiple consecutive delimiters, leading/trailing (skipped)
    std::string str7 = ",,a,b,,";
    std::vector<std::string> expected7 = {"a", "b"};
    EXPECT_EQ(Utils::split(str7, ',', true), expected7);

    std::string str8 = "a,,,b";
    std::vector<std::string> expected8 = {"a", "b"};
    EXPECT_EQ(Utils::split(str8, ',', true), expected8);
    
    std::string str9 = ",,,";
    std::vector<std::string> expected9 = {};
    EXPECT_EQ(Utils::split(str9, ',', true), expected9);

    std::string str10 = ",";
    std::vector<std::string> expected10 = {};
    EXPECT_EQ(Utils::split(str10, ',', true), expected10);

    // Test string with only empty tokens
    std::string str11 = ",,,,,,";
    std::vector<std::string> expected11 = {};
    EXPECT_EQ(Utils::split(str11, ',', true), expected11);
}


TEST_F(StringUtilsTest, ToLower) {
    EXPECT_EQ(Utils::toLower("Hello World"), "hello world");
    EXPECT_EQ(Utils::toLower("ALREADY lower"), "already lower");
    EXPECT_EQ(Utils::toLower("123!@#"), "123!@#");
    EXPECT_EQ(Utils::toLower(""), "");
    // Test with non-ASCII characters (should only affect ASCII part)
    EXPECT_EQ(Utils::toLower("Grüße"), "grüße");
}

TEST_F(StringUtilsTest, ToUpper) {
    EXPECT_EQ(Utils::toUpper("Hello World"), "HELLO WORLD");
    EXPECT_EQ(Utils::toUpper("ALREADY UPPER"), "ALREADY UPPER");
    EXPECT_EQ(Utils::toUpper("123!@#"), "123!@#");
    EXPECT_EQ(Utils::toUpper(""), "");
    // Test with non-ASCII characters (should only affect ASCII part)
    EXPECT_EQ(Utils::toUpper("Grüße"), "GRüßE");
}

TEST_F(StringUtilsTest, CaseInsensitiveEquals) {
    EXPECT_TRUE(Utils::caseInsensitiveEquals(std::string("hello"), std::string("HELLO")));
    EXPECT_TRUE(Utils::caseInsensitiveEquals(std::string("HeLlO"), std::string("hElLo")));
    EXPECT_TRUE(Utils::caseInsensitiveEquals(std::string(""), std::string("")));
    EXPECT_FALSE(Utils::caseInsensitiveEquals(std::string("hello"), std::string("world")));
    EXPECT_FALSE(Utils::caseInsensitiveEquals(std::string("hello"), std::string("hell")));
    EXPECT_FALSE(Utils::caseInsensitiveEquals(std::string("hell"), std::string("hello")));
    EXPECT_FALSE(Utils::caseInsensitiveEquals(std::string("hello"), std::string("")));
    // Test with non-ASCII characters (should compare byte-wise for non-ASCII parts)
    EXPECT_TRUE(Utils::caseInsensitiveEquals(std::string("Grüße"), std::string("grüße")));
    EXPECT_TRUE(Utils::caseInsensitiveEquals(std::string("Grüße"), std::string("grüßE")));
}

TEST_F(StringUtilsTest, CaseInsensitiveSearch) {
    EXPECT_TRUE(Utils::caseInsensitiveSearch(std::string("Hello World"), std::string("world")));
    EXPECT_TRUE(Utils::caseInsensitiveSearch(std::string("Hello World"), std::string("WORLD")));
    EXPECT_TRUE(Utils::caseInsensitiveSearch(std::string("Hello World"), std::string("lo Wo")));
    EXPECT_TRUE(Utils::caseInsensitiveSearch(std::string("abc"), std::string(""))); // Empty pattern
    EXPECT_FALSE(Utils::caseInsensitiveSearch(std::string("Hello World"), std::string("goodbye")));
    EXPECT_FALSE(Utils::caseInsensitiveSearch(std::string(""), std::string("a"))); // Empty text
    // Test with non-ASCII characters
    EXPECT_TRUE(Utils::caseInsensitiveSearch(std::string("Ein Grüße"), std::string("grüße")));
    EXPECT_TRUE(Utils::caseInsensitiveSearch(std::string("Ein Grüße"), std::string("grüßE")));
}

TEST_F(StringUtilsTest, CaseInsensitiveStarts) {
    EXPECT_TRUE(Utils::caseInsensitiveStarts("Hello World", "Hello"));
    EXPECT_TRUE(Utils::caseInsensitiveStarts("Hello World", "hElLo"));
    EXPECT_TRUE(Utils::caseInsensitiveStarts("Test", "Test"));
    EXPECT_TRUE(Utils::caseInsensitiveStarts("Test", ""));
    EXPECT_FALSE(Utils::caseInsensitiveStarts("Hello World", "World"));
    EXPECT_FALSE(Utils::caseInsensitiveStarts("Hello", "HelloWorld"));
    EXPECT_FALSE(Utils::caseInsensitiveStarts("", "a"));
    // Test with non-ASCII characters
    EXPECT_TRUE(Utils::caseInsensitiveStarts("Grüße", "grü"));
    EXPECT_TRUE(Utils::caseInsensitiveStarts("Grüße", "GrüßE"));
}

TEST_F(StringUtilsTest, CaseInsensitiveEnds) {
    EXPECT_TRUE(Utils::caseInsensitiveEnds("Hello World", "World"));
    EXPECT_TRUE(Utils::caseInsensitiveEnds("Hello World", "wOrLd"));
    EXPECT_TRUE(Utils::caseInsensitiveEnds("Test", "Test"));
    EXPECT_TRUE(Utils::caseInsensitiveEnds("Test", ""));
    EXPECT_FALSE(Utils::caseInsensitiveEnds("Hello World", "Hello"));
    EXPECT_FALSE(Utils::caseInsensitiveEnds("World", "HelloWorld"));
    EXPECT_FALSE(Utils::caseInsensitiveEnds("", "a"));
    // Test with non-ASCII characters
    EXPECT_TRUE(Utils::caseInsensitiveEnds("Grüße", "üße"));
    EXPECT_FALSE(Utils::caseInsensitiveEnds("Grüße", "ÜßE")); // 'Ü' not matched case-insensitively
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

    // --- Malformed UTF-8 edge cases ---
    // Lone start bytes
    EXPECT_EQ(Utils::escapeJsonString(std::string(1, '\xC0')), "\\uFFFD"); // Invalid 2-byte start byte
    EXPECT_EQ(Utils::escapeJsonString(std::string(1, '\xED')), "\\uFFFD"); // Invalid 3-byte start byte (surrogate, but without sequence)
    EXPECT_EQ(Utils::escapeJsonString(std::string(1, '\xF5')), "\\uFFFD"); // Invalid 4-byte start byte (too high)
    EXPECT_EQ(Utils::escapeJsonString(std::string(1, '\xFE')), "\\uFFFD"); // Invalid start byte
    EXPECT_EQ(Utils::escapeJsonString(std::string(1, '\xFF')), "\\uFFFD"); // Invalid start byte

    // Incomplete multi-byte sequences at the end of the string
    EXPECT_EQ(Utils::escapeJsonString(std::string("hello") + '\xC2'), "hello\\uFFFD"); // Incomplete 2-byte sequence
    EXPECT_EQ(Utils::escapeJsonString(std::string("hello") + '\xE0' + '\xA0'), "hello\\uFFFD\\uFFFD"); // Incomplete 3-byte sequence
    EXPECT_EQ(Utils::escapeJsonString(std::string("hello") + '\xF0' + '\x90' + '\x80'), "hello\\uFFFD\\uFFFD\\uFFFD"); // Incomplete 4-byte sequence

    // Overlong encodings
    EXPECT_EQ(Utils::escapeJsonString(std::string{'\xC0', '\x80'}), "\\uFFFD\\uFFFD"); // Overlong U+0000
    EXPECT_EQ(Utils::escapeJsonString(std::string{'\xC1', '\xBF'}), "\\uFFFD\\uFFFD"); // Overlong U+007F
    EXPECT_EQ(Utils::escapeJsonString(std::string{'\xE0', '\x80', '\x80'}), "\\uFFFD\\uFFFD\\uFFFD"); // Overlong U+0000
    EXPECT_EQ(Utils::escapeJsonString(std::string{'\xE0', '\x81', '\xBF'}), "\\uFFFD\\uFFFD\\uFFFD"); // Overlong U+007F
    EXPECT_EQ(Utils::escapeJsonString(std::string{'\xF0', '\x80', '\x80', '\x80'}), "\\uFFFD\\uFFFD\\uFFFD\\uFFFD"); // Overlong U+0000

    // Surrogates outside of a pair
    EXPECT_EQ(Utils::escapeJsonString(std::string{'\xED', '\xA0', '\x80'}), "\\uFFFD\\uFFFD\\uFFFD"); // High surrogate (U+D800)
    EXPECT_EQ(Utils::escapeJsonString(std::string{'\xED', '\xBF', '\xBF'}), "\\uFFFD\\uFFFD\\uFFFD"); // Low surrogate (U+DFFF)
    EXPECT_EQ(Utils::escapeJsonString(std::string{'\xED', '\xA0', '\x80', '\xED', '\xA0', '\x80'}), "\\uFFFD\\uFFFD\\uFFFD\\uFFFD\\uFFFD\\uFFFD"); // Two high surrogates

    // U+FFFE, U+FFFF (noncharacters)
    EXPECT_EQ(Utils::escapeJsonString(std::string{'\xEF', '\xBF', '\xBE'}), "\\uFFFE"); // U+FFFE
    EXPECT_EQ(Utils::escapeJsonString(std::string{'\xEF', '\xBF', '\xBF'}), "\\uFFFF"); // U+FFFF

    // Mixed valid and invalid
    EXPECT_EQ(Utils::escapeJsonString(std::string("Valid") + '\xF0' + '\x90' + '\x80' + "Invalid" + '\xC2' + "Char"), "Valid\\uFFFD\\uFFFD\\uFFFDInvalid\\uFFFDChar");
    EXPECT_EQ(Utils::escapeJsonString(std::string{'\xED', '\xA0', '\x80', '\xF0', '\x90', '\x80', '\x80'}), "\\uFFFD\\uFFFD\\uFFFD\\uD800\\uDC00"); // Mixed invalid and valid 4-byte sequence


    // Invalid continuation byte
    EXPECT_EQ(Utils::escapeJsonString(std::string{'\xC2', 'A'}), "\\uFFFDA"); // \xC2 is start byte, 'A' is not continuation

}

TEST_F(StringUtilsTest, GlobToRegex) {
    EXPECT_EQ(Utils::globToRegex("file*.txt"), "^file.*\\.txt$");
    EXPECT_EQ(Utils::globToRegex("file?.log"), "^file.\\.log$");
    EXPECT_EQ(Utils::globToRegex("config.json"), "^config\\.json$");
    // Test with all special regex characters that should be escaped, including '-'
    EXPECT_EQ(Utils::globToRegex("special_chars-^$()[]{}|\\.+"), R"(^special_chars\-\^\$\(\)\[\]\{\}\|\\\.\+$)");
    EXPECT_EQ(Utils::globToRegex(""), "^$"); // Empty glob
    EXPECT_EQ(Utils::globToRegex("*"), "^.*$"); // Only asterisk
    EXPECT_EQ(Utils::globToRegex("?"), "^.$"); // Only question mark
}

TEST_F(StringUtilsTest, ParseHumanReadableSizeBasic) {
    auto bytes = Utils::parseHumanReadableSize("1536");
    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(*bytes, 1536u);

    auto kib = Utils::parseHumanReadableSize("1.5KB");
    ASSERT_TRUE(kib.has_value());
    EXPECT_EQ(*kib, 1536u);

    auto withWhitespace = Utils::parseHumanReadableSize(" 2 MB ");
    ASSERT_TRUE(withWhitespace.has_value());
    EXPECT_EQ(*withWhitespace, 2u * 1024u * 1024u);
}

TEST_F(StringUtilsTest, ParseHumanReadableSizeRejectsOverflow) {
    const std::string oversizedKb = std::to_string(std::numeric_limits<size_t>::max() / 1024u + 1u) + "KB";
    EXPECT_FALSE(Utils::parseHumanReadableSize(oversizedKb).has_value());
}

TEST_F(StringUtilsTest, ParseHumanReadableSizeRejectsMalformedNumericPart) {
    EXPECT_FALSE(Utils::parseHumanReadableSize("1..5KB").has_value());
    EXPECT_FALSE(Utils::parseHumanReadableSize("1.2.3").has_value());
    EXPECT_FALSE(Utils::parseHumanReadableSize("1 2KB").has_value());
    EXPECT_FALSE(Utils::parseHumanReadableSize("1e3KB").has_value());
    EXPECT_FALSE(Utils::parseHumanReadableSize("1e3").has_value());
}
