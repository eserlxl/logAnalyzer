#include "utils/Core.h"
#include <gtest/gtest.h>

// Test suite for string utility functions
class StringUtilsTest : public ::testing::Test {
protected:
    // You can add helper functions or member variables here if needed
};

TEST_F(StringUtilsTest, ReplaceAll) {
    std::string str = "one two three two one";
    Utils::replaceAll(str, "two", "2");
    EXPECT_EQ(str, "one 2 three 2 one");

    Utils::replaceAll(str, "one", "1");
    EXPECT_EQ(str, "1 2 three 2 1");

    // Test replacement where 'to' contains 'from'
    std::string str2 = "abab";
    Utils::replaceAll(str2, "a", "ab");
    EXPECT_EQ(str2, "abbabb");

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

    // Buggy case from audit
    std::string str3 = "Banana banana";
    Utils::replaceAllIgnoreCase(str3, "banana", "APPLE");
    EXPECT_EQ(str3, "APPLE APPLE");

    // Another overlapping case
    std::string str4 = "abab";
    Utils::replaceAllIgnoreCase(str4, "A", "AB");
    EXPECT_EQ(str4, "ABbABb");
}

TEST_F(StringUtilsTest, Trim) {
    EXPECT_EQ(Utils::trim("   hello world   "), "hello world");
    EXPECT_EQ(Utils::trim("hello world   "), "hello world");
    EXPECT_EQ(Utils::trim("   hello world"), "hello world");
    EXPECT_EQ(Utils::trim("hello world"), "hello world");
    EXPECT_EQ(Utils::trim("   "), "");
    EXPECT_EQ(Utils::trim(""), "");
    EXPECT_EQ(Utils::trim("\t\n hello \n\t"), "hello");
}

TEST_F(StringUtilsTest, Split) {
    std::string str1 = "a,b,c";
    std::vector<std::string> expected1 = {"a", "b", "c"};
    EXPECT_EQ(Utils::split(str1, ','), expected1);

    // Consecutive delimiters
    std::string str2 = "a,,b";
    std::vector<std::string> expected2 = {"a", "", "b"};
    EXPECT_EQ(Utils::split(str2, ','), expected2);

    // Leading delimiter
    std::string str3 = ",a,b";
    std::vector<std::string> expected3 = {"", "a", "b"};
    EXPECT_EQ(Utils::split(str3, ','), expected3);

    // Trailing delimiter
    std::string str4 = "a,b,";
    std::vector<std::string> expected4 = {"a", "b", ""};
    EXPECT_EQ(Utils::split(str4, ','), expected4);

    // No delimiters
    std::string str5 = "abc";
    std::vector<std::string> expected5 = {"abc"};
    EXPECT_EQ(Utils::split(str5, ','), expected5);

    // Empty string
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

TEST_F(StringUtilsTest, EscapeJsonString) {
    EXPECT_EQ(Utils::escapeJsonString("hello"), "hello");
    EXPECT_EQ(Utils::escapeJsonString("hello \"world\""), "hello \\\"world\\\"");
    EXPECT_EQ(Utils::escapeJsonString("c:\\path"), "c:\\\\path");
    EXPECT_EQ(Utils::escapeJsonString("\b\f\n\r\t"), "\\b\\f\\n\\r\\t");
    EXPECT_EQ(Utils::escapeJsonString("\x1f"), "\\u001f");
}

TEST_F(StringUtilsTest, GlobToRegex) {
    EXPECT_EQ(Utils::globToRegex("file*.txt"), "file.*\\.txt");
    EXPECT_EQ(Utils::globToRegex("file?.log"), "file.\\.log");
    EXPECT_EQ(Utils::globToRegex("config.json"), "config\\.json");
    EXPECT_EQ(Utils::globToRegex("special_chars-^$()[]{}|\\"), "special_chars\\-\\^\\$\\(\\)\\[\\]\\{\\}\\|\\\\");
}
