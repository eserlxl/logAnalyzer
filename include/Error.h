#pragma once

#include <string>
#include <expected> // For std::expected

namespace ErrorCode {

// Define a common error structure for the application
struct Error {
    enum class Code {
        Unknown,
        InvalidArgument,
        FileNotFound,
        FileNotReadable,
        InvalidRegex,
        MalformedLogEntry,
        InvalidCLIOption,
        StatisticNotFound, // For new stats feature
        // Add more error codes as needed
    };

    Code code;
    std::string message;

    // Constructor
    Error(Code c, std::string msg) : code(c), message(std::move(msg)) {}

    // Default constructor for cases where only code is needed
    Error(Code c) : code(c), message("") {}

    // Static factory for common errors
    static Error invalidArgument(const std::string& argName) {
        return Error(Code::InvalidArgument, "Invalid argument: " + argName);
    }
    static Error fileNotFound(const std::string& filePath) {
        return Error(Code::FileNotFound, "File not found: " + filePath);
    }
    static Error fileNotReadable(const std::string& filePath) {
        return Error(Code::FileNotReadable, "File not readable: " + filePath);
    }
     static Error invalidCLIOption(const std::string& option) {
        return Error(Code::InvalidCLIOption, "Invalid CLI option: " + option);
    }
    static Error statisticNotFound(const std::string& statName) {
        return Error(Code::StatisticNotFound, "Statistic collector not found: " + statName);
    }


    // Convert to string for logging or display
    std::string toString() const {
        // This could be expanded to map enum values to human-readable strings
        std::string codeStr;
        switch (code) {
            case Code::Unknown: codeStr = "Unknown"; break;
            case Code::InvalidArgument: codeStr = "InvalidArgument"; break;
            case Code::FileNotFound: codeStr = "FileNotFound"; break;
            case Code::FileNotReadable: codeStr = "FileNotReadable"; break;
            case Code::InvalidRegex: codeStr = "InvalidRegex"; break;
            case Code::MalformedLogEntry: codeStr = "MalformedLogEntry"; break;
            case Code::InvalidCLIOption: codeStr = "InvalidCLIOption"; break;
            case Code::StatisticNotFound: codeStr = "StatisticNotFound"; break;
            default: codeStr = "UnknownCode"; break;
        }
        if (!message.empty()) {
            return codeStr + ": " + message;
        }
        return codeStr;
    }
};

// Type alias for std::expected to simplify function signatures
template<typename T>
using Result = std::expected<T, Error>;

} // namespace ErrorCode
