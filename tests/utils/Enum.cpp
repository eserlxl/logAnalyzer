// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "utils/Core.h"
#include "core/LogTypes.h"
#include "filter/Types.h" // Ensure this header includes SortBy and SortOrder
#include "filter/Condition.h" // Added as a common header that might transitively include sort-related enums
#include "filter/Expression.h" // Added for similar reasons
#include "export/Exporter.h"
#include "stats/Statistics.h"
#include <optional>

// --- LogLevel Enum Conversions ---
TEST(UtilsEnumConversionTest, LogLevelToString) {
    EXPECT_EQ(Utils::logLevelToString(LogLevel::TRACE), "TRACE");
    EXPECT_EQ(Utils::logLevelToString(LogLevel::DEBUG), "DEBUG");
    EXPECT_EQ(Utils::logLevelToString(LogLevel::INFO), "INFO");
    EXPECT_EQ(Utils::logLevelToString(LogLevel::WARNING), "WARNING");
    EXPECT_EQ(Utils::logLevelToString(LogLevel::ERROR), "ERROR");
    EXPECT_EQ(Utils::logLevelToString(LogLevel::CRITICAL), "CRITICAL");
    EXPECT_EQ(Utils::logLevelToString(LogLevel::FATAL), "FATAL");
    EXPECT_EQ(Utils::logLevelToString(LogLevel::UNKNOWN), "UNKNOWN");
}

TEST(UtilsEnumConversionTest, StringToLogLevel) {
    EXPECT_EQ(Utils::stringToLogLevel("TRACE"), LogLevel::TRACE);
    EXPECT_EQ(Utils::stringToLogLevel("debug"), LogLevel::DEBUG); // Case insensitive
    EXPECT_EQ(Utils::stringToLogLevel("Info"), LogLevel::INFO);
    EXPECT_EQ(Utils::stringToLogLevel("WARNING"), LogLevel::WARNING);
    EXPECT_EQ(Utils::stringToLogLevel("ERROR"), LogLevel::ERROR);
    EXPECT_EQ(Utils::stringToLogLevel("CRITICAL"), LogLevel::CRITICAL);
    EXPECT_EQ(Utils::stringToLogLevel("FATAL"), LogLevel::FATAL);
    EXPECT_EQ(Utils::stringToLogLevel("INVALID"), LogLevel::UNKNOWN);
}

// --- LogEntryField Enum Conversions ---
TEST(UtilsEnumConversionTest, LogEntryFieldToString) {
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::TIMESTAMP), "TIMESTAMP");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::LEVEL), "LEVEL");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::MESSAGE), "MESSAGE");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::SOURCE_FILE), "SOURCE_FILE");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::LINE_NUMBER), "LINE_NUMBER");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::THREAD_ID), "THREAD_ID");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::MODULE), "MODULE");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::HOST), "HOST");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::CUSTOM), "CUSTOM");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::STRUCTURED_FIELD), "STRUCTURED_FIELD");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::UNKNOWN), "UNKNOWN");
}

TEST(UtilsEnumConversionTest, StringToLogEntryField) {
    EXPECT_EQ(Utils::stringToLogEntryField("TIMESTAMP"), LogEntryField::TIMESTAMP);
    EXPECT_EQ(Utils::stringToLogEntryField("level"), LogEntryField::LEVEL); // Case insensitive
    EXPECT_EQ(Utils::stringToLogEntryField("Message"), LogEntryField::MESSAGE);
    EXPECT_EQ(Utils::stringToLogEntryField("SOURCE_FILE"), LogEntryField::SOURCE_FILE);
    EXPECT_EQ(Utils::stringToLogEntryField("LINE_NUMBER"), LogEntryField::LINE_NUMBER);
    EXPECT_EQ(Utils::stringToLogEntryField("THREAD_ID"), LogEntryField::THREAD_ID);
    EXPECT_EQ(Utils::stringToLogEntryField("MODULE"), LogEntryField::MODULE);
    EXPECT_EQ(Utils::stringToLogEntryField("HOST"), LogEntryField::HOST);
    EXPECT_EQ(Utils::stringToLogEntryField("CUSTOM"), LogEntryField::UNKNOWN);
    EXPECT_EQ(Utils::stringToLogEntryField("STRUCTURED_FIELD"), LogEntryField::STRUCTURED_FIELD);
    EXPECT_EQ(Utils::stringToLogEntryField("INVALID"), LogEntryField::UNKNOWN);
}

