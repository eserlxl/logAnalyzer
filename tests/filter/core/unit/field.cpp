// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "filter/Core.h"
#include "filter/Condition.h" // Added
#include "filter/Types.h" // Added
#include "filter/ConcreteFilters.h" // Added
#include "core/Log/Types.h"
#include <chrono>
#include <map>
#include <sstream> // Required for std::stringstream for general string manipulation if needed
#include <nlohmann/json.hpp> // New include for JSON testing (might be needed for some types)

using namespace filter;

// Test fixture for creating LogEntry objects
class FilterTest : public ::testing::Test {
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

TEST_F(FilterTest, FieldExistsFilter) {
    FieldExistsFilter filter("user_id");
    auto entry1 = createLogEntry(1, "app.log", now, LogLevel::INFO, "User logged in", {{"user_id", "123"}, {"session", "xyz"}});
    auto entry2 = createLogEntry(2, "app.log", now, LogLevel::WARNING, "User not found", {{"session", "abc"}});
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2));
}

TEST_F(FilterTest, FieldValueFilter) {
    FieldValueFilter filter("status_code", "404", PatternType::Literal);
    auto entry1 = createLogEntry(1, "web.log", now, LogLevel::WARNING, "Not found", {{"status_code", "404"}});
    auto entry2 = createLogEntry(2, "web.log", now, LogLevel::INFO, "OK", {{"status_code", "200"}});
    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2));

    FieldValueFilter filter_icase("status_code", "NotFound", PatternType::Literal, false);
    auto entry3 = createLogEntry(3, "web.log", now, LogLevel::WARNING, "Not found", {{"status_code", "notfound"}});
    EXPECT_TRUE(filter_icase.matches(entry3));
}


TEST_F(FilterTest, FieldValueFilterWildcardSubstringMatch) {
    FieldValueFilter filter("user_id", "admin*", PatternType::Wildcard);
    auto entry1 = createLogEntry(1, "app.log", now, LogLevel::INFO, "msg", {{"user_id", "admin123"}});
    auto entry2 = createLogEntry(2, "app.log", now, LogLevel::INFO, "msg", {{"user_id", "superuser"}});
    auto entry3 = createLogEntry(3, "app.log", now, LogLevel::INFO, "msg", {{"user_id", "guest"}});
    auto entry4 = createLogEntry(4, "app.log", now, LogLevel::INFO, "msg", {{"user_id", "system-admin"}}); // Should match "admin" as substring

    EXPECT_TRUE(filter.matches(entry1));
    EXPECT_FALSE(filter.matches(entry2));
    EXPECT_FALSE(filter.matches(entry3));
    EXPECT_FALSE(filter.matches(entry4)); // "system-admin" does not start with "admin"
}

TEST_F(FilterTest, ValueSetFilter) {
    std::set<std::string> allowed_users = {"admin", "guest", "root"};
    ValueSetFilter filter("user", allowed_users, true); // Case-sensitive

    EXPECT_TRUE(filter.matches(createLogEntry(1, "log", now, LogLevel::INFO, "msg", {{"user", "admin"}})));
    EXPECT_TRUE(filter.matches(createLogEntry(2, "log", now, LogLevel::INFO, "msg", {{"user", "guest"}})));
    EXPECT_FALSE(filter.matches(createLogEntry(3, "log", now, LogLevel::INFO, "msg", {{"user", "Admin"}}))); // Case-sensitive mismatch
    EXPECT_FALSE(filter.matches(createLogEntry(4, "log", now, LogLevel::INFO, "msg", {{"user", "dev"}})));
    EXPECT_FALSE(filter.matches(createLogEntry(5, "log", now, LogLevel::INFO, "msg", {{"role", "admin"}}))); // Wrong field

    std::set<std::string> allowed_statuses = {"OK", "ERROR"};
    ValueSetFilter filter_icase("status", allowed_statuses, false); // Case-insensitive

    EXPECT_TRUE(filter_icase.matches(createLogEntry(6, "log", now, LogLevel::INFO, "msg", {{"status", "ok"}})));
    EXPECT_TRUE(filter_icase.matches(createLogEntry(7, "log", now, LogLevel::INFO, "msg", {{"status", "ERROR"}})));
    EXPECT_FALSE(filter_icase.matches(createLogEntry(8, "log", now, LogLevel::INFO, "msg", {{"status", "pending"}})));
}

TEST_F(FilterTest, DottedKeyValueSetFilter) {
    std::set<std::string> allowed_roles = {"superadmin", "moderator"};
    DottedKeyValueSetFilter filter("user.role", allowed_roles, true); // Case-sensitive

    EXPECT_TRUE(filter.matches(createLogEntry(1, "log", now, LogLevel::INFO, "msg", {{"user.role", "superadmin"}})));
    EXPECT_FALSE(filter.matches(createLogEntry(2, "log", now, LogLevel::INFO, "msg", {{"user.role", "SuperAdmin"}}))); // Case-sensitive mismatch
    EXPECT_FALSE(filter.matches(createLogEntry(3, "log", now, LogLevel::INFO, "msg", {{"user.role", "viewer"}})));
    EXPECT_FALSE(filter.matches(createLogEntry(4, "log", now, LogLevel::INFO, "msg", {{"account.role", "superadmin"}}))); // Wrong field path

    std::set<std::string> allowed_events = {"LOGIN", "LOGOUT"};
    DottedKeyValueSetFilter filter_icase("event.type", allowed_events, false); // Case-insensitive

    EXPECT_TRUE(filter_icase.matches(createLogEntry(5, "log", now, LogLevel::INFO, "msg", {{"event.type", "login"}})));
    EXPECT_TRUE(filter_icase.matches(createLogEntry(6, "log", now, LogLevel::INFO, "msg", {{"event.type", "LOGOUT"}})));
    EXPECT_FALSE(filter_icase.matches(createLogEntry(7, "log", now, LogLevel::INFO, "msg", {{"event.type", "failed_login"}})));
}
