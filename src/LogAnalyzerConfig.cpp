// This file is part of the LogAnalyzer project.

#include "LogAnalyzerConfig.h" // Contains declarations for the methods defined here

// Includes required for the implementation details
#include "LogTypes.h"
#include "Filter.h"
#include "Exporter.h"
#include "Statistics.h"
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <string_view>
#include <functional> // Required for std::function
#include <utility>    // Required for std::move
#include <expected>   // For std::expected (C++23)
#include <fstream>    // For file operations (fromFile)
#include <regex>      // For regex validation in validate()
#include <algorithm>  // For std::lexicographical_compare in tests (if not already covered)

// Assuming nlohmann/json library is available for JSON parsing/serialization
// If not, this will require a different JSON library or manual parsing.
#include <nlohmann/json.hpp> 

// Defined as in LogAnalyzer.h comment
static constexpr std::string_view DEFAULT_LOG_REGEX_PATTERN_SV = R"(^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) ([A-Z]+): (.*)$)";
static const std::string DEFAULT_LOG_REGEX_PATTERN = std::string(DEFAULT_LOG_REGEX_PATTERN_SV);

// Helper functions for JSON serialization/deserialization
namespace { // Anonymous namespace for internal linkage

// Helper to convert LogLevel enum to string
std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        case LogLevel::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

// Helper to convert string to LogLevel enum
LogLevel stringToLogLevel(const std::string& levelStr) {
    if (levelStr == "TRACE") return LogLevel::TRACE;
    if (levelStr == "DEBUG") return LogLevel::DEBUG;
    if (levelStr == "INFO") return LogLevel::INFO;
    if (levelStr == "WARNING") return LogLevel::WARNING;
    if (levelStr == "ERROR") return LogLevel::ERROR;
    if (levelStr == "CRITICAL") return LogLevel::CRITICAL;
    return LogLevel::UNKNOWN;
}

// End of helper functions moved to respective headers
// ... implementation of LogAnalyzerSettings methods ...
}
