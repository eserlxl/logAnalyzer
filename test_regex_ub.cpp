#include <iostream>
#include <regex>
#include <string>

int main() {
    std::string text = "key=value";
    std::regex re("(\\w+)=(\\w+)"); // Only 2 groups: 0(full), 1(key), 2(value)
    std::smatch match;
    
    if (std::regex_search(text, match, re)) {
        std::cout << "Match size: " << match.size() << std::endl;
        // Accessing index 3, which is >= match.size() (3)
        // Groups are 0, 1, 2. Size is 3.
        try {
            // This is supposedly UB, but let's see if it throws or crashes or returns empty.
            // On some implementations (MSVC), it might assert. On libstdc++, it might segfault or return garbage.
            // Safe code should not do this.
            // But checking if I can detect it.
            if (match.size() <= 3) {
                 std::cout << "Index 3 is out of bounds (size=" << match.size() << ")" << std::endl;
            }
        } catch (...) {
            std::cout << "Caught exception" << std::endl;
        }
    }
    return 0;
}
