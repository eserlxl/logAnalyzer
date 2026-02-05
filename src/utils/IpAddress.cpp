#include "utils/IpAddress.h"
#include <string.h> // For memset

namespace Utils {

std::optional<IpAddress> parseIpAddress(const std::string& ipStr) {
    // Try parsing as IPv4
    std::array<unsigned char, 4> ipv4Bytes;
    if (inet_pton(AF_INET, ipStr.c_str(), ipv4Bytes.data()) == 1) {
        return IpAddress(ipv4Bytes);
    }

    // Try parsing as IPv6
    std::array<unsigned char, 16> ipv6Bytes;
    if (inet_pton(AF_INET6, ipStr.c_str(), ipv6Bytes.data()) == 1) {
        return IpAddress(ipv6Bytes);
    }

    return std::nullopt; // Failed to parse as either IPv4 or IPv6
}

std::string IpAddress::toString() const {
    char str[INET6_ADDRSTRLEN]; // Max length for IPv6 address string
    if (family == AF_INET) {
        if (inet_ntop(AF_INET, std::get<std::array<unsigned char, 4>>(addressBytes).data(), str, INET_ADDRSTRLEN) != nullptr) {
            return str;
        }
    } else if (family == AF_INET6) {
        if (inet_ntop(AF_INET6, std::get<std::array<unsigned char, 16>>(addressBytes).data(), str, INET6_ADDRSTRLEN) != nullptr) {
            return str;
        }
    }
    return ""; // Error or unknown family
}

// Comparison operators
bool IpAddress::operator==(const IpAddress& other) const {
    if (family != other.family) {
        return false;
    }
    if (family == AF_INET) {
        return std::get<std::array<unsigned char, 4>>(addressBytes) == std::get<std::array<unsigned char, 4>>(other.addressBytes);
    } else if (family == AF_INET6) {
        return std::get<std::array<unsigned char, 16>>(addressBytes) == std::get<std::array<unsigned char, 16>>(other.addressBytes);
    }
    return false; // Should not happen for valid IpAddress objects
}

bool IpAddress::operator!=(const IpAddress& other) const {
    return !(*this == other);
}

bool IpAddress::operator<(const IpAddress& other) const {
    if (family != other.family) {
        // IPv4 is considered "less than" IPv6 for comparison purposes if families differ.
        // This is an arbitrary but consistent choice.
        return family == AF_INET;
    }
    
    if (family == AF_INET) {
        return std::get<std::array<unsigned char, 4>>(addressBytes) < std::get<std::array<unsigned char, 4>>(other.addressBytes);
    } else if (family == AF_INET6) {
        return std::get<std::array<unsigned char, 16>>(addressBytes) < std::get<std::array<unsigned char, 16>>(other.addressBytes);
    }
    return false; // Should not happen
}

bool IpAddress::operator<=(const IpAddress& other) const {
    return (*this < other) || (*this == other);
}

bool IpAddress::operator>(const IpAddress& other) const {
    return !(*this <= other);
}

bool IpAddress::operator>=(const IpAddress& other) const {
    return !(*this < other);
}

} // namespace Utils
