// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "tests/config/CLIConfigTest.h"

TEST_F(CLIConfigTest, EnabledStatistics) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--stats", "unique_messages", "--stats", "top_messages:5"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second; 
    ASSERT_EQ(options.enabledStatistics.size(), 2);
    ASSERT_EQ(options.enabledStatistics[0], "unique_messages");
    ASSERT_EQ(options.enabledStatistics[1], "top_messages:5");
}

TEST_F(CLIConfigTest, TopMessagesCount) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--top-n", "20"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.topMessagesCount, 20);
    ASSERT_EQ(settings.exportSettings.topMessagesCount, 20);
}

TEST_F(CLIConfigTest, StatsWindow) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--stats-window", "300"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.statsWindow.has_value());
    ASSERT_EQ(options.statsWindow.value(), std::chrono::seconds(300));
}

TEST_F(CLIConfigTest, FindGapsDuration) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--find-gaps", "5000"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.findGapsDuration.has_value());
    ASSERT_EQ(options.findGapsDuration.value(), std::chrono::milliseconds(5000));
}