// --- ExportFormat Enum Conversions ---
TEST(UtilsEnumConversionTest, ExportFormatToString) {
    EXPECT_EQ(Utils::exportFormatToString(ExportFormat::PLAINTEXT), "PLAINTEXT");
    EXPECT_EQ(Utils::exportFormatToString(ExportFormat::JSON), "JSON");
    EXPECT_EQ(Utils::exportFormatToString(ExportFormat::CSV), "CSV");
    EXPECT_EQ(Utils::exportFormatToString(ExportFormat::XML), "XML");
}

TEST(UtilsEnumConversionTest, StringToExportFormat) {
    EXPECT_EQ(Utils::stringToExportFormat("PLAINTEXT").value(), ExportFormat::PLAINTEXT);
    EXPECT_EQ(Utils::stringToExportFormat("text").value(), ExportFormat::PLAINTEXT); // Alias
    EXPECT_EQ(Utils::stringToExportFormat("json").value(), ExportFormat::JSON);
    EXPECT_EQ(Utils::stringToExportFormat("CSV").value(), ExportFormat::CSV);
    EXPECT_EQ(Utils::stringToExportFormat("xml").value(), ExportFormat::XML);
    EXPECT_FALSE(Utils::stringToExportFormat("PDF").has_value());
}

// --- StatisticType Enum Conversions ---
TEST(UtilsEnumConversionTest, StatisticTypeToString) {
    EXPECT_EQ(Utils::statisticTypeToString(StatisticType::UNIQUE_MESSAGES), "UNIQUE_MESSAGES");
    EXPECT_EQ(Utils::statisticTypeToString(StatisticType::TOP_MESSAGES), "TOP_MESSAGES");
    EXPECT_EQ(Utils::statisticTypeToString(StatisticType::ENTRY_RATE), "ENTRY_RATE");
    EXPECT_EQ(Utils::statisticTypeToString(StatisticType::LOG_LEVEL_COUNT), "COUNT_BY_LEVEL");
    EXPECT_EQ(Utils::statisticTypeToString(StatisticType::FIELD_VALUE_COUNT), "FIELD_VALUE_COUNT");
    EXPECT_EQ(Utils::statisticTypeToString(StatisticType::TOP_N_FIELD_VALUES), "TOP_N_FIELD_VALUES");
}

TEST(UtilsEnumConversionTest, StringToStatisticType) {
    EXPECT_EQ(Utils::stringToStatisticType("UNIQUE_MESSAGES").value(), StatisticType::UNIQUE_MESSAGES);
    EXPECT_EQ(Utils::stringToStatisticType("top_messages").value(), StatisticType::TOP_MESSAGES);
    EXPECT_EQ(Utils::stringToStatisticType("Entry_Rate").value(), StatisticType::ENTRY_RATE);
    EXPECT_EQ(Utils::stringToStatisticType("COUNT_BY_LEVEL").value(), StatisticType::LOG_LEVEL_COUNT);
    EXPECT_EQ(Utils::stringToStatisticType("LOG_LEVEL_COUNT").value(), StatisticType::LOG_LEVEL_COUNT); // Alias
    EXPECT_EQ(Utils::stringToStatisticType("field_value_count").value(), StatisticType::FIELD_VALUE_COUNT);
    EXPECT_EQ(Utils::stringToStatisticType("TOP_N_FIELD_VALUES").value(), StatisticType::TOP_N_FIELD_VALUES);
    EXPECT_FALSE(Utils::stringToStatisticType("INVALID_STAT").has_value());
}

// --- PatternType Enum Conversions ---
TEST(UtilsEnumConversionTest, PatternTypeToString) {
    EXPECT_EQ(Utils::patternTypeToString(PatternType::Literal), "Literal");
    EXPECT_EQ(Utils::patternTypeToString(PatternType::Regex), "Regex");
    EXPECT_EQ(Utils::patternTypeToString(PatternType::Wildcard), "Wildcard");
}

TEST(UtilsEnumConversionTest, StringToPatternType) {
    EXPECT_EQ(Utils::stringToPatternType("LITERAL").value(), PatternType::Literal);
    EXPECT_EQ(Utils::stringToPatternType("regex").value(), PatternType::Regex);
    EXPECT_EQ(Utils::stringToPatternType("Wildcard").value(), PatternType::Wildcard);
    EXPECT_FALSE(Utils::stringToPatternType("FUZZY").has_value());
}

