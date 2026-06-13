// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "core/CaseInsensitiveLess.h"
#include <map>
#include <string>

using LogAnalyzerInternal::CaseInsensitiveLess;

TEST(CaseInsensitiveLessTest, OrdersIgnoringCase) {
    CaseInsensitiveLess less;
    EXPECT_TRUE(less("abc", "ABD"));   // abc < abd ignoring case
    EXPECT_FALSE(less("ABD", "abc"));
    EXPECT_TRUE(less("Apple", "banana"));
    EXPECT_FALSE(less("banana", "Apple"));
}

TEST(CaseInsensitiveLessTest, TreatsSameLettersDifferentCaseAsEqual) {
    CaseInsensitiveLess less;
    // Equal ignoring case -> neither compares less than the other.
    EXPECT_FALSE(less("ABC", "abc"));
    EXPECT_FALSE(less("abc", "ABC"));
    EXPECT_FALSE(less("Apple", "aPPLE"));
    EXPECT_FALSE(less("aPPLE", "Apple"));
}

TEST(CaseInsensitiveLessTest, HandlesEmptyAndPrefixStrings) {
    CaseInsensitiveLess less;
    EXPECT_TRUE(less("", "a"));        // empty precedes non-empty
    EXPECT_FALSE(less("a", ""));
    EXPECT_FALSE(less("", ""));
    EXPECT_TRUE(less("ab", "abc"));    // a prefix precedes the longer string
    EXPECT_FALSE(less("abc", "ab"));
}

TEST(CaseInsensitiveLessTest, WorksAsCaseInsensitiveMapComparator) {
    std::map<std::string, int, CaseInsensitiveLess> m;
    m["Key"] = 1;
    m["KEY"] = 2;   // same key ignoring case -> overwrites the existing entry
    m["key"] = 3;
    EXPECT_EQ(m.size(), 1);
    EXPECT_EQ(m.at("kEy"), 3);
}
