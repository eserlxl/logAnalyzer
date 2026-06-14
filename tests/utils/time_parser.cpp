// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

// Focused tests for the format-driven and CLI-facing entry points in
// src/utils/time_parser.cpp that had no direct coverage: parseTimeWithFormats
// (custom strptime-style formats, fallback chain, failure, empty-formats
// delegation), validateTimestampCliOption (the CLI validator wrapper), and the
// empty-input error paths of parseTime and parseDuration.

#include "gtest/gtest.h"
#include "utils/time.h"

#include <chrono>
#include <ctime>
#include <string>
#include <vector>

namespace {

// Build the time_point parseTimeWithFormats is expected to produce for a broken-
// down local time. Mirrors the code's path (std::mktime with tm_isdst = -1), so
// the comparison holds regardless of the host timezone.
std::chrono::system_clock::time_point localTimePoint(int year, int mon, int day,
                                                      int hour, int min, int sec) {
    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = mon - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    tm.tm_isdst = -1;
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

TEST(TimeParserWithFormats, SingleValidFormatParses) {
    auto result = Utils::parseTimeWithFormats("2023-01-15 13:05:09", {"%Y-%m-%d %H:%M:%S"});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), localTimePoint(2023, 1, 15, 13, 5, 9));
}

TEST(TimeParserWithFormats, FallsBackThroughFormatsUntilOneMatches) {
    // The first format cannot parse a date-only string; the second can.
    auto result = Utils::parseTimeWithFormats("2023-06-20", {"%H:%M:%S", "%Y-%m-%d"});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), localTimePoint(2023, 6, 20, 0, 0, 0));
}

TEST(TimeParserWithFormats, AllFormatsFailReturnsError) {
    auto result = Utils::parseTimeWithFormats("not-a-date", {"%Y-%m-%d", "%H:%M:%S"});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message,
              "Failed to parse time with provided formats: not-a-date");
}

TEST(TimeParserWithFormats, EmptyFormatsDelegatesToParseTime) {
    // With no formats, the function falls back to the general parseTime path.
    const std::string input = "1700000000"; // unix seconds, accepted by parseTime
    auto viaFormats = Utils::parseTimeWithFormats(input, {});
    auto direct = Utils::parseTime(input);
    ASSERT_TRUE(direct.has_value());
    ASSERT_TRUE(viaFormats.has_value());
    EXPECT_EQ(viaFormats.value(), direct.value());
}

TEST(ValidateTimestampCliOption, AcceptsValidAndRejectsInvalid) {
    EXPECT_TRUE(Utils::validateTimestampCliOption("1700000000").has_value());

    auto bad = Utils::validateTimestampCliOption("definitely not a timestamp");
    EXPECT_FALSE(bad.has_value());
}

TEST(ParseTimeEdgeCases, WhitespaceOnlyReportsEmptyValue) {
    auto result = Utils::parseTime("   ");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "Empty time value.");
}

TEST(ParseDurationEdgeCases, EmptyStringReportsEmptyDuration) {
    auto result = Utils::parseDuration("", false);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "Empty duration string.");
}

} // namespace
