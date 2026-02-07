#include <iostream>
#include <regex>
#include <string>
#include <map>

// Copy-paste of the function under test
void parseStructuredData(const std::string& data, std::map<std::string, std::string>& targetMap, const std::regex& kvPattern) {
    std::sregex_iterator next(data.begin(), data.end(), kvPattern);
    std::sregex_iterator end;
    while (next != end) {
        std::smatch match = *next;
        std::string key = match[1].str();
        std::string value;

        // Check for quoted values (groups 2 and 3) or unquoted (group 4)
        if (match.size() > 2 && match[2].matched) { // Double quotes
            value = match[2].str();
        } else if (match.size() > 3 && match[3].matched) { // Single quotes
            value = match[3].str();
        } else if (match.size() > 4 && match[4].matched) { // Unquoted
            value = match[4].str();
        }
        targetMap[key] = value;
        next++;
    }
}

int main() {
    std::map<std::string, std::string> m;
    
    // Test 1: Fewer groups than expected
    // Pattern: (\w+)=(\w+)  -> Group 1: Key, Group 2: Value.
    // match.size() will be 3 (0, 1, 2).
    // match[2] is matched. match[3] access?
    // In standard C++, accessing match[3] when size is 3 returns unmatched sub_match. It is SAFE.
    // My previous assumption that it was unsafe was cautious, but let's verify if logic holds.
    
    std::string text = "k=v";
    std::regex re("(\\w+)=(\\w+)");
    
    // Simulating the function logic (without the size checks I added in the copy above to be safe, 
    // I want to see if the ORIGINAL logic holds or if I should recommend size checks).
    // The original code:
    /*
        if (match[2].matched) { ... }
        else if (match[3].matched) { ... }
        else if (match[4].matched) { ... }
    */
    
    std::sregex_iterator next(text.begin(), text.end(), re);
    if (next != std::sregex_iterator()) {
        std::smatch match = *next;
        std::cout << "Match size: " << match.size() << std::endl;
        std::cout << "G2 matched: " << match[2].matched << std::endl;
        std::cout << "G3 matched: " << match[3].matched << " (Is safe?)" << std::endl;
        
        if (match[2].matched) std::cout << "Value taken from G2: " << match[2].str() << std::endl;
    }
    
    return 0;
}
