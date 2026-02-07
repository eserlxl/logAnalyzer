// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "core/Log/ParserUtils.h"
#include <regex>

namespace Utils {
    void parseStructuredData(const std::string& data, std::map<std::string, std::string>& targetMap, const std::regex& kvPattern) {
        std::sregex_iterator next(data.begin(), data.end(), kvPattern);
        std::sregex_iterator end;
        while (next != end) {
            std::smatch match = *next;
            std::string key = match[1].str();
            std::string value;

            // Check for quoted values (groups 2 and 3) or unquoted (group 4)
            if (match[2].matched) { // Double quotes
                value = match[2].str();
            } else if (match[3].matched) { // Single quotes
                value = match[3].str();
            } else if (match[4].matched) { // Unquoted
                value = match[4].str();
            }
            targetMap[key] = value;
            next++;
        }
    }

    void parseLegacyStructuredData(const std::string& message, std::map<std::string, std::string>& targetMap) {
        static const std::regex kvPattern("([a-zA-Z0-9_.-]+)\\s*=\\s*(?:\"(.*?)\"|'([^']*)'|([^\\s,]+))[, ]*", std::regex::optimize);
        parseStructuredData(message, targetMap, kvPattern);
    }
}
