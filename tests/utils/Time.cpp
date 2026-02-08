// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "gtest/gtest.h"
#include "utils/Core.h"
#include <string>
#include <chrono>
#include <optional>

// Helper function to create a time point with a specific date and time
std::chrono::system_clock::time_point createTimePoint(int year, int month, int day, int hour, int minute, int second) {
    std::tm tm = {};
    tm.tm_year = year - 1900; // Years since 1900
    tm.tm_mon = month - 1;    // Months since January
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    tm.tm_isdst = -1; // Let mktime determine DST
    std::time_t tt = std::mktime(&tm);
    if (tt == (std::time_t)-1) {
        throw std::runtime_error("Failed to create time_t for test.");
    }
    return std::chrono::system_clock::from_time_t(tt);
}

TEST(UtilsTime, ParseDuration) {
    using namespace Utils;

    // Test valid durations
    EXPECT_EQ(parseDuration("10s", false).value(), std::chrono::seconds(10));
    EXPECT_EQ(parseDuration("5m", false).value(), std::chrono::minutes(5));
    EXPECT_EQ(parseDuration("2h", false).value(), std::chrono::hours(2));
    EXPECT_EQ(parseDuration("1d", false).value(), std::chrono::days(1));

    // Test extended units (ms, us, w, M, y)
    EXPECT_EQ(parseDuration("500ms", true).value(), std::chrono::milliseconds(500));
    EXPECT_EQ(parseDuration("999ms", true).value(), std::chrono::milliseconds(999));
    EXPECT_EQ(parseDuration("1000ms", true).value(), std::chrono::seconds(1));
    EXPECT_EQ(parseDuration("500us", true).value(), std::chrono::microseconds(500));
    EXPECT_EQ(parseDuration("1w", true).value(), std::chrono::days(7));
    EXPECT_EQ(parseDuration("1M", true).value(), std::chrono::days(30)); // Approximate month
    EXPECT_EQ(parseDuration("1y", true).value(), std::chrono::days(365)); // Approximate year

    // Test invalid formats
    EXPECT_FALSE(parseDuration("10x", false).has_value());
    EXPECT_FALSE(parseDuration("s", false).has_value());
    EXPECT_FALSE(parseDuration("1000", false).has_value());
    EXPECT_FALSE(parseDuration("10s extra", false).has_value());

    // Test stoll out_of_range
    // Assuming a very large number that would exceed long long capacity
    // This might need adjustment based on actual LLONG_MAX, but testing the concept
    std::string max_ll_str(std::to_string(LLONG_MAX));
    std::string too_large_str = max_ll_str + "0";
    EXPECT_FALSE(parseDuration(too_large_str + "s", false).has_value()); // Should fail due to out_of_range

    // Test overflow during unit conversion for extended units.
    const long long maxMicros = std::chrono::microseconds::max().count();
    const long long microsPerWeek = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::weeks(1)).count();
    const long long overflowingWeeks = (maxMicros / microsPerWeek) + 1;
    EXPECT_FALSE(parseDuration(std::to_string(overflowingWeeks) + "w", true).has_value());
    
    // Test extended units disabled
    EXPECT_FALSE(parseDuration("500ms", false).has_value());
    EXPECT_FALSE(parseDuration("1w", false).has_value());
}

