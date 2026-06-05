// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "analyzer/core.h"
#include "filter/expression.h"
#include <map>
#include <string>
#include <fstream>

using namespace filter;

class LogAnalyzerFilterTest : public ::testing::Test {
protected:
    LogAnalyzer analyzer;
};

TEST_F(LogAnalyzerFilterTest, SortedFilteredEntriesDescendingUsesStrictComparator) {
    const std::string filePath = "test_desc_sort.log";
    std::ofstream ofs(filePath);
    ofs << "2023-01-01 10:00:00 INFO: alpha\n";
    ofs << "2023-01-01 10:00:01 INFO: zeta\n";
    ofs << "2023-01-01 10:00:02 INFO: middle\n";
    ofs << "2023-01-01 10:00:03 INFO: middle\n";
    ofs.close();

    auto loadResult = analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
    std::remove(filePath.c_str());
    ASSERT_TRUE(loadResult.has_value()) << loadResult.error().toString();

    FilterExpression allEntries;
    auto sortedResult = analyzer.getSortedFilteredEntries(allEntries, SortBy::MESSAGE, SortOrder::DESCENDING);
    ASSERT_TRUE(sortedResult.has_value()) << sortedResult.error().toString();
    auto sorted = sortedResult.value();

    ASSERT_EQ(sorted.size(), 4);
    EXPECT_EQ(sorted.front().message, "zeta");
    EXPECT_EQ(sorted.back().message, "alpha");
    for (size_t i = 1; i < sorted.size(); ++i) {
        EXPECT_GE(sorted[i - 1].message, sorted[i].message);
    }
}

TEST_F(LogAnalyzerFilterTest, SortedFilteredEntriesMaintainOrderingAndMembershipAcrossDirections) {
    const std::string filePath = "test_sort_properties.log";
    std::ofstream ofs(filePath);
    ofs << "2023-01-01 10:00:00 INFO: beta\n";
    ofs << "2023-01-01 10:00:01 ERROR: alpha\n";
    ofs << "2023-01-01 10:00:02 WARNING: alpha\n";
    ofs << "2023-01-01 10:00:03 DEBUG: gamma\n";
    ofs << "2023-01-01 10:00:04 INFO: beta\n";
    ofs << "2023-01-01 10:00:05 ERROR: delta\n";
    ofs.close();

    auto loadResult = analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
    std::remove(filePath.c_str());
    ASSERT_TRUE(loadResult.has_value()) << loadResult.error().toString();

    auto entryId = [](const LogEntry& e) {
        const auto ts = e.timestamp.has_value() 
            ? std::to_string(e.timestamp->time_since_epoch().count())
            : std::string("no-ts");
        return ts + "|" + 
               std::to_string(static_cast<int>(e.level)) + "|" + e.sourceFile + "|" + e.message;
    };

    auto lessByField = [](const LogEntry& lhs, const LogEntry& rhs, SortBy key) {
        switch (key) {
            case SortBy::TIMESTAMP: return lhs.timestamp < rhs.timestamp;
            case SortBy::LEVEL: return lhs.level < rhs.level;
            case SortBy::MESSAGE: return lhs.message < rhs.message;
            case SortBy::SOURCE: return lhs.sourceFile < rhs.sourceFile;
            case SortBy::THREAD_ID:
                if (lhs.threadId.has_value() && rhs.threadId.has_value()) return *lhs.threadId < *rhs.threadId;
                return lhs.threadId.has_value() < rhs.threadId.has_value();
            default:
                return false;
        }
        return false;
    };

    const std::vector<SortBy> sortKeys = { SortBy::TIMESTAMP, SortBy::LEVEL, SortBy::MESSAGE, SortBy::SOURCE, SortBy::THREAD_ID };

    FilterExpression allEntries;
    for (const auto sortBy : sortKeys) {
        auto ascendingResult = analyzer.getSortedFilteredEntries(allEntries, sortBy, SortOrder::ASCENDING);
        auto descendingResult = analyzer.getSortedFilteredEntries(allEntries, sortBy, SortOrder::DESCENDING);

        ASSERT_TRUE(ascendingResult.has_value()) << ascendingResult.error().toString();
        ASSERT_TRUE(descendingResult.has_value()) << descendingResult.error().toString();

        const auto ascending = ascendingResult.value();
        const auto descending = descendingResult.value();

        ASSERT_EQ(ascending.size(), descending.size());
        ASSERT_EQ(ascending.size(), 6u);

        for (size_t i = 1; i < ascending.size(); ++i) {
            EXPECT_FALSE(lessByField(ascending[i], ascending[i - 1], sortBy));
        }
        for (size_t i = 1; i < descending.size(); ++i) {
            EXPECT_FALSE(lessByField(descending[i - 1], descending[i], sortBy));
        }

        std::map<std::string, size_t> ascCounts;
        std::map<std::string, size_t> descCounts;
        for (const auto& e : ascending) ++ascCounts[entryId(e)];
        for (const auto& e : descending) ++descCounts[entryId(e)];
        EXPECT_EQ(ascCounts, descCounts);
    }
}

class FilterExpressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::string logContent = 
            "2023-01-01 10:00:00 INFO: User 'test' logged in.\n"
            "2023-01-01 10:01:00 DEBUG: Starting task 'A'.\n"
            "2023-01-01 10:02:00 WARNING: Task 'A' took 200ms.\n"
            "2023-01-01 10:03:00 INFO: User 'test' logged out.\n"
            "2023-01-01 10:04:00 ERROR: Failed to process task 'B'.\n";
        std::string filePath = "filter_expression_test.log";
        std::ofstream ofs(filePath);
        ofs << logContent;
        ofs.close();
        auto reportResult = analyzer.loadAndReplace(filePath, ParserErrorAction::Warn);
        ASSERT_TRUE(reportResult.has_value());
        std::remove(filePath.c_str());
    }

    LogAnalyzer analyzer;
};

TEST_F(FilterExpressionTest, SimpleCondition) {
    auto condition = FilterCondition::createString(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO");
    ASSERT_TRUE(condition.has_value());
    FilterExpression expression(*condition);
    auto result = analyzer.getFilteredEntries(expression);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 2);
}

TEST_F(FilterExpressionTest, AndExpression) {
    auto cond1 = FilterCondition::createString(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO");
    ASSERT_TRUE(cond1.has_value());
    auto cond2 = FilterCondition::createString(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "logged in");
    ASSERT_TRUE(cond2.has_value());
    FilterExpression expression = FilterExpression(*cond1).And(FilterExpression(*cond2));
    auto result = analyzer.getFilteredEntries(expression);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 1);
    ASSERT_EQ(result.value()[0].message, "User 'test' logged in.");
}

TEST_F(FilterExpressionTest, OrExpression) {
    auto cond1 = FilterCondition::createString(LogEntryField::LEVEL, FilterOperator::EQUALS, "ERROR");
    ASSERT_TRUE(cond1.has_value());
    auto cond2 = FilterCondition::createString(LogEntryField::LEVEL, FilterOperator::EQUALS, "WARNING");
    ASSERT_TRUE(cond2.has_value());
    FilterExpression expression = FilterExpression(*cond1).Or(FilterExpression(*cond2));
    auto result = analyzer.getFilteredEntries(expression);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 2);
}

TEST_F(FilterExpressionTest, NotExpression) {
    auto condition = FilterCondition::createString(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO");
    ASSERT_TRUE(condition.has_value());
    FilterExpression expression = FilterExpression(*condition).Not();
    auto result = analyzer.getFilteredEntries(expression);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 3);
}

TEST_F(FilterExpressionTest, ComplexExpression) {
    auto info = FilterCondition::createString(LogEntryField::LEVEL, FilterOperator::EQUALS, "INFO");
    ASSERT_TRUE(info.has_value());
    auto test = FilterCondition::createString(LogEntryField::MESSAGE, FilterOperator::CONTAINS, "test");
    ASSERT_TRUE(test.has_value());
    auto error = FilterCondition::createString(LogEntryField::LEVEL, FilterOperator::EQUALS, "ERROR");
    ASSERT_TRUE(error.has_value());
    FilterExpression expression = (FilterExpression(*info).And(FilterExpression(*test))).Or(FilterExpression(*error));
    auto result = analyzer.getFilteredEntries(expression);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 3);
}
