// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "core/log/parser_utils.h"

namespace Utils {
    void parseStructuredData(const std::string& data, std::map<std::string, std::string>& targetMap, const std::regex& kvPattern) {
        std::sregex_iterator next(data.begin(), data.end(), kvPattern);
        std::sregex_iterator end;
        while (next != end) {
            const std::smatch& match = *next;
            // Ensure there's at least a capture group for the key (group 1)
            if (match.size() < 2) {
                ++next;
                continue;
            }
            std::string key = match[1].str();
            std::string value;
            bool value_matched = false;
            // Iterate through all capture groups starting from index 2 to find the value
            // Selects the first non-empty capturing group after the key's capturing group
            for (size_t i = 2; i < match.size(); ++i) {
                if (match[i].matched) {
                    value = match[i].str();
                    value_matched = true;
                    break;
                }
            }
            if (value_matched) { // Only add if a value capturing group was successfully matched
                targetMap[key] = value;
            }
            ++next;
        }
    }

    void parseLegacyStructuredData(const std::string& message, std::map<std::string, std::string>& targetMap) {
        // Changed ([^\\s,]+) to ([^\\s,]*) to allow empty unquoted values
        static const std::regex kvPattern(
            "([a-zA-Z0-9_.-]+)\\s*=\\s*(?:"
            "\"(.*?)\"|" // Group 2: Properly double-quoted value (without quotes)
            "'([^']*)'|" // Group 3: Properly single-quoted value (without quotes)
            // Group 4: Malformed double-quoted value (includes opening quote, up to next key or end)
            "(\"[^\\s,]*?(?:(?=[a-zA-Z0-9_.-]+\\s*=)|$))|"
            // Group 5: Malformed single-quoted value (includes opening quote, up to next key or end)
            "('[^\\s,]*?(?:(?=[a-zA-Z0-9_.-]+\\s*=)|$))|"
            // Group 6: Unquoted value (up to next key or end)
            "([^\\s,]*?(?:(?=[a-zA-Z0-9_.-]+\\s*=)|$))"
            ")[, ]*", std::regex::optimize);
        parseStructuredData(message, targetMap, kvPattern);
    }
}
