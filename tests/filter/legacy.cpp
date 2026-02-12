// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "helper.h"
#include "filter/concrete_filters.h"

using namespace filter;

// --- Legacy Filter Tests (maintained for backward compatibility) ---
class LegacyFilterTest : public ::testing::Test {
protected:
    LogEntry createLogEntry(
        size_t id,
        const std::string& sourceFile,
        std::chrono::system_clock::time_point timestamp,
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

    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
};


TEST_F(LegacyFilterTest, CompositeFilterEmptyListAND) {
    CompositeFilter filter(CompositeFilter::Logic::AND);
    auto entry = createLogEntry(1, "web.log", now, LogLevel::INFO, "Any message");
    EXPECT_TRUE(filter.matches(entry)); // Empty AND should always return true
}

TEST_F(LegacyFilterTest, CompositeFilterEmptyListOR) {
    CompositeFilter filter(CompositeFilter::Logic::OR);
    auto entry = createLogEntry(1, "web.log", now, LogLevel::INFO, "Any message");
    EXPECT_FALSE(filter.matches(entry)); // Empty OR should always return false
}

TEST_F(LegacyFilterTest, ExclusionFilter) {
    auto base_filter = std::make_shared<FieldExistsFilter>("error_code");
    ExclusionFilter filter(base_filter);

    auto entry1 = createLogEntry(1, "app.log", now, LogLevel::ERROR, "An error occurred", {{"error_code", "E101"}});
    auto entry2 = createLogEntry(2, "app.log", now, LogLevel::INFO, "Operation successful", {{"status", "OK"}});

    EXPECT_FALSE(filter.matches(entry1)); // error_code exists, so exclusion filter should return false
    EXPECT_TRUE(filter.matches(entry2));  // error_code does not exist, so exclusion filter should return true
}
