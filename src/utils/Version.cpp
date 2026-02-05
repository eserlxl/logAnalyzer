// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 eserlxl

#include "utils/Version.h"
#include "utils/String.h" // For Utils::split
#include <regex>
#include <charconv> // For std::from_chars in C++17

namespace Utils {

// Helper to parse a version component (major, minor, patch)
static std::optional<unsigned int> parseComponent(const std::string& s) {
    if (s.empty()) {
        return 0; // Treat empty as 0 for numeric components
    }
    unsigned int value;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec == std::errc() && ptr == s.data() + s.size()) {
        return value;
    }
    return std::nullopt;
}

// Helper for comparing prerelease identifiers
static int comparePrerelease(const std::string& pr1, const std::string& pr2) {
    if (pr1.empty() && pr2.empty()) return 0;
    if (pr1.empty()) return 1; // No prerelease is greater than prerelease
    if (pr2.empty()) return -1; // Prerelease is less than no prerelease

    auto split1 = Utils::split(pr1, '.');
    auto split2 = Utils::split(pr2, '.');

    for (size_t i = 0; i < std::min(split1.size(), split2.size()); ++i) {
        // Numeric identifiers have lower precedence than non-numeric
        bool isNum1 = std::all_of(split1[i].begin(), split1[i].end(), ::isdigit);
        bool isNum2 = std::all_of(split2[i].begin(), split2[i].end(), ::isdigit);

        if (isNum1 && isNum2) {
            unsigned int num1 = 0, num2 = 0;
            std::from_chars(split1[i].data(), split1[i].data() + split1[i].size(), num1);
            std::from_chars(split2[i].data(), split2[i].data() + split2[i].size(), num2);
            if (num1 < num2) return -1;
            if (num1 > num2) return 1;
        } else if (isNum1) { // num1 is numeric, num2 is not (numeric is lower)
            return -1;
        } else if (isNum2) { // num2 is numeric, num1 is not (numeric is lower)
            return 1;
        } else { // Both are non-numeric strings
            int cmp = split1[i].compare(split2[i]);
            if (cmp < 0) return -1;
            if (cmp > 0) return 1;
        }
    }

    if (split1.size() < split2.size()) return -1; // Shorter prerelease has lower precedence
    if (split1.size() > split2.size()) return 1;

    return 0; // Equal
}

// Comparison operators
bool SemanticVersion::operator==(const SemanticVersion& other) const {
    return major == other.major &&
           minor == other.minor &&
           patch == other.patch &&
           prerelease == other.prerelease &&
           build == other.build; // According to SemVer, build metadata is ignored for comparison
                                // but for strict equality we can include it.
}

bool SemanticVersion::operator!=(const SemanticVersion& other) const {
    return !(*this == other);
}

bool SemanticVersion::operator<(const SemanticVersion& other) const {
    if (major < other.major) return true;
    if (major > other.major) return false;

    if (minor < other.minor) return true;
    if (minor > other.minor) return false;

    if (patch < other.patch) return true;
    if (patch > other.patch) return false;

    // Prerelease has lower precedence than a normal version
    int prCmp = comparePrerelease(prerelease, other.prerelease);
    if (prCmp == -1) return true;
    if (prCmp == 1) return false;

    // Build metadata is ignored for precedence comparison as per SemVer 2.0.0
    return false;
}

bool SemanticVersion::operator<=(const SemanticVersion& other) const {
    return (*this < other) || (*this == other);
}

bool SemanticVersion::operator>(const SemanticVersion& other) const {
    return !(*this <= other);
}

bool SemanticVersion::operator>=(const SemanticVersion& other) const {
    return !(*this < other);
}

// Utility function to convert to string
std::string SemanticVersion::toString() const {
    std::string s = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    if (!prerelease.empty()) {
        s += "-" + prerelease;
    }
    if (!build.empty()) {
        s += "+" + build;
    }
    return s;
}

std::optional<SemanticVersion> parseSemanticVersion(const std::string& versionStr) {
    // SemVer regex: ^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-((?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+([0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$
    // Simplified for common cases. This regex ensures:
    // - Major, minor, patch are non-negative integers. Leading zeros are only allowed for '0'.
    // - Prerelease: alpha-numeric, can be dot-separated. Numeric parts of prerelease can have leading zeros.
    // - Build: alpha-numeric, can be dot-separated.
    static const std::regex semverRegex(
        R"(^(\d+)\.(\d+)\.(\d+)(?:-([0-9a-zA-Z\.-]+))?(?:\+([0-9a-zA-Z\.-]+))?$)"
    );

    std::smatch matches;
    if (std::regex_match(versionStr, matches, semverRegex)) {
        if (matches.size() >= 4) { // Minimum: major.minor.patch
            auto major = parseComponent(matches[1].str());
            auto minor = parseComponent(matches[2].str());
            auto patch = parseComponent(matches[3].str());

            if (!major.has_value() || !minor.has_value() || !patch.has_value()) {
                // This shouldn't happen with the current regex if matches are found,
                // but as a safeguard.
                return std::nullopt;
            }

            SemanticVersion sv(*major, *minor, *patch);
            if (matches.size() >= 5 && matches[4].matched) {
                sv.prerelease = matches[4].str();
            }
            if (matches.size() >= 6 && matches[5].matched) {
                sv.build = matches[5].str();
            }
            return sv;
        }
    }
    return std::nullopt;
}

} // namespace Utils
