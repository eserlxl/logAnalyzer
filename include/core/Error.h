#pragma once

#include <string>
#include <expected> // For std::expected
#include <stdexcept> // For std::runtime_error

// Moved outside namespace to be globally accessible
enum class Code {
    Unknown,
    InvalidArgument,
    FileNotFound,
    FileNotReadable,
    InvalidRegex,
    MalformedLogEntry,
    InvalidCLIOption,
    StatisticNotFound,
    TimestampParsingFailed, // New error code
    SettingsRestoreFailed, // New error code
    BufferLimitExceeded, // New error code for multi-line buffer overflow
    ConversionError, // New error code for type conversion failures
    JsonParseError, // New error code for JSON parsing failures
    NotImplemented, // New error code for unimplemented features
    FieldNotFound, // New error code for when a log entry field is not found
    Unexpected, // Added Unexpected error code
    // Add more error codes as needed
};

namespace ErrorCode {

// Define a common error structure for the application
struct Error : public std::runtime_error {
    Code code;
    std::string message;
    std::string jsonPath; // Added jsonPath for enhanced error reporting

    // Constructor with message and jsonPath
    Error(Code c, std::string msg, std::string path) : std::runtime_error(msg), code(c), message(std::move(msg)), jsonPath(std::move(path)) {}

    // Constructor with message (jsonPath defaults to empty)
    Error(Code c, std::string msg) : Error(c, std::move(msg), "") {}

    // Default constructor for cases where only code is needed (jsonPath defaults to empty)
    Error(Code c) : Error(c, "Unknown Error", "") {}

    // Override what() method from std::runtime_error
    const char* what() const noexcept override {
        return message.c_str();
    }

    // Static factory for common errors, updated to include jsonPath
    static Error invalidArgument(const std::string& argName, const std::string& path = "") {
        return Error(Code::InvalidArgument, "Invalid argument: " + argName, path);
    }
    static Error fileNotFound(const std::string& filePath, const std::string& path = "") {
        return Error(Code::FileNotFound, "File not found: " + filePath, path);
    }
    static Error fileNotReadable(const std::string& filePath, const std::string& path = "") {
        return Error(Code::FileNotReadable, "File not readable: " + filePath, path);
    }
     static Error invalidCLIOption(const std::string& option, const std::string& path = "") {
        return Error(Code::InvalidCLIOption, "Invalid CLI option: " + option, path);
    }
    static Error statisticNotFound(const std::string& statName, const std::string& path = "") {
        return Error(Code::StatisticNotFound, "Statistic collector not found: " + statName, path);
    }
    static Error timestampParsingFailed(const std::string& details, const std::string& path = "") {
        return Error(Code::TimestampParsingFailed, "Timestamp parsing failed: " + details, path);
    }
    static Error settingsRestoreFailed(const std::string& details, const std::string& path = "") {
        return Error(Code::SettingsRestoreFailed, "Settings restore failed: " + details, path);
    }
    static Error unexpected(const std::string& details, const std::string& path = "") {
        return Error(Code::Unexpected, "Unexpected error: " + details, path);
    }

    bool operator==(const Error& other) const {
        return code == other.code && message == other.message && jsonPath == other.jsonPath;
    }


    // Convert to string for logging or display
    std::string toString() const {
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
            case Code::TimestampParsingFailed: codeStr = "TimestampParsingFailed"; break;
            case Code::SettingsRestoreFailed: codeStr = "SettingsRestoreFailed"; break;
            case Code::BufferLimitExceeded: codeStr = "BufferLimitExceeded"; break;
            case Code::ConversionError: codeStr = "ConversionError"; break;
            case Code::JsonParseError: codeStr = "JsonParseError"; break;
            case Code::NotImplemented: codeStr = "NotImplemented"; break;
            case Code::Unexpected: codeStr = "Unexpected"; break;
            default: codeStr = "UnknownCode"; break;
        }
        std::string fullMessage = codeStr;
        if (!message.empty()) {
            fullMessage += ": " + message;
        }
        if (!jsonPath.empty()) {
            fullMessage += " at JSON path: " + jsonPath;
        }
        return fullMessage;
    }
};

// Overload operator<< for ErrorCode::Error to enable streaming to ostream
inline std::ostream& operator<<(std::ostream& os, const ErrorCode::Error& error) {
    os << error.toString();
    return os;
}

// Type alias for std::expected to simplify function signatures
template<typename T>
using Result = std::expected<T, Error>;

} // namespace ErrorCode
