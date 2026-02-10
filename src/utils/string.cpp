// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "utils/String.h" // Explicitly include String.h
#include "utils/Core.h" // Includes all necessary declarations for Utils namespace
#include <algorithm>
#include <vector>
#include <regex> // For globToRegex only
#include <iomanip> // For std::hex, std::uppercase in appendJsonUnicodeEscape

namespace Utils {

// Custom ASCII-only toLower to avoid locale issues with std::tolower.
// Only characters 'A' through 'Z' are converted. Others are returned as is.
char asciiToLower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c + ('a' - 'A'));
    }
    return c;
}

// Custom ASCII-only toUpper to avoid locale issues with std::toupper.
// Only characters 'a' through 'z' are converted. Others are returned as is.
char asciiToUpper(char c) {
    if (c >= 'a' && c <= 'z') {
        return static_cast<char>(c - ('a' - 'A'));
    }
    return c;
}

// Helper for case-insensitive character comparison (ASCII-only)
static auto caseInsensitiveCharCompare = [](char c1, char c2) {
    return asciiToLower(c1) == asciiToLower(c2);
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
        // Use std::search with custom comparator for case-insensitive find
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
    return split(str, delimiter, false); // Default to not skipping empty tokens
}

std::vector<std::string> split(const std::string& str, char delimiter, bool skipEmptyTokens) {
    std::vector<std::string> tokens;
    if (str.empty()) {
        return tokens;
    }

    std::string currentToken;
    std::istringstream iss(str);
    std::string segment;

    while (std::getline(iss, segment, delimiter)) {
        if (skipEmptyTokens && segment.empty()) {
            continue;
        }
        tokens.push_back(segment);
    }
    
    // Handle trailing delimiter: if the last character is a delimiter, std::getline won't
    // add an empty string for the part after it. We need to add it explicitly if not skipping.
    // Also handles the case of a single delimiter string like "," producing {"",""}
    if (!skipEmptyTokens && str.length() > 0 && str.back() == delimiter) {
        tokens.push_back("");
    }
    // Special case for an empty string input with no delimiters (should yield one empty token if not skipping)
    // The previous logic for `str.empty()` at the beginning handles this if the string is truly empty.
    // But if we start with ",a,b", we get {"", "a", "b"} correctly.
    // If str is "a,b,", we want {"a", "b", ""}.
    // If str is "a,,b", we want {"a", "", "b"}.
    // The current getline loop for "a,b," will produce {"a", "b"}. The check above adds "".
    // For "a,,b", it produces {"a", "", "b"}. This is correct.
    // For ",a,b", it produces {"", "a", "b"}. This is correct.
    // For ",,", it produces {"", ""}. The check above adds another "". This is wrong.
    // Let's refine the trailing empty token logic based on `std::string::find`.

    // Re-implement `split` more robustly with `std::string::find` for consistent empty token handling.
    tokens.clear(); // Clear tokens from previous attempt
    size_t lastPos = 0;
    size_t findPos = str.find(delimiter, lastPos);

    while (findPos != std::string::npos) {
        std::string token = str.substr(lastPos, findPos - lastPos);
        if (!(skipEmptyTokens && token.empty())) {
            tokens.push_back(token);
        }
        lastPos = findPos + 1;
        findPos = str.find(delimiter, lastPos);
    }

    // Add the last token
    std::string lastToken = str.substr(lastPos);
    if (!(skipEmptyTokens && lastToken.empty())) {
        tokens.push_back(lastToken);
    }
    
    return tokens;
}

std::string toLower(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
                   [](char c){ return asciiToLower(c); }); // Use asciiToLower
    return lowerStr;
}

std::string toUpper(const std::string& str) {
    std::string upperStr = str;
    std::transform(upperStr.begin(), upperStr.end(), upperStr.begin(),
                   [](char c){ return asciiToUpper(c); }); // Use asciiToUpper
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
    // Use reverse iterators for efficiency and correctness with std::equal
    return std::equal(suffix.rbegin(), suffix.rend(), text.rbegin(), caseInsensitiveCharCompare);
}

bool isNumeric(std::string_view s) {
    if (s.empty()) return false;
    
    size_t start = 0;
    if (s[0] == '-' || s[0] == '+') {
        if (s.length() == 1) return false;
        start = 1;
    }
    
    bool hasDecimal = false;
    bool hasDigits = false;
    
    for (size_t i = start; i < s.length(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(s[i]))) {
            hasDigits = true;
        } else if (s[i] == '.') {
            if (hasDecimal) return false; // Only one decimal point allowed
            hasDecimal = true;
        } else {
            return false;
        }
    }
    
    return hasDigits;
}

