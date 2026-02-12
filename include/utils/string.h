// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef UTILS_STRING_H
#define UTILS_STRING_H

#include <string>
#include <string_view>
#include <vector>
#include <map> // Added for parseStructuredData
#include <regex> // Added for parseStructuredData

namespace Utils {

// Note: Case-insensitive comparisons are ASCII-only and not locale-dependent.
char asciiToLower(char c);
void replaceAll(std::string &str, const std::string &from, const std::string &to);
void replaceAllIgnoreCase(std::string& str, const std::string& from, const std::string& to);
std::string trim(const std::string& str, std::string_view whitespace = " \t\n\r\f\v");
std::vector<std::string> split(const std::string& str, char delimiter);
std::vector<std::string> split(const std::string& str, char delimiter, bool skipEmptyTokens);
std::string toLower(const std::string& str);
std::string toUpper(const std::string& str);
bool caseInsensitiveEquals(std::string_view s1, std::string_view s2);
bool caseInsensitiveSearch(std::string_view text, std::string_view pattern);
bool caseInsensitiveStarts(std::string_view text, std::string_view prefix);
bool caseInsensitiveEnds(std::string_view text, std::string_view suffix);
bool isNumeric(std::string_view s);
std::string escapeJsonString(const std::string& input);

/**
 * @brief Converts a limited glob pattern to a regex string.
 *
 * This implementation supports:
 * - `*`: matches any sequence of zero or more characters.
 * - `?`: matches any single character.
 *
 * It does NOT support more advanced glob features like character sets (`[a-z]`),
 * negations (`[!abc]`), or brace expansion (`{foo,bar}`).
 *
 * @param globPattern The glob pattern to convert.
 * @return A string representing the equivalent regular expression.
 */
std::string globToRegex(const std::string& globPattern);

// Added declarations for parsing structured data
void parseStructuredData(const std::string& data, std::map<std::string, std::string>& targetMap, const std::regex& kvPattern);
void parseLegacyStructuredData(const std::string& message, std::map<std::string, std::string>& targetMap);

} // namespace Utils

#endif // UTILS_STRING_H
