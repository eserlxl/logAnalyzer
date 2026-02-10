// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef LOG_ANALYZER_CORE_LOG_PARSER_UTILS_H
#define LOG_ANALYZER_CORE_LOG_PARSER_UTILS_H

#include <string>
#include <map>
#include <regex>

namespace Utils {
    // Function to parse structured data from a string into a map
    void parseStructuredData(const std::string& data, std::map<std::string, std::string>& targetMap, const std::regex& kvPattern);

    // Legacy wrapper
    void parseLegacyStructuredData(const std::string& message, std::map<std::string, std::string>& targetMap);
}

#endif // LOG_ANALYZER_CORE_LOG_PARSER_UTILS_H
