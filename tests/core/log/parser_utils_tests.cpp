// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "core/log/parserutils.h"
#include <regex>

TEST(ParserUtilsTest, ParseStructuredData_LegacyPattern) {
    std::string log = "key1=\"value1\" key2='value2' key3=value3";
    std::map<std::string, std::string> results;
    
    Utils::parseLegacyStructuredData(log, results);
    
    EXPECT_EQ(results["key1"], "value1");
    EXPECT_EQ(results["key2"], "value2");
    EXPECT_EQ(results["key3"], "value3");
}

TEST(ParserUtilsTest, ParseStructuredData_CustomPattern_Group5) {
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
