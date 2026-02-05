#ifndef UTILS_VERSION_H
#define UTILS_VERSION_H

#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <algorithm>
#include <iostream> // For temporary debug output

namespace Utils {

struct SemanticVersion {
    unsigned int major = 0;
    unsigned int minor = 0;
    unsigned int patch = 0;
    std::string prerelease; // e.g., "alpha", "beta", "rc.1"
    std::string build;      // e.g., "build.123", "001"

    // Default constructor
    SemanticVersion() = default;

    // Parameterized constructor
    SemanticVersion(unsigned int maj, unsigned int min, unsigned int pat,
                    std::string pre = "", std::string bld = "")
        : major(maj), minor(min), patch(pat), prerelease(std::move(pre)), build(std::move(bld)) {}

    // Comparison operators
    bool operator==(const SemanticVersion& other) const;
    bool operator!=(const SemanticVersion& other) const;
    bool operator<(const SemanticVersion& other) const;
    bool operator<=(const SemanticVersion& other) const;
    bool operator>(const SemanticVersion& other) const;
    bool operator>=(const SemanticVersion& other) const;

    // Utility function to convert to string (optional, for debugging/display)
    std::string toString() const;
};

std::optional<SemanticVersion> parseSemanticVersion(const std::string& versionStr);

} // namespace Utils

#endif // UTILS_VERSION_H
