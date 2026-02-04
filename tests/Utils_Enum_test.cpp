#include <gtest/gtest.h>
#include "../include/Utils.h"
#include "../include/LogTypes.h" // For LogEntryField, LogLevel
#include "../include/Filter.h"   // For FilterOperator, FilterLogicalOperator, FilterValueType
#include "../include/Exporter.h" // For ExportFormat
#include "../include/Statistics.h" // For StatisticType

// --- LogEntryField Enum Conversions ---
TEST(UtilsEnumConversionTest, LogEntryFieldToString) {
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::TIMESTAMP), "timestamp");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::LEVEL), "level");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::MESSAGE), "message");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::SOURCE_FILE), "source_file");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::LINE_NUMBER), "line_number");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::THREAD_ID), "thread_id");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::MODULE), "module");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::HOST), "host");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::CUSTOM), "custom");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::STRUCTURED_FIELD), "structured_field");
    EXPECT_EQ(Utils::logEntryFieldToString(LogEntryField::UNKNOWN), "unknown");
}

TEST(UtilsEnumConversionTest, StringToLogEntryField) {
    EXPECT_EQ(Utils::stringToLogEntryField("timestamp"), LogEntryField::TIMESTAMP);
    EXPECT_EQ(Utils::stringToLogEntryField("LEVEL"), LogEntryField::LEVEL); // Case-insensitive
    EXPECT_EQ(Utils::stringToLogEntryField("message"), LogEntryField::MESSAGE);
    EXPECT_EQ(Utils::stringToLogEntryField("Source_File"), LogEntryField::SOURCE_FILE); // Case-insensitive with underscore
    EXPECT_EQ(Utils::stringToLogEntryField("line_number"), LogEntryField::LINE_NUMBER);
    EXPECT_EQ(Utils::stringToLogEntryField("thread_id"), LogEntryField::THREAD_ID);
    EXPECT_EQ(Utils::stringToLogEntryField("module"), LogEntryField::MODULE);
    EXPECT_EQ(Utils::stringToLogEntryField("host"), LogEntryField::HOST);
    EXPECT_EQ(Utils::stringToLogEntryField("custom"), LogEntryField::CUSTOM);
    EXPECT_EQ(Utils::stringToLogEntryField("structured_field"), LogEntryField::STRUCTURED_FIELD);
    EXPECT_EQ(Utils::stringToLogEntryField("non_existent_field"), LogEntryField::UNKNOWN);
    EXPECT_EQ(Utils::stringToLogEntryField(""), LogEntryField::UNKNOWN);
}

// --- FilterOperator Enum Conversions ---
TEST(UtilsEnumConversionTest, FilterOperatorToString) {
    EXPECT_EQ(Utils::filterOperatorToString(FilterOperator::EQUALS), "EQUALS");
    EXPECT_EQ(Utils::filterOperatorToString(FilterOperator::NOT_EQUALS), "NOT_EQUALS");
    EXPECT_EQ(Utils::filterOperatorToString(FilterOperator::CONTAINS), "CONTAINS");
    EXPECT_EQ(Utils::filterOperatorToString(FilterOperator::NOT_CONTAINS), "NOT_CONTAINS");
    EXPECT_EQ(Utils::filterOperatorToString(FilterOperator::STARTS_WITH), "STARTS_WITH");
    EXPECT_EQ(Utils::filterOperatorToString(FilterOperator::ENDS_WITH), "ENDS_WITH");
    EXPECT_EQ(Utils::filterOperatorToString(FilterOperator::REGEX_MATCH), "REGEX_MATCH");
    EXPECT_EQ(Utils::filterOperatorToString(FilterOperator::LESS_THAN), "LESS_THAN");
    EXPECT_EQ(Utils::filterOperatorToString(FilterOperator::GREATER_THAN), "GREATER_THAN");
    EXPECT_EQ(Utils::filterOperatorToString(FilterOperator::LESS_THAN_OR_EQUAL), "LESS_THAN_OR_EQUAL");
    EXPECT_EQ(Utils::filterOperatorToString(FilterOperator::GREATER_THAN_OR_EQUAL), "GREATER_THAN_OR_EQUAL");
    EXPECT_EQ(Utils::filterOperatorToString(FilterOperator::UNKNOWN), "UNKNOWN");
}

