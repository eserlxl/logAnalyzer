// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 eserlxl

#include "utils/String.h" // Explicitly include String.h
#include "utils/Core.h" // Includes all necessary declarations for Utils namespace
#include <algorithm>
#include <vector>
#include <regex>

namespace Utils {

// Helper for case-insensitive character comparison
static auto caseInsensitiveCharCompare = [](char c1, char c2) {
    return std::tolower(static_cast<unsigned char>(c1)) == std::tolower(static_cast<unsigned char>(c2));
};

void replaceAll(std::string &str, const std::string &from, const std::string &to) {
    if (from.empty()) return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' contains 'from'
    }
}

void replaceAllIgnoreCase(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty() || str.empty()) {
        return;
    }

    std::string result;
    result.reserve(str.length()); // Optimistic reservation

    size_t current_pos = 0;
    while (current_pos < str.length()) {
        auto it = std::search(str.begin() + current_pos, str.end(),
                              from.begin(), from.end(),
                              caseInsensitiveCharCompare);

        if (it == str.end()) {
            // No more occurrences found, append remaining part and break
            result.append(str, current_pos, std::string::npos);
            break;
        }

        size_t found_pos = std::distance(str.begin(), it);

        // Append the part of the string before the match
        result.append(str, current_pos, found_pos - current_pos);

        // Append the replacement string
        result.append(to);

        // Advance current_pos past the *found* (original) 'from' string
        // This is key to allowing 'to' to be part of subsequent searches, if 'from' overlaps with 'to'
        current_pos = found_pos + from.length();
    }
    str = std::move(result);
}

std::string trim(const std::string& str, std::string_view whitespace) {
    const size_t strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos) return ""; // no content

    const size_t strEnd = str.find_last_not_of(whitespace);
    const size_t strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    if (str.empty()) {
        return tokens;
    }
    std::string::size_type lastPos = 0;
    std::string::size_type findPos = str.find(delimiter, lastPos);

    while (findPos != std::string::npos) {
        tokens.push_back(str.substr(lastPos, findPos - lastPos));
        lastPos = findPos + 1;
        findPos = str.find(delimiter, lastPos);
    }

    tokens.push_back(str.substr(lastPos));
    return tokens;
}

std::string toLower(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return lowerStr;
}

