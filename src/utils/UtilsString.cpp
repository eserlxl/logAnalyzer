#include "utils/Utils.h" // Includes all necessary declarations for Utils namespace
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
    std::string lowerStr = toLower(str);
    std::string lowerFrom = toLower(from);

    size_t start_pos = 0;
    while ((start_pos = lowerStr.find(lowerFrom, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        lowerStr.replace(start_pos, from.length(), to); // Update lowerStr as well
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
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
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

std::string escapeJsonString(const std::string& input) {
    std::ostringstream ss;
    for (char c : input) {
        switch (c) {
            case '"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7e) {
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(c));
                } else {
                    ss << c;
                }
                break;
        }
    }
    return ss.str();
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