TEST(UtilsEnumConversionTest, StringToFilterOperator) {
    EXPECT_EQ(Utils::stringToFilterOperator("EQUALS"), FilterOperator::EQUALS);
    EXPECT_EQ(Utils::stringToFilterOperator("equals"), FilterOperator::EQUALS); // Case-insensitive
    EXPECT_EQ(Utils::stringToFilterOperator("NOT_EQUALS"), FilterOperator::NOT_EQUALS);
    EXPECT_EQ(Utils::stringToFilterOperator("contains"), FilterOperator::CONTAINS);
    EXPECT_EQ(Utils::stringToFilterOperator("NOT_CONTAINS"), FilterOperator::NOT_CONTAINS);
    EXPECT_EQ(Utils::stringToFilterOperator("starts_with"), FilterOperator::STARTS_WITH);
    EXPECT_EQ(Utils::stringToFilterOperator("ENDS_WITH"), FilterOperator::ENDS_WITH);
    EXPECT_EQ(Utils::stringToFilterOperator("regex_match"), FilterOperator::REGEX_MATCH);
    EXPECT_EQ(Utils::stringToFilterOperator("less_than"), FilterOperator::LESS_THAN);
    EXPECT_EQ(Utils::stringToFilterOperator("GREATER_THAN"), FilterOperator::GREATER_THAN);
    EXPECT_EQ(Utils::stringToFilterOperator("less_than_or_equal"), FilterOperator::LESS_THAN_OR_EQUAL);
    EXPECT_EQ(Utils::stringToFilterOperator("GREATER_THAN_OR_EQUAL"), FilterOperator::GREATER_THAN_OR_EQUAL);
    EXPECT_EQ(Utils::stringToFilterOperator("non_existent_op"), FilterOperator::UNKNOWN);
    EXPECT_EQ(Utils::stringToFilterOperator(""), FilterOperator::UNKNOWN);
}

// --- FilterLogicalOperator Enum Conversions ---
TEST(UtilsEnumConversionTest, FilterLogicalOperatorToString) {
    EXPECT_EQ(Utils::filterLogicalOperatorToString(FilterLogicalOperator::AND), "AND");
    EXPECT_EQ(Utils::filterLogicalOperatorToString(FilterLogicalOperator::OR), "OR");
    EXPECT_EQ(Utils::filterLogicalOperatorToString(FilterLogicalOperator::NOT), "NOT");
    EXPECT_EQ(Utils::filterLogicalOperatorToString(FilterLogicalOperator::UNKNOWN), "UNKNOWN");
}

TEST(UtilsEnumConversionTest, StringToFilterLogicalOperator) {
    EXPECT_EQ(Utils::stringToFilterLogicalOperator("AND"), FilterLogicalOperator::AND);
    EXPECT_EQ(Utils::stringToFilterLogicalOperator("and"), FilterLogicalOperator::AND); // Case-insensitive
    EXPECT_EQ(Utils::stringToFilterLogicalOperator("OR"), FilterLogicalOperator::OR);
    EXPECT_EQ(Utils::stringToFilterLogicalOperator("not"), FilterLogicalOperator::NOT);
    EXPECT_EQ(Utils::stringToFilterLogicalOperator("non_existent_logic"), FilterLogicalOperator::UNKNOWN);
    EXPECT_EQ(Utils::stringToFilterLogicalOperator(""), FilterLogicalOperator::UNKNOWN);
}

// --- FilterValueType Enum Conversions ---
TEST(UtilsEnumConversionTest, FilterValueTypeToString) {
    EXPECT_EQ(Utils::filterValueTypeToString(FilterValueType::STRING), "STRING");
    EXPECT_EQ(Utils::filterValueTypeToString(FilterValueType::NUMERIC), "NUMERIC");
    EXPECT_EQ(Utils::filterValueTypeToString(FilterValueType::DATETIME), "DATETIME");
    EXPECT_EQ(Utils::filterValueTypeToString(FilterValueType::UNKNOWN), "UNKNOWN");
}

TEST(UtilsEnumConversionTest, StringToFilterValueType) {
    EXPECT_EQ(Utils::stringToFilterValueType("STRING"), FilterValueType::STRING);
    EXPECT_EQ(Utils::stringToFilterValueType("string"), FilterValueType::STRING); // Case-insensitive
    EXPECT_EQ(Utils::stringToFilterValueType("NUMERIC"), FilterValueType::NUMERIC);
    EXPECT_EQ(Utils::stringToFilterValueType("datetime"), FilterValueType::DATETIME);
    EXPECT_EQ(Utils::stringToFilterValueType("non_existent_type"), FilterValueType::UNKNOWN);
    EXPECT_EQ(Utils::stringToFilterValueType(""), FilterValueType::UNKNOWN);
}

