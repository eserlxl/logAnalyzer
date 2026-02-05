#include "utils/Time.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Helper function to create a LogEntry for tests
LogEntry createLogEntry(
    size_t id,
    const std::string& sourceFile,
    const std::chrono::system_clock::time_point& timestamp,
    LogLevel level,
    const std::string& message,
    const std::map<std::string, std::string>& customFields = {}
) {
    LogEntry entry;
    entry.id = id;
    entry.sourceFile = sourceFile;
    entry.timestamp = timestamp;
    entry.level = level;
    entry.message = message;
    entry.customFields = customFields;
    return entry;
}
