// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

// Focused tests for src/config/cli_helpers.cpp's two untested parsing helpers:
// trimInPlace (whitespace trimming used throughout config parsing) and
// parseStatisticConfig (the --stats argument parser). parseStatisticConfig has
// many branches — the prefix forms, the legacy bare type name, and the
// comma-delimited key=value form — none of which had direct unit coverage.

#include "gtest/gtest.h"
#include "config/cli_helpers.h"

#include <optional>
#include <string>

namespace {

using CLIConfigHelpers::parseStatisticConfig;
using CLIConfigHelpers::trimInPlace;

TEST(TrimInPlace, EmptyAndAllWhitespaceBecomeEmpty) {
    std::string empty;
    trimInPlace(empty);
    EXPECT_EQ(empty, "");

    std::string spaces = "   \t  ";
    trimInPlace(spaces);
    EXPECT_EQ(spaces, "");
}

TEST(TrimInPlace, TrimsEndsButPreservesInternalWhitespace) {
    std::string leading = "   hello";
    trimInPlace(leading);
    EXPECT_EQ(leading, "hello");

    std::string trailing = "hello   ";
    trimInPlace(trailing);
    EXPECT_EQ(trailing, "hello");

    std::string both = "  hi  ";
    trimInPlace(both);
    EXPECT_EQ(both, "hi");

    std::string internal = "  a b\tc  ";
    trimInPlace(internal);
    EXPECT_EQ(internal, "a b\tc");
}

TEST(ParseStatisticConfig, TopMessagesValidDigits) {
    auto cfg = parseStatisticConfig("top_messages:5");
    ASSERT_TRUE(cfg.has_value());
    EXPECT_EQ(cfg->type, StatisticType::TOP_MESSAGES);
    EXPECT_EQ(cfg->params.at("top_n"), "5");
}

TEST(ParseStatisticConfig, TopMessagesNonDigitRejected) {
    EXPECT_FALSE(parseStatisticConfig("top_messages:abc").has_value());
}

TEST(ParseStatisticConfig, PrefixMatchIsCaseInsensitiveAndTrimsWhitespace) {
    auto upper = parseStatisticConfig("TOP_MESSAGES:42");
    ASSERT_TRUE(upper.has_value());
    EXPECT_EQ(upper->type, StatisticType::TOP_MESSAGES);
    EXPECT_EQ(upper->params.at("top_n"), "42");

    auto spaced = parseStatisticConfig("  top_messages:  7  ");
    ASSERT_TRUE(spaced.has_value());
    EXPECT_EQ(spaced->type, StatisticType::TOP_MESSAGES);
    EXPECT_EQ(spaced->params.at("top_n"), "7");
}

TEST(ParseStatisticConfig, PercentileStatsCapturesField) {
    auto cfg = parseStatisticConfig("percentile_stats:latency_ms");
    ASSERT_TRUE(cfg.has_value());
    EXPECT_EQ(cfg->type, StatisticType::PERCENTILE_STATS);
    EXPECT_EQ(cfg->params.at("field"), "latency_ms");
}

TEST(ParseStatisticConfig, PercentileStatsEmptyFieldRejected) {
    EXPECT_FALSE(parseStatisticConfig("percentile_stats:").has_value());
}

TEST(ParseStatisticConfig, MovingAverageFindGapsAndHistogramKeys) {
    auto mar = parseStatisticConfig("moving_average_rate:60");
    ASSERT_TRUE(mar.has_value());
    EXPECT_EQ(mar->type, StatisticType::MOVING_AVERAGE_RATE);
    EXPECT_EQ(mar->params.at("bucket"), "60");

    auto gaps = parseStatisticConfig("find_gaps:1000");
    ASSERT_TRUE(gaps.has_value());
    EXPECT_EQ(gaps->type, StatisticType::FIND_GAPS);
    EXPECT_EQ(gaps->params.at("threshold_ms"), "1000");

    auto hist = parseStatisticConfig("time_bucket_histogram:3600");
    ASSERT_TRUE(hist.has_value());
    EXPECT_EQ(hist->type, StatisticType::TIME_BUCKET_HISTOGRAM);
    EXPECT_EQ(hist->params.at("bucket"), "3600");
}

TEST(ParseStatisticConfig, LegacyBareTypeNameResolves) {
    auto cfg = parseStatisticConfig("count_by_level");
    ASSERT_TRUE(cfg.has_value());
    EXPECT_EQ(cfg->type, StatisticType::LOG_LEVEL_COUNT);
    EXPECT_TRUE(cfg->params.empty());
}

TEST(ParseStatisticConfig, CommaDelimitedKeyValueForm) {
    auto cfg = parseStatisticConfig("type=top_messages,top_n=7");
    ASSERT_TRUE(cfg.has_value());
    EXPECT_EQ(cfg->type, StatisticType::TOP_MESSAGES);
    EXPECT_EQ(cfg->params.at("top_n"), "7");
}

TEST(ParseStatisticConfig, DuplicateTypeRejected) {
    EXPECT_FALSE(parseStatisticConfig("type=top_messages,type=find_gaps").has_value());
}

TEST(ParseStatisticConfig, EmptyInputRejected) {
    EXPECT_FALSE(parseStatisticConfig("").has_value());
}

} // namespace
