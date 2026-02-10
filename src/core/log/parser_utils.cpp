// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "utils/string.h"
#include <regex>

namespace Utils {
    void parseStructuredData(const std::string& data, std::map<std::string, std::string>& targetMap, const std::regex& kvPattern) {
        std::sregex_iterator next(data.begin(), data.end(), kvPattern);
        std::sregex_iterator end;
        while (next != end) {
            const std::smatch& match = *next;
            std::string key = match[1].str();
            std::string value;

            // Iterate through all capture groups starting from index 2 to find the value
            for (size_t i = 2; i < match.size(); ++i) {
                if (match[i].matched) {
                    value = match[i].str();
                    break;
                }
            }
            targetMap[key] = value;
            ++next;
        }
    }

    void parseLegacyStructuredData(const std::string& message, std::map<std::string, std::string>& targetMap) {
        static const std::regex kvPattern("([a-zA-Z0-9_.-]+)\\s*=\\s*(?:\"(.*?)\"|'([^']*)'|([^\\s,]+))[, ]*", std::regex::optimize);
        parseStructuredData(message, targetMap, kvPattern);
    }
}
