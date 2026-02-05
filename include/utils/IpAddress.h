#ifndef UTILS_IP_ADDRESS_H
#define UTILS_IP_ADDRESS_H

#include <string>
#include <vector>
#include <optional>
#include <array>
#include <variant> // For std::variant
#include <stdexcept>
#include <iostream> // For temporary debug output

// Include for inet_pton and inet_ntop
#include <arpa/inet.h>
#include <sys/socket.h>

namespace Utils {

struct IpAddress {
    // Using std::array to store raw byte representation of IP addresses
    // IPv4 address length is 4 bytes, IPv6 is 16 bytes
    std::variant<std::array<unsigned char, 4>, std::array<unsigned char, 16>> addressBytes;
    int family = AF_UNSPEC; // AF_INET for IPv4, AF_INET6 for IPv6

    IpAddress() = default;

    // Constructor for IPv4
    explicit IpAddress(std::array<unsigned char, 4> bytes) : addressBytes(bytes), family(AF_INET) {}
    // Constructor for IPv6
    explicit IpAddress(std::array<unsigned char, 16> bytes) : addressBytes(bytes), family(AF_INET6) {}

    // Check if the address is IPv4
    bool isIPv4() const { return family == AF_INET; }
    // Check if the address is IPv6
    bool isIPv6() const { return family == AF_INET6; }

    // Comparison operators
    bool operator==(const IpAddress& other) const;
    bool operator!=(const IpAddress& other) const;
    bool operator<(const IpAddress& other) const;
    bool operator<=(const IpAddress& other) const;
    bool operator>(const IpAddress& other) const;
    bool operator>=(const IpAddress& other) const;

    // Utility function to convert to string
    std::string toString() const;
};

std::optional<IpAddress> parseIpAddress(const std::string& ipStr);

} // namespace Utils

#endif // UTILS_IP_ADDRESS_H