// --- ParseError Enum Conversions ---
TEST(UtilsEnumConversionTest, ParseErrorToString) {
    EXPECT_EQ(Utils::parseErrorToString(ParseError::SUCCESS), "Success");
    EXPECT_EQ(Utils::parseErrorToString(ParseError::PARTIAL_FAILURE), "Partial Failure");
    EXPECT_EQ(Utils::parseErrorToString(ParseError::UNKNOWN_ERROR), "Unknown Error");
    EXPECT_EQ(Utils::parseErrorToString(ParseError::INVALID_REGEX_PATTERN), "Invalid Regex Pattern");
}

TEST(UtilsEnumConversionTest, StringToParseError) {
    EXPECT_EQ(Utils::stringToParseError("SUCCESS").value(), ParseError::SUCCESS);
    EXPECT_EQ(Utils::stringToParseError("partial failure").value(), ParseError::PARTIAL_FAILURE);
    EXPECT_EQ(Utils::stringToParseError("PARTIAL_FAILURE").value(), ParseError::PARTIAL_FAILURE); // Alias
    EXPECT_EQ(Utils::stringToParseError("unknown error").value(), ParseError::UNKNOWN_ERROR);
    EXPECT_EQ(Utils::stringToParseError("invalid_regex_pattern").value(), ParseError::INVALID_REGEX_PATTERN);
    EXPECT_FALSE(Utils::stringToParseError("CRASH").has_value());
}

// --- SortBy Enum Conversions ---
TEST(UtilsEnumConversionTest, SortByToString) {
    EXPECT_EQ(Utils::sortByToString(SortBy::TIMESTAMP), "TIMESTAMP");
    EXPECT_EQ(Utils::sortByToString(SortBy::LEVEL), "LEVEL");
    EXPECT_EQ(Utils::sortByToString(SortBy::MESSAGE), "MESSAGE");
    EXPECT_EQ(Utils::sortByToString(SortBy::SOURCE), "SOURCE");
    EXPECT_EQ(Utils::sortByToString(SortBy::THREAD_ID), "THREAD_ID");
}

TEST(UtilsEnumConversionTest, StringToSortBy) {
    EXPECT_EQ(Utils::stringToSortBy("TIMESTAMP").value(), SortBy::TIMESTAMP);
    EXPECT_EQ(Utils::stringToSortBy("time").value(), SortBy::TIMESTAMP); // Alias
    EXPECT_EQ(Utils::stringToSortBy("LEVEL").value(), SortBy::LEVEL);
    EXPECT_EQ(Utils::stringToSortBy("MESSAGE").value(), SortBy::MESSAGE);
    EXPECT_EQ(Utils::stringToSortBy("msg").value(), SortBy::MESSAGE); // Alias
    EXPECT_EQ(Utils::stringToSortBy("SOURCE").value(), SortBy::SOURCE);
    EXPECT_EQ(Utils::stringToSortBy("THREAD_ID").value(), SortBy::THREAD_ID);
    EXPECT_EQ(Utils::stringToSortBy("thread").value(), SortBy::THREAD_ID); // Alias
    EXPECT_FALSE(Utils::stringToSortBy("SIZE").has_value());
}

// --- SortOrder Enum Conversions ---
TEST(UtilsEnumConversionTest, SortOrderToString) {
    EXPECT_EQ(Utils::sortOrderToString(SortOrder::ASCENDING), "ASCENDING");
    EXPECT_EQ(Utils::sortOrderToString(SortOrder::DESCENDING), "DESCENDING");
}

TEST(UtilsEnumConversionTest, StringToSortOrder) {
    EXPECT_EQ(Utils::stringToSortOrder("ASCENDING").value(), SortOrder::ASCENDING);
    EXPECT_EQ(Utils::stringToSortOrder("asc").value(), SortOrder::ASCENDING); // Alias
    EXPECT_EQ(Utils::stringToSortOrder("DESCENDING").value(), SortOrder::DESCENDING);
    EXPECT_EQ(Utils::stringToSortOrder("desc").value(), SortOrder::DESCENDING); // Alias
    EXPECT_FALSE(Utils::stringToSortOrder("RANDOM").has_value());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
