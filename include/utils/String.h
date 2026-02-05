#ifndef UTILS_STRING_H
#define UTILS_STRING_H

#include <string>
#include <string_view>
#include <vector>

namespace Utils {

void replaceAll(std::string &str, const std::string &from, const std::string &to);
void replaceAllIgnoreCase(std::string& str, const std::string& from, const std::string& to);
std::string trim(const std::string& str, std::string_view whitespace = " \t\n\r\f\v");
std::vector<std::string> split(const std::string& str, char delimiter);
std::string toLower(const std::string& str);
std::string toUpper(const std::string& str);
bool caseInsensitiveEquals(const std::string& s1, const std::string& s2);
bool caseInsensitiveSearch(const std::string& text, const std::string& pattern);
bool startsWithIgnoreCase(const std::string& text, const std::string& prefix); // Deprecated: Use caseInsensitiveStarts
bool endsWithIgnoreCase(const std::string& text, const std::string& suffix);   // Deprecated: Use caseInsensitiveEnds
bool caseInsensitiveStarts(const std::string& text, const std::string& prefix);
bool caseInsensitiveEnds(const std::string& text, const std::string& suffix);
std::string escapeJsonString(const std::string& input);
std::string globToRegex(const std::string& globPattern);

} // namespace Utils

#endif // UTILS_STRING_H