std::string toUpper(const std::string& str) {
    std::string upperStr = str;
    std::transform(upperStr.begin(), upperStr.end(), upperStr.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    return upperStr;
}

bool caseInsensitiveEquals(const std::string& s1, const std::string& s2) {
    if (s1.length() != s2.length()) {
        return false;
    }
    return std::equal(s1.begin(), s1.end(), s2.begin(), caseInsensitiveCharCompare);
}

bool caseInsensitiveSearch(const std::string& text, const std::string& pattern) {
    if (pattern.empty()) {
        return true; // Or false, depending on desired behavior; true is common
    }
    if (text.empty() && !pattern.empty()) {
        return false;
    }
    auto it = std::search(text.begin(), text.end(),
                          pattern.begin(), pattern.end(),
                          caseInsensitiveCharCompare);
    return it != text.end();
}

bool caseInsensitiveStarts(const std::string& text, const std::string& prefix) {
    if (prefix.length() > text.length()) {
        return false;
    }
    return std::equal(prefix.begin(), prefix.end(), text.begin(), caseInsensitiveCharCompare);
}

bool caseInsensitiveEnds(const std::string& text, const std::string& suffix) {
    if (suffix.length() > text.length()) {
        return false;
    }
    return std::equal(suffix.rbegin(), suffix.rend(), text.rbegin(), caseInsensitiveCharCompare);
}

// Helper to convert a Unicode code point to its UTF-16 surrogate pair representation if necessary
// and format it as JSON \uXXXX escapes.
static void appendJsonUnicodeEscape(std::string& output, uint32_t cp) {
    if (cp <= 0xFFFF) {
        // Basic Multilingual Plane (BMP) characters
        char buf[7]; // \uXXXX\0
        snprintf(buf, sizeof(buf), "\\u%04X", cp);
        output += buf;
    } else if (cp <= 0x10FFFF) {
        // Supplementary Plane characters (requires surrogate pair)
        // Convert to surrogate pair
        cp -= 0x10000;
        uint32_t high_surrogate = (cp >> 10) + 0xD800;
        uint32_t low_surrogate = (cp & 0x3FF) + 0xDC00;

        char buf[13]; // \uXXXX\uYYYY\0
        snprintf(buf, sizeof(buf), "\\u%04X\\u%04X", high_surrogate, low_surrogate);
        output += buf;
    } else {
        // Invalid Unicode code point, escape as replacement character
        char buf[7]; // \uFFFD\0
        snprintf(buf, sizeof(buf), "\\u%04X", 0xFFFD); // Use Unicode replacement character
        output += buf;
    }
}

std::string escapeJsonString(const std::string& input) {
    std::string output;
    output.reserve(input.length()); // Reserve space

    for (size_t i = 0; i < input.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);

        switch (c) {
            case '"':  output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (c < 0x20) { // Control characters
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04X", c);
                    output += buf;
                } else if (c <= 0x7E) { // Printable ASCII
                    output += c;
                } else { // Potential multi-byte UTF-8
                    uint32_t code_point = 0;
                    int num_bytes = 0;

                    if ((c & 0xE0) == 0xC0) { // 2-byte sequence
                        if (i + 1 < input.length()) {
                            unsigned char c2 = static_cast<unsigned char>(input[i+1]);
                            if ((c2 & 0xC0) == 0x80) {
                                code_point = ((c & 0x1F) << 6) | (c2 & 0x3F);
                                if (code_point >= 0x80) { // Not overlong
                                    num_bytes = 2;
                                }
                            }
                        }
                    } else if ((c & 0xF0) == 0xE0) { // 3-byte sequence
                        if (i + 2 < input.length()) {
                            unsigned char c2 = static_cast<unsigned char>(input[i+1]);
                            unsigned char c3 = static_cast<unsigned char>(input[i+2]);
                            if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80) {
                                code_point = ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                                if (code_point >= 0x800 && (code_point < 0xD800 || code_point > 0xDFFF)) { // Not overlong and not a surrogate
                                    num_bytes = 3;
                                }
                            }
                        }
                    } else if ((c & 0xF8) == 0xF0) { // 4-byte sequence
                        if (i + 3 < input.length()) {
                            unsigned char c2 = static_cast<unsigned char>(input[i+1]);
                            unsigned char c3 = static_cast<unsigned char>(input[i+2]);
                            unsigned char c4 = static_cast<unsigned char>(input[i+3]);
                            if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80 && (c4 & 0xC0) == 0x80) {
                                code_point = ((c & 0x07) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) << 6) | (c4 & 0x3F);
                                if (code_point >= 0x10000 && code_point <= 0x10FFFF) { // Not overlong and in valid range
                                    num_bytes = 4;
                                }
                            }
                        }
                    }

                    if (num_bytes > 0) {
                        appendJsonUnicodeEscape(output, code_point);
                        i += (num_bytes - 1);
                    } else {
                        // Invalid UTF-8 sequence, escape the byte
                        char buf[7];
                        snprintf(buf, sizeof(buf), "\\u%04X", c);
                        output += buf;
                    }
                }
                break;
        }
    }
    return output;
}

std::string globToRegex(const std::string& globPattern) {
    std::string regexPattern = "^"; // Start anchor
    for (char c : globPattern) {
        switch (c) {
            case '*':
                regexPattern += ".*";
                break;
            case '?':
                regexPattern += ".";
                break;
            case '.':
            case '+':
            case '^':
            case '$':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '|':
            case '\\':
            case '-': // Added to escape
                regexPattern.push_back('\\'); // Escape regex special characters
                regexPattern += c;
                break;
            default:
                regexPattern += c;
                break;
        }
    }
    regexPattern += "$"; // End anchor
    return regexPattern;
}

} // namespace Utils