TEST(UtilsTime, ParseRelativeTime) {
    using namespace Utils;

    auto now = std::chrono::system_clock::now();

    // Test "X units ago"
    EXPECT_LE((parseRelativeTime("10s ago").value() - now).count(), -10);
    EXPECT_LE((parseRelativeTime("5m ago").value() - now).count(), -300);
    EXPECT_LE((parseRelativeTime("2h ago").value() - now).count(), -7200);
    EXPECT_LE((parseRelativeTime("1d ago").value() - now).count(), -86400);
    
    // Test "in X units"
    EXPECT_GE((parseRelativeTime("in 10s").value() - now).count(), 10);
    EXPECT_GE((parseRelativeTime("in 5m").value() - now).count(), 300);
    EXPECT_GE((parseRelativeTime("in 2h").value() - now).count(), 7200);
    EXPECT_GE((parseRelativeTime("in 1d").value() - now).count(), 86400);

    // Test special relative times
    auto yesterday = now - std::chrono::days(1);
    auto tomorrow = now + std::chrono::days(1);
    
    auto parse_yesterday = parseRelativeTime("yesterday").value();
    EXPECT_EQ(std::chrono::duration_cast<std::chrono::seconds>(parse_yesterday.time_since_epoch()).count(), std::chrono::duration_cast<std::chrono::seconds>(yesterday.time_since_epoch()).count());

    auto parse_tomorrow = parseRelativeTime("tomorrow").value();
    EXPECT_EQ(std::chrono::duration_cast<std::chrono::seconds>(parse_tomorrow.time_since_epoch()).count(), std::chrono::duration_cast<std::chrono::seconds>(tomorrow.time_since_epoch()).count());

    // Test invalid formats
    EXPECT_FALSE(parseRelativeTime("10x ago").has_value());
    EXPECT_FALSE(parseRelativeTime("in s").has_value());
    EXPECT_FALSE(parseRelativeTime("yesterday ").has_value()); // Trailing space
    EXPECT_FALSE(parseRelativeTime("  tomorrow").has_value()); // Leading space
    EXPECT_FALSE(parseRelativeTime("last week").has_value());
}

TEST(UtilsTime, ParseAbsoluteTime) {
    using namespace Utils;

    // Test valid absolute time
    auto tp = parseAbsoluteTime("2023-10-27 10:30:00").value();
    auto expected_tp = createTimePoint(2023, 10, 27, 10, 30, 0);
    EXPECT_EQ(tp, expected_tp);

    // Test invalid format
    EXPECT_FALSE(parseAbsoluteTime("27-10-2023 10:30:00").has_value());
    EXPECT_FALSE(parseAbsoluteTime("2023/10/27 10:30:00").has_value());
    EXPECT_FALSE(parseAbsoluteTime("2023-10-27T10:30:00").has_value()); // ISO format, should be handled by parseISO8601

    // Test invalid date (e.g., Feb 30)
    EXPECT_FALSE(parseAbsoluteTime("2023-02-30 10:00:00").has_value());
}

TEST(UtilsTime, ParseISO8601) {
    using namespace Utils;

    // Test ISO 8601 with Z (UTC)
    auto tp_utc = parseISO8601("2023-10-27T10:30:00Z").value();
    // To properly test UTC, we create a UTC time representation
    std::tm tm_utc = {};
    tm_utc.tm_year = 2023 - 1900;
    tm_utc.tm_mon = 10 - 1;
    tm_utc.tm_mday = 27;
    tm_utc.tm_hour = 10;
    tm_utc.tm_min = 30;
    tm_utc.tm_sec = 0;
    time_t time_utc = timegm(&tm_utc);
    auto expected_tp_utc = std::chrono::system_clock::from_time_t(time_utc);
    EXPECT_EQ(tp_utc, expected_tp_utc);
    
    // Test ISO 8601 without timezone (assumed local)
    auto tp_local = parseISO8601("2023-10-27T10:30:00").value();
    auto expected_tp_local = createTimePoint(2023, 10, 27, 10, 30, 0);
    EXPECT_EQ(tp_local, expected_tp_local);

    // Test ISO 8601 with offset (UTC+X)
    auto tp_offset_plus = parseISO8601("2023-10-27T12:30:00+02:00").value();
    // This is 12:30 in a timezone 2 hours ahead of UTC. So, it's 10:30 UTC.
    EXPECT_EQ(tp_offset_plus, expected_tp_utc);


    // Test ISO 8601 with offset (UTC-X)
    auto tp_offset_minus = parseISO8601("2023-10-27T05:30:00-05:00").value();
    // This is 5:30 in a timezone 5 hours behind UTC. So, it's 10:30 UTC.
    EXPECT_EQ(tp_offset_minus, expected_tp_utc);

    // Test invalid formats
    EXPECT_FALSE(parseISO8601("2023-10-27 10:30:00").has_value()); // Standard format, not ISO
    EXPECT_FALSE(parseISO8601("2023-13-01T10:30:00Z").has_value()); // Invalid month
    EXPECT_FALSE(parseISO8601("2023-10-27T25:30:00Z").has_value()); // Invalid hour
    EXPECT_FALSE(parseISO8601("2023-02-30T10:30:00").has_value()); // Invalid date (local)
    EXPECT_FALSE(parseISO8601("2023-02-30T10:30:00Z").has_value()); // Invalid date (UTC)
    EXPECT_FALSE(parseISO8601("2023-02-30T10:30:00+02:00").has_value()); // Invalid date (offset)
    EXPECT_FALSE(parseISO8601("2023-10-27T10:30:00+24:00").has_value()); // Invalid hours in offset
    EXPECT_FALSE(parseISO8601("2023-10-27T10:30:00-24:00").has_value()); // Invalid hours in offset
    EXPECT_FALSE(parseISO8601("2023-10-27T10:30:00+02:65").has_value()); // Invalid minutes in offset
    EXPECT_FALSE(parseISO8601("2023-10-27T10:30:00+XX:00").has_value()); // Invalid offset hours
}

