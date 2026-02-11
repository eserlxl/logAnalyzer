#include <iostream>
#include <string>
#include <regex>
#include <map>

void test_regex_behavior(const std::string& message) {
    static const std::regex kvPattern("([a-zA-Z0-9_.-]+)\\s*=\\s*(?:\\"(.*?)\\"|'([^']*)'|([^\\s,]*?(?:(?=[a-zA-Z0-9_.-]+\\s*=)|$)))[, ]*", std::regex::optimize);
    std::sregex_iterator next(message.begin(), message.end(), kvPattern);
    std::sregex_iterator end;

    std::map<std::string, std::string> results;

    std::cout << "--- Testing: \"" << message << "\" ---" << std::endl;
    int match_count = 0;
    while (next != end) {
        match_count++;
        const std::smatch& match = *next;
        std::cout << "  Full Match " << match_count << ": \"" << match.str() << "\"" << std::endl;
        
        // Print all groups, including empty ones, and their 'matched' status
        for (size_t i = 0; i < match.size(); ++i) {
            std::cout << "    Group " << i << ": \"" << match[i].str() << "\" (matched: " << match[i].matched << ")" << std::endl;
        }

        if (match.size() < 2) {
            std::cout << "    Warning: Match has less than 2 groups, skipping." << std::endl;
            ++next;
            continue;
        }

        std::string key = match[1].str();
        std::string value;
        bool value_matched = false;
        // Iterate through all capture groups starting from index 2 to find the value
        // Selects the first non-empty capturing group after the key's capturing group
        for (size_t i = 2; i < match.size(); ++i) {
            if (match[i].matched) { // Check if the group actually participated in the match
                value = match[i].str();
                value_matched = true;
                break;
            }
        }
        
        if (value_matched) {
            results[key] = value;
            std::cout << "    Extracted: Key=\"" << key << "\", Value=\"" << value << "\"" << std::endl;
        } else {
            std::cout << "    Extracted: Key=\"" << key << "\", Value=\"\" (no value group matched)" << std::endl;
        }
        ++next;
    }
    if (match_count == 0) {
        std::cout << "  No matches found." << std::endl;
    }
    std::cout << "  Final results map size: " << results.size() << std::endl;
    for (const auto& pair : results) {
        std::cout << "    Map entry: \"" << pair.first << "\" -> \"" << pair.second << "\"" << std::endl;
    }
    std::cout << "------------------------------------------" << std::endl << std::endl;
}

int main() {
    test_regex_behavior("key1=\"value1 key2=value2");
    test_regex_behavior("key1=\"value1\"");
    test_regex_behavior("key1=value1");
    test_regex_behavior("key1='value1'");
    test_regex_behavior("key1=\"\" key2=''" );
    test_regex_behavior("key1=\"value with spaces\"");
    test_regex_behavior("key1=\"value!@#$%^&*()\"");
    test_regex_behavior("key1=v1 key1=v2");
    test_regex_behavior("key1=value1 key2=value2");
    test_regex_behavior("key1=\"value1 key2=value2 key3=value3\""); // Malformed, but longer
    return 0;
}