#include "utils/String.h" // Explicitly include String.h
#include "utils/Core.h" // Includes all necessary declarations for Utils namespace
#include <algorithm>
#include <string>
#include <sstream> // For std::ostringstream in escapeJsonString
#include <iomanip> // For std::hex in escapeJsonString
#include <vector>
#include <regex>

namespace Utils {

void replaceAll(std::string &str, const std::string &from, const std::string &to) {
    if (from.empty()) return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' contains 'from'
    }
}

void replaceAllIgnoreCase(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) return;

    std::string lowerFrom = toLower(from);
    size_t start_pos = 0;
    while (true) {
        std::string lowerStr = toLower(str); // Recalculate lowerStr in each iteration
        start_pos = lowerStr.find(lowerFrom, start_pos);
        if (start_pos == std::string::npos) {
            break;
        }
        str.replace(start_pos, from.length(), to);
        // Important: Do NOT modify lowerStr directly with 'to' as it might not be lowercase
        // The next iteration will recalculate lowerStr from the updated 'str'
        start_pos += to.length();
    }
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
    return toLower(s1) == toLower(s2);
}

bool caseInsensitiveSearch(const std::string& text, const std::string& pattern) {
    std::string lowerText = toLower(text);
    std::string lowerPattern = toLower(pattern);
    return lowerText.find(lowerPattern) != std::string::npos;
}

[[deprecated("Use caseInsensitiveStarts instead.")]]
bool startsWithIgnoreCase(const std::string& text, const std::string& prefix) {
    if (prefix.length() > text.length()) return false;
    return toLower(text.substr(0, prefix.length())) == toLower(prefix);
}

[[deprecated("Use caseInsensitiveEnds instead.")]]
bool endsWithIgnoreCase(const std::string& text, const std::string& suffix) {
    if (suffix.length() > text.length()) return false;
    return toLower(text.substr(text.length() - suffix.length())) == toLower(suffix);
}

bool caseInsensitiveStarts(const std::string& text, const std::string& prefix) {
    return startsWithIgnoreCase(text, prefix);
}

bool caseInsensitiveEnds(const std::string& text, const std::string& suffix) {
    return endsWithIgnoreCase(text, suffix);
}

std::string escapeJsonString(const std::string& input) {
    std::string output;
    output.reserve(input.length()); // Reserve at least the original length
    for (char c : input) {
        switch (c) {
            case '"':  output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7e) {
                    std::ostringstream ss;
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(c));
                    output += ss.str();
                } else {
                    output += c;
                }
                break;
        }
    }
    return output;
}

std::string globToRegex(const std::string& globPattern) {
    std::string regexPattern;
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
    return regexPattern;
}

} // namespace Utils

