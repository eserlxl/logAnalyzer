#include "../include/Utils.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>

namespace Utils {

void replaceAll(std::string &str, const std::string &from, const std::string &to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

void replaceAllIgnoreCase(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return;
    }
    std::string lowerStr = toLower(str);
    std::string lowerFrom = toLower(from);
    size_t start_pos = 0;
    while ((start_pos = lowerStr.find(lowerFrom, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        lowerStr.replace(start_pos, from.length(), toLower(to)); // Keep lowerStr in sync
        start_pos += to.length();
    }
}

std::string trim(const std::string& str, const std::string& whitespace) {
    const size_t strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; // no content

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
                   ::tolower);
    return lowerStr;
}

std::string toUpper(const std::string& str) {
    std::string upperStr = str;
    std::transform(upperStr.begin(), upperStr.end(), upperStr.begin(),
                   ::toupper);
    return upperStr;
}

std::string escapeJsonString(const std::string& input) {
    std::ostringstream oss;
    for (char c : input) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 32) { // Control characters
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    oss << c;
                }
                break;
        }
    }
    return oss.str();
}

std::string globToRegex(const std::string& globPattern) {
    std::string regexPattern = "^"; // Anchor to the start of the string
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
                regexPattern += '\\'; // Escape regex special characters
                regexPattern += c;
                break;
            default:
                regexPattern += c;
                break;
        }
    }
    regexPattern += "$"; // Anchor to the end of the string
    return regexPattern;
}

} // namespace Utils