TEST(UtilsTime, ParseDayRange) {
    using namespace Utils;

    // Test with YYYY-MM-DD
    auto range1 = parseDayRange("2023-10-27").value();
    auto start1 = createTimePoint(2023, 10, 27, 0, 0, 0);
    auto end1 = start1 + std::chrono::days(1);
    EXPECT_EQ(range1.first, start1);
    EXPECT_EQ(range1.second, end1);

    // Test with YYYY/MM/DD
    auto range2 = parseDayRange("2023/10/27").value();
    EXPECT_EQ(range2.first, start1);
    EXPECT_EQ(range2.second, end1);

    // Test with MM-DD-YYYY
    auto range3 = parseDayRange("10-27-2023").value();
    EXPECT_EQ(range3.first, start1);
    EXPECT_EQ(range3.second, end1);

    // Test with MM/DD/YYYY
    auto range4 = parseDayRange("10/27/2023").value();
    EXPECT_EQ(range4.first, start1);
    EXPECT_EQ(range4.second, end1);

    // Test invalid formats
    EXPECT_FALSE(parseDayRange("27-10-2023").has_value());
    EXPECT_FALSE(parseDayRange("2023-10-32").has_value());
    EXPECT_FALSE(parseDayRange("2023-13-27").has_value());
    EXPECT_FALSE(parseDayRange("2023-02-30").has_value());

    // Test DST transition (Spring Forward - day is 23 hours)
    auto start_before_dst = createTimePoint(2023, 3, 11, 0, 0, 0);
    auto range_before_dst = parseDayRange("2023-03-11").value();
    EXPECT_EQ(range_before_dst.first, start_before_dst);
    EXPECT_EQ(range_before_dst.second, start_before_dst + std::chrono::days(1));

    // Test DST transition (Fall Back - day is 25 hours)
    auto start_during_dst_end = createTimePoint(2023, 11, 5, 0, 0, 0);
    auto range_during_dst_end = parseDayRange("2023-11-05").value();
    EXPECT_EQ(range_during_dst_end.first, start_during_dst_end);
    EXPECT_EQ(range_during_dst_end.second, start_during_dst_end + std::chrono::days(1));
}

TEST(UtilsTime, FormatTimestamp) {
    auto testTimePoint = createTimePoint(2023, 10, 27, 10, 30, 0);
    std::string format = "%Y-%m-%d %H:%M:%S";
    
    std::string expected_format = "2023-10-27 10:30:00";
    EXPECT_EQ(Utils::formatTimestamp(testTimePoint, format), expected_format);

    // Test thread safety
    int numThreads = 10;
    std::vector<std::string> results(numThreads);
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&, i, testTimePoint, format, expected_format]() {
            results[i] = Utils::formatTimestamp(testTimePoint, format);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Check that all threads produced the same output
    for (int i = 0; i < numThreads; ++i) {
        EXPECT_EQ(results[i], expected_format);
    }
}