// Helper to convert a Unicode code point to its UTF-16 surrogate pair representation if necessary
// and format it as JSON \uXXXX escapes.
static void appendJsonUnicodeEscape(std::string& output, uint32_t cp) {
    if (cp <= 0xFFFF) {
        // Basic Multilingual Plane (BMP) characters
        std::ostringstream oss;
        oss << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << cp;
        output += oss.str();
    } else if (cp <= 0x10FFFF) {
        // Supplementary Plane characters (requires surrogate pair)
        // Convert to surrogate pair
        cp -= 0x10000;
        uint32_t high_surrogate = (cp >> 10) + 0xD800;
        uint32_t low_surrogate = (cp & 0x3FF) + 0xDC00;

        std::ostringstream oss;
        oss << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << high_surrogate
            << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << low_surrogate;
        output += oss.str();
    } else {
        // Invalid Unicode code point (e.g., > 0x10FFFF), escape as replacement character
        // This should ideally not be reached if UTF-8 decoding is robust
        std::ostringstream oss;
        oss << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << 0xFFFD; // Unicode replacement character
        output += oss.str();
    }
}

std::string escapeJsonString(const std::string& input) {
    std::string output;
    output.reserve(input.length() * 2); // Optimistic reservation for some expansion

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
                    appendJsonUnicodeEscape(output, c);
                } else if (c <= 0x7E) { // Printable ASCII
                    output += c;
                } else { // Potential multi-byte UTF-8 sequence
                    uint32_t code_point = 0;
                    int num_bytes = 0;

                    // Determine the number of bytes in the UTF-8 sequence
                    if ((c & 0x80) == 0) { // 1-byte sequence (0xxxxxxx) - already handled above (c <= 0x7E)
                        num_bytes = 1;
                        code_point = c;
                    } else if ((c & 0xE0) == 0xC0) { // 2-byte sequence (110xxxxx 10xxxxxx)
                        if (i + 1 < input.length()) {
                            unsigned char c2 = static_cast<unsigned char>(input[i+1]);
                            if ((c2 & 0xC0) == 0x80) { // Check for continuation byte
                                code_point = ((c & 0x1F) << 6) | (c2 & 0x3F);
                                // Overlong encoding check: U+0000 to U+007F should be 1-byte
                                if (code_point >= 0x80) { // Smallest 2-byte char is U+0080
                                    num_bytes = 2;
                                }
                            }
                        }
                    } else if ((c & 0xF0) == 0xE0) { // 3-byte sequence (1110xxxx 10xxxxxx 10xxxxxx)
                        if (i + 2 < input.length()) {
                            unsigned char c2 = static_cast<unsigned char>(input[i+1]);
                            unsigned char c3 = static_cast<unsigned char>(input[i+2]);
                            if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80) { // Check for continuation bytes
                                code_point = ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                                // Overlong encoding check: U+0080 to U+07FF should be 2-byte
                                // U+0800 to U+FFFF is valid for 3-byte, excluding surrogates (D800-DFFF)
                                if (code_point >= 0x800 && (code_point < 0xD800 || code_point > 0xDFFF)) {
                                    num_bytes = 3;
                                }
                            }
                        }
                    } else if ((c & 0xF8) == 0xF0) { // 4-byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
                        if (i + 3 < input.length()) {
                            unsigned char c2 = static_cast<unsigned char>(input[i+1]);
                            unsigned char c3 = static_cast<unsigned char>(input[i+2]);
                            unsigned char c4 = static_cast<unsigned char>(input[i+3]);
                            if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80 && (c4 & 0xC0) == 0x80) { // Check for continuation bytes
                                code_point = ((c & 0x07) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) << 6) | (c4 & 0x3F);
                                // Overlong encoding check: U+0000 to U+FFFF should be 1-3 bytes
                                // Valid range for 4-byte is U+10000 to U+10FFFF
                                if (code_point >= 0x10000 && code_point <= 0x10FFFF) {
                                    num_bytes = 4;
                                }
                            }
                        }
                    }
                    // For invalid sequences, incomplete sequences, or overlong encodings, num_bytes will be 0
                    if (num_bytes > 0) {
                        appendJsonUnicodeEscape(output, code_point);
                        i += (num_bytes - 1); // Advance index by the number of bytes consumed
                    } else {
                        // Invalid UTF-8 start byte or malformed sequence, escape as replacement character
                        appendJsonUnicodeEscape(output, 0xFFFD); // Use Unicode replacement character
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
            case '-': // Escape '-' only if not defining a character range. For glob, it's always literal.
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