// --- LogLevel Enum Conversions (Existing but ensuring completeness) ---
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
    EXPECT_EQ(Utils::stringToLogLevel("debug"), LogLevel::DEBUG); // Case-insensitive
    EXPECT_EQ(Utils::stringToLogLevel("INFO"), LogLevel::INFO);
    EXPECT_EQ(Utils::stringToLogLevel("warning"), LogLevel::WARNING);
    EXPECT_EQ(Utils::stringToLogLevel("ERROR"), LogLevel::ERROR);
    EXPECT_EQ(Utils::stringToLogLevel("critical"), LogLevel::CRITICAL);
    EXPECT_EQ(Utils::stringToLogLevel("FATAL"), LogLevel::FATAL);
    EXPECT_EQ(Utils::stringToLogLevel("non_existent_level"), LogLevel::UNKNOWN);
    EXPECT_EQ(Utils::stringToLogLevel(""), LogLevel::UNKNOWN);
}

// --- ExportFormat Enum Conversions ---
TEST(UtilsEnumConversionTest, ExportFormatToString) {
    EXPECT_EQ(Utils::exportFormatToString(ExportFormat::PLAINTEXT), "plaintext");
    EXPECT_EQ(Utils::exportFormatToString(ExportFormat::JSON), "json");
    EXPECT_EQ(Utils::exportFormatToString(ExportFormat::CSV), "csv");
    EXPECT_EQ(Utils::exportFormatToString(ExportFormat::XML), "xml");
    EXPECT_EQ(Utils::exportFormatToString(ExportFormat::UNKNOWN), "unknown");
}

TEST(UtilsEnumConversionTest, StringToExportFormat) {
    EXPECT_EQ(Utils::stringToExportFormat("plaintext"), ExportFormat::PLAINTEXT);
    EXPECT_EQ(Utils::stringToExportFormat("JSON"), ExportFormat::JSON); // Case-insensitive
    EXPECT_EQ(Utils::stringToExportFormat("csv"), ExportFormat::CSV);
    EXPECT_EQ(Utils::stringToExportFormat("XML"), ExportFormat::XML);
    EXPECT_EQ(Utils::stringToExportFormat("non_existent_format"), ExportFormat::UNKNOWN);
    EXPECT_EQ(Utils::stringToExportFormat(""), ExportFormat::UNKNOWN);
}

// --- StatisticType Enum Conversions ---
TEST(UtilsEnumConversionTest, StatisticTypeToString) {
    EXPECT_EQ(Utils::statisticTypeToString(StatisticType::UNIQUE_MESSAGES), "unique_messages");
    EXPECT_EQ(Utils::statisticTypeToString(StatisticType::TOP_MESSAGES), "top_messages");
    EXPECT_EQ(Utils::statisticTypeToString(StatisticType::ENTRY_RATE), "entry_rate");
    EXPECT_EQ(Utils::statisticTypeToString(StatisticType::LOG_LEVEL_COUNT), "count_by_level");
    EXPECT_EQ(Utils::statisticTypeToString(StatisticType::FIELD_VALUE_COUNT), "field_value_count");
    EXPECT_EQ(Utils::statisticTypeToString(StatisticType::TOP_N_FIELD_VALUES), "top_n_field_values");
    EXPECT_EQ(Utils::statisticTypeToString(StatisticType::UNKNOWN), "unknown");
}

TEST(UtilsEnumConversionTest, StringToStatisticType) {
    EXPECT_EQ(Utils::stringToStatisticType("unique_messages"), StatisticType::UNIQUE_MESSAGES);
    EXPECT_EQ(Utils::stringToStatisticType("TOP_MESSAGES"), StatisticType::TOP_MESSAGES); // Case-insensitive
    EXPECT_EQ(Utils::stringToStatisticType("entry_rate"), StatisticType::ENTRY_RATE);
    EXPECT_EQ(Utils::stringToStatisticType("COUNT_BY_LEVEL"), StatisticType::LOG_LEVEL_COUNT);
    EXPECT_EQ(Utils::stringToStatisticType("field_value_count"), StatisticType::FIELD_VALUE_COUNT);
    EXPECT_EQ(Utils::stringToStatisticType("top_n_field_values"), StatisticType::TOP_N_FIELD_VALUES);
    EXPECT_EQ(Utils::stringToStatisticType("non_existent_statistic"), StatisticType::UNKNOWN);
    EXPECT_EQ(Utils::stringToStatisticType(""), StatisticType::UNKNOWN);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
