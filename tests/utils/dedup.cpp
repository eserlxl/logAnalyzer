// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "utils/dedup.h"
#include "core/log/types.h"
#include <gtest/gtest.h>
#include <vector>

class DedupFieldTest : public ::testing::Test {
protected:
    static LogEntry makeEntry(LogLevel level, const std::string& msg, const std::string& src = "f.cpp",
                              std::map<std::string, std::string> custom = {}) {
        LogEntry e;
        e.level = level;
        e.message = msg;
        e.sourceFile = src;
        e.customFields = std::move(custom);
        return e;
    }
};

TEST_F(DedupFieldTest, DedupByMessage) {
    std::vector<LogEntry> entries = {
        makeEntry(LogLevel::INFO, "hello"),
        makeEntry(LogLevel::INFO, "world"),
        makeEntry(LogLevel::WARNING, "hello"),
        makeEntry(LogLevel::ERROR, "world"),
        makeEntry(LogLevel::INFO, "unique"),
    };
    Utils::applyDedupField(entries, "message");
    ASSERT_EQ(entries.size(), 3u);
    ASSERT_EQ(entries[0].message, "hello");
    ASSERT_EQ(entries[1].message, "world");
    ASSERT_EQ(entries[2].message, "unique");
}

TEST_F(DedupFieldTest, DedupByLevel) {
    std::vector<LogEntry> entries = {
        makeEntry(LogLevel::INFO, "a"),
        makeEntry(LogLevel::INFO, "b"),
        makeEntry(LogLevel::ERROR, "c"),
    };
    Utils::applyDedupField(entries, "level");
    ASSERT_EQ(entries.size(), 2u);
    ASSERT_EQ(entries[0].level, LogLevel::INFO);
    ASSERT_EQ(entries[1].level, LogLevel::ERROR);
}

TEST_F(DedupFieldTest, DedupByCustomField) {
    std::vector<LogEntry> entries = {
        makeEntry(LogLevel::INFO, "a", "f.cpp", {{"session", "s1"}}),
        makeEntry(LogLevel::INFO, "b", "f.cpp", {{"session", "s2"}}),
        makeEntry(LogLevel::INFO, "c", "f.cpp", {{"session", "s1"}}),
    };
    Utils::applyDedupField(entries, "session");
    ASSERT_EQ(entries.size(), 2u);
    ASSERT_EQ(entries[0].customFields.at("session"), "s1");
    ASSERT_EQ(entries[1].customFields.at("session"), "s2");
}

TEST_F(DedupFieldTest, AbsentCustomFieldKeepsAll) {
    std::vector<LogEntry> entries = {
        makeEntry(LogLevel::INFO, "a"),
        makeEntry(LogLevel::INFO, "b"),
        makeEntry(LogLevel::INFO, "c"),
    };
    Utils::applyDedupField(entries, "no_such_field");
    ASSERT_EQ(entries.size(), 3u);
}

TEST_F(DedupFieldTest, EmptyFieldIsNoOp) {
    std::vector<LogEntry> entries = {
        makeEntry(LogLevel::INFO, "x"),
        makeEntry(LogLevel::INFO, "x"),
    };
    Utils::applyDedupField(entries, "");
    ASSERT_EQ(entries.size(), 2u);
}

TEST_F(DedupFieldTest, GetDedupKeyLevel) {
    LogEntry e = makeEntry(LogLevel::ERROR, "msg");
    size_t idx = 0;
    ASSERT_EQ(Utils::getDedupKey(e, "level", idx), "ERROR");
    ASSERT_EQ(idx, 0u);
}

TEST_F(DedupFieldTest, GetDedupKeyMessage) {
    LogEntry e = makeEntry(LogLevel::INFO, "hello world");
    size_t idx = 0;
    ASSERT_EQ(Utils::getDedupKey(e, "message", idx), "hello world");
}

TEST_F(DedupFieldTest, GetDedupKeyCustom) {
    LogEntry e = makeEntry(LogLevel::INFO, "x", "f.cpp", {{"txid", "abc123"}});
    size_t idx = 0;
    ASSERT_EQ(Utils::getDedupKey(e, "txid", idx), "abc123");
}

TEST_F(DedupFieldTest, GetDedupKeyAbsent) {
    LogEntry e = makeEntry(LogLevel::INFO, "x");
    size_t idx = 5;
    std::string key = Utils::getDedupKey(e, "missing", idx);
    ASSERT_EQ(key, "__absent__5");
    ASSERT_EQ(idx, 6u);
}

TEST_F(DedupFieldTest, GetDedupKeySourceAlias) {
    LogEntry e = makeEntry(LogLevel::INFO, "msg", "server.cpp");
    size_t idx = 0;
    ASSERT_EQ(Utils::getDedupKey(e, "source", idx), "server.cpp");
    ASSERT_EQ(Utils::getDedupKey(e, "source_file", idx), "server.cpp");
}

TEST_F(DedupFieldTest, DedupBySourceFileAlias) {
    std::vector<LogEntry> entries = {
        makeEntry(LogLevel::INFO, "a", "alpha.cpp"),
        makeEntry(LogLevel::INFO, "b", "alpha.cpp"),
        makeEntry(LogLevel::INFO, "c", "beta.cpp"),
    };
    Utils::applyDedupField(entries, "source_file");
    ASSERT_EQ(entries.size(), 2u);
    ASSERT_EQ(entries[0].sourceFile, "alpha.cpp");
    ASSERT_EQ(entries[1].sourceFile, "beta.cpp");
}
