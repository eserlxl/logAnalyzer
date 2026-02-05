#include "gtest/gtest.h"
#include "config/CLIConfig.h"
#include "core/LogTypes.h"
#include "core/Error.h"
#include "utils/Time.h" // Added for Utils::parseTime in tests
#include "export/Exporter.h" // Added for ExportFormat
#include "config/Settings.h" // Needed for DEFAULT_LOG_REGEX_PATTERN_INTERNAL
#include <fstream>
#include <filesystem>
#include <chrono>

// Test fixture for CLIConfig tests
class CLIConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a dummy file for tests that need a file path
        std::ofstream dummy_file("dummy_log_file.log");
        dummy_file << "dummy content\n";
        dummy_file.close();
    }

    void TearDown() override {
        std::filesystem::remove("dummy_log_file.log");
    }

    // Helper function to call parseCLI with a vector of C-style strings
    ErrorCode::Result<std::pair<LogAnalyzerSettings, CLIConfig::CLIOptions>> parse(std::vector<const char*> args) {
        return CLIConfig::parseCLI(args.size(), args.data());
    }
};

TEST_F(CLIConfigTest, ParseLogLevel) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--level", "INFO"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.filterLevels.size(), 1);
    ASSERT_EQ(options.filterLevels[0], LogLevel::INFO);
    // LogAnalyzerSettings does not directly store filterLevels, they are converted to filterRules.
    // Explicit verification of filterRules is complex and depends on CLIConfig's internal conversion logic,
    // which is beyond the scope of a direct CLI option test. For now, check CLIOptions.
}

TEST_F(CLIConfigTest, ParseSingleFilePathAndVerifyOptions) {
    auto result = parse({"log_analyzer", "dummy_log_file.log"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    
    // Verify CLIOptions
    ASSERT_EQ(options.filePaths.size(), 1);
    ASSERT_EQ(options.filePaths[0], "dummy_log_file.log");
    ASSERT_FALSE(options.readFromStdin);
    // LogAnalyzerSettings does not directly store filePaths, it's an input source.
}

TEST_F(CLIConfigTest, NoArgsReturnsError) {
    auto result = parse({"log_analyzer"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidArgument);
}

TEST_F(CLIConfigTest, ParseMultipleLogLevels) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--level", "INFO", "--level", "DEBUG"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.filterLevels.size(), 2);
    ASSERT_EQ(options.filterLevels[0], LogLevel::INFO);
    ASSERT_EQ(options.filterLevels[1], LogLevel::DEBUG);
}

TEST_F(CLIConfigTest, InvalidLogLevel) {
    auto result = parse({"log_analyzer", "--level", "INVALID", "dummy_log_file.log"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption);
}

TEST_F(CLIConfigTest, SortByTimestampAsc) {
    auto result = parse({"log_analyzer", "--sort-by", "time", "--order", "asc", "dummy_log_file.log"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_TRUE(options.sortBy.has_value());
    ASSERT_EQ(options.sortBy.value(), SortBy::TIMESTAMP);
    ASSERT_TRUE(options.sortOrder.has_value());
    ASSERT_EQ(options.sortOrder.value(), SortOrder::ASCENDING);
    // Verify LogAnalyzerSettings through ExportSettings
    ASSERT_TRUE(settings.exportSettings.sortBy.has_value());
    ASSERT_EQ(settings.exportSettings.sortBy.value(), SortBy::TIMESTAMP);
    ASSERT_TRUE(settings.exportSettings.sortOrder.has_value());
    ASSERT_EQ(settings.exportSettings.sortOrder.value(), SortOrder::ASCENDING);
}

TEST_F(CLIConfigTest, ReadFromStdin) {
    auto result = parse({"log_analyzer", "--stdin"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.readFromStdin);
}

TEST_F(CLIConfigTest, ReadFromStdinWithDash) {
    auto result = parse({"log_analyzer", "-"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.readFromStdin);
    ASSERT_TRUE(options.filePaths.empty());
}

TEST_F(CLIConfigTest, StdinAndFileError) {
    auto result = parse({"log_analyzer", "--stdin", "dummy_log_file.log"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidArgument);
}

TEST_F(CLIConfigTest, ParseMultipleFilePaths) {
    std::ofstream dummy_file2("dummy_log_file2.log");
    dummy_file2 << "more dummy content\n";
    dummy_file2.close();

    auto result = parse({"log_analyzer", "dummy_log_file.log", "dummy_log_file2.log"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;

    ASSERT_EQ(options.filePaths.size(), 2);
    ASSERT_EQ(options.filePaths[0], "dummy_log_file.log");
    ASSERT_EQ(options.filePaths[1], "dummy_log_file2.log");
    // LogAnalyzerSettings does not directly store filePaths, it's an input source.
    
    std::filesystem::remove("dummy_log_file2.log");
}

TEST_F(CLIConfigTest, OutputJsonOption) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--format", "json"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();

    ASSERT_EQ(options.outputFormat, "json");
    ASSERT_EQ(settings.exportSettings.format, ExportFormat::JSON);
}

TEST_F(CLIConfigTest, OutputTextOption) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--format", "text"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();

    ASSERT_EQ(options.outputFormat, "text");
    ASSERT_EQ(settings.exportSettings.format, ExportFormat::TEXT);
}

TEST_F(CLIConfigTest, NoColorOption) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--color", "never"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();

    ASSERT_EQ(options.colorOption, CLIConfig::ColorOption::NEVER);
    ASSERT_TRUE(settings.exportSettings.outputNoColor);
}

TEST_F(CLIConfigTest, FilterKeyword) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--keyword", "error"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.filterKeywords.size(), 1);
    ASSERT_EQ(options.filterKeywords[0], "error");
    // Verification of filterRules in LogAnalyzerSettings is complex and depends on CLIConfig's internal conversion
}

TEST_F(CLIConfigTest, FilterMultipleKeywords) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--keyword", "error", "--keyword", "warn"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.filterKeywords.size(), 2);
    ASSERT_EQ(options.filterKeywords[0], "error");
    ASSERT_EQ(options.filterKeywords[1], "warn");
}

TEST_F(CLIConfigTest, ExcludeKeyword) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--exclude-keyword", "debug"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.excludeKeywords.size(), 1);
    ASSERT_EQ(options.excludeKeywords[0], "debug");
}

TEST_F(CLIConfigTest, KeywordCaseSensitive) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--keyword", "error", "--case-sensitive"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.keywordCaseSensitive);
    // LogAnalyzerSettings.caseSensitiveParsing is for regex parsing, not filter matching.
}

TEST_F(CLIConfigTest, FilterRegex) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--regex", ".*ERROR.*"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.regexPatterns.size(), 1);
    ASSERT_EQ(options.regexPatterns[0], ".*ERROR.*");
}

TEST_F(CLIConfigTest, ExcludeRegex) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--exclude-regex", ".*DEBUG.*"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.excludeRegexPatterns.size(), 1);
    ASSERT_EQ(options.excludeRegexPatterns[0], ".*DEBUG.*");
}

TEST_F(CLIConfigTest, FilterLogic) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--logic", "OR"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.filterLogic.has_value());
    ASSERT_EQ(options.filterLogic.value(), CompositeFilter::Logic::OR);
}

TEST_F(CLIConfigTest, ComplexFilterExpression) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--expression", "level == INFO AND message contains 'user'"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.complexFilterExpression, "level == INFO AND message contains 'user'");
    // settings.rootFilterExpression will be populated after parsing the string, not directly comparable here
}

TEST_F(CLIConfigTest, FilterStartTimeISO) {
    std::string time_str = "2023-01-01 10:00:00";
    auto expected_time = Utils::parseTime(time_str).value();
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--start", time_str.c_str()});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.startTime.has_value());
    ASSERT_EQ(options.startTime.value(), expected_time);
    // These options contribute to filterRules, not direct LogAnalyzerSettings members
}

TEST_F(CLIConfigTest, FilterEndTimeRelative) {
    std::string time_str = "1 hour ago";
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--end", time_str.c_str()});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.endTime.has_value());

    auto now = std::chrono::system_clock::now();
    ASSERT_LT(options.endTime.value(), now);
    ASSERT_GT(options.endTime.value(), now - std::chrono::hours(2));
    // These options contribute to filterRules, not direct LogAnalyzerSettings members
}

TEST_F(CLIConfigTest, FilterDurationWithStartTime) {
    std::string start_time_str = "2023-01-01 00:00:00";
    std::string duration_str = "1h";
    auto expected_start_time = Utils::parseTime(start_time_str).value();
    auto expected_end_time = expected_start_time + std::chrono::hours(1);

    auto result = parse({"log_analyzer", "dummy_log_file.log", "--start", start_time_str.c_str(), "--duration", duration_str.c_str()});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    
    ASSERT_TRUE(options.startTime.has_value());
    ASSERT_EQ(options.startTime.value(), expected_start_time);
    ASSERT_TRUE(options.endTime.has_value());
    ASSERT_EQ(options.endTime.value(), expected_end_time);
    ASSERT_TRUE(options.duration.has_value());
    ASSERT_EQ(options.duration.value(), std::chrono::hours(1));
    // These options contribute to filterRules, not direct LogAnalyzerSettings members
}



TEST_F(CLIConfigTest, OptionValueMissing) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--level"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption); // CLI11 throws RequiredError
}

TEST_F(CLIConfigTest, UnknownOptionError) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--unknown-option"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption); // CLI11 throws Error
}

TEST_F(CLIConfigTest, HelpOption) {
    auto result = parse({"log_analyzer", "--help"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption); // CLI11 throws CallForHelp
}

TEST_F(CLIConfigTest, VersionOption) {
    // Assuming --version exists and behaves like --help for now, might need adjustment
    // if CLIConfig handles --version differently (e.g., custom callback)
    auto result = parse({"log_analyzer", "--version"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption); // CLI11 throws CallForHelp if version is not explicitly handled
}

TEST_F(CLIConfigTest, DurationWithoutTimeBoundariesError) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--duration", "1h"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidArgument);
}

TEST_F(CLIConfigTest, LogLevelCaseInsensitivity) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--level", "debug", "--level", "WARNING"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.filterLevels.size(), 2);
    ASSERT_EQ(options.filterLevels[0], LogLevel::DEBUG);
    ASSERT_EQ(options.filterLevels[1], LogLevel::WARNING);
}

TEST_F(CLIConfigTest, SortByCaseInsensitivity) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--sort-by", "TIME"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.sortBy.has_value());
    ASSERT_EQ(options.sortBy.value(), SortBy::TIMESTAMP);
}

TEST_F(CLIConfigTest, SortOrderCaseInsensitivity) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--order", "DESC"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.sortOrder.has_value());
    ASSERT_EQ(options.sortOrder.value(), SortOrder::DESCENDING);
}

TEST_F(CLIConfigTest, OutputFormatCaseInsensitivity) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--format", "JSON"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.outputFormat, "json"); // Stored as lowercase
    ASSERT_EQ(settings.exportSettings.format, ExportFormat::JSON);
}

TEST_F(CLIConfigTest, ColorOptionCaseInsensitivity) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--color", "ALWAYS"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.colorOption, CLIConfig::ColorOption::ALWAYS);
    ASSERT_FALSE(settings.exportSettings.outputNoColor); // "always" implies color is not disabled
}

TEST_F(CLIConfigTest, ParserErrorActionCaseInsensitivity) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--on-parse-error", "IGNORE"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.parserErrorAction, CLIConfig::ParserErrorAction::Ignore);
}

TEST_F(CLIConfigTest, DefaultValues) {
    auto result = parse({"log_analyzer", "dummy_log_file.log"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();

    // Verify CLIOptions default values
    ASSERT_TRUE(options.filterLevels.empty());
    ASSERT_FALSE(options.minLogLevel.has_value());
    ASSERT_TRUE(options.filterKeywords.empty());
    ASSERT_TRUE(options.excludeKeywords.empty());
    ASSERT_FALSE(options.keywordCaseSensitive);
    ASSERT_TRUE(options.regexPatterns.empty());
    ASSERT_TRUE(options.excludeRegexPatterns.empty());
    ASSERT_FALSE(options.filterLogic.has_value());
    ASSERT_FALSE(options.startTime.has_value());
    ASSERT_FALSE(options.endTime.has_value());
    ASSERT_FALSE(options.duration.has_value());
    ASSERT_FALSE(options.sortBy.has_value());
    ASSERT_FALSE(options.sortOrder.has_value());
    ASSERT_EQ(options.outputFormat, "text");
    ASSERT_TRUE(options.outputPath.empty());
    ASSERT_EQ(options.textOutputFormat, "{timestamp} {level}: {message}");
    ASSERT_FALSE(options.includeSummary);
    ASSERT_FALSE(options.prettyPrint);
    ASSERT_EQ(options.colorOption, CLIConfig::ColorOption::AUTO);
    ASSERT_EQ(options.csvSeparator, ',');
    ASSERT_TRUE(options.csvFields.empty());
    ASSERT_EQ(options.topMessagesCount, 10);
    ASSERT_FALSE(options.streamMode);
    ASSERT_EQ(options.parserErrorAction, CLIConfig::ParserErrorAction::Warn);
    ASSERT_FALSE(options.tailMode);
    ASSERT_EQ(options.tailInterval, std::chrono::milliseconds(1000));
    ASSERT_TRUE(options.complexFilterExpression.empty());
    ASSERT_TRUE(options.jsonFields.empty());
    ASSERT_TRUE(options.enabledStatistics.empty());
    ASSERT_FALSE(options.statsWindow.has_value());
    ASSERT_FALSE(options.findGapsDuration.has_value());
    ASSERT_FALSE(options.readFromStdin); // Should be false if file path provided

    // Verify LogAnalyzerSettings default values (based on CLIConfig defaults)
    ASSERT_EQ(settings.lineParsePattern, DEFAULT_LOG_REGEX_PATTERN_INTERNAL); // Corrected
    ASSERT_TRUE(settings.filterRules.empty()); // No filters by default
    ASSERT_FALSE(settings.rootFilterExpression.has_value());
    ASSERT_EQ(settings.exportSettings.outputPath, "");
    ASSERT_EQ(settings.exportSettings.format, ExportFormat::TEXT);
    ASSERT_EQ(settings.exportSettings.textOutputFormat, "{timestamp} {level}: {message}");
    ASSERT_FALSE(settings.exportSettings.includeSummary);
    ASSERT_FALSE(settings.exportSettings.prettyPrint);
    ASSERT_FALSE(settings.exportSettings.outputNoColor); // AUTO implies not explicitly no-color
    ASSERT_EQ(settings.exportSettings.csvSeparator, ',');
    ASSERT_TRUE(settings.exportSettings.csvFields.empty());
    ASSERT_TRUE(settings.exportSettings.jsonFields.empty());
    ASSERT_EQ(settings.exportSettings.topMessagesCount, 10); // Default for ExportSettings
    ASSERT_FALSE(settings.exportSettings.streamMode);
    ASSERT_FALSE(settings.exportSettings.tailMode);
    ASSERT_EQ(settings.exportSettings.tailInterval, std::chrono::milliseconds(1000));
    ASSERT_EQ(settings.parserErrorAction, CLIConfig::ParserErrorAction::Warn);
    ASSERT_TRUE(settings.customLogLevelMappings.empty());
    ASSERT_TRUE(settings.statisticConfigs.empty());
}

TEST_F(CLIConfigTest, CustomParsePattern) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--pattern", "^\\[(\\d+)\\](.*)$"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.lineParsePattern, "^\\[(\\d+)\\](.*)$");
    ASSERT_EQ(settings.lineParsePattern, "^\\[(\\d+)\\](.*)$");
}

TEST_F(CLIConfigTest, OutputPath) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--output", "output.txt"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.outputPath, "output.txt");
    ASSERT_EQ(settings.exportSettings.outputPath, "output.txt");
}

TEST_F(CLIConfigTest, CustomTextOutputFormat) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--text-format", "{level}: {message}"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.textOutputFormat, "{level}: {message}");
    ASSERT_EQ(settings.exportSettings.textOutputFormat, "{level}: {message}");
}

TEST_F(CLIConfigTest, IncludeSummary) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--include-summary"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_TRUE(options.includeSummary);
    ASSERT_TRUE(settings.exportSettings.includeSummary);
}

TEST_F(CLIConfigTest, PrettyPrint) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--pretty"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_TRUE(options.prettyPrint);
    ASSERT_TRUE(settings.exportSettings.prettyPrint);
}

TEST_F(CLIConfigTest, ColorOptionAuto) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--color", "auto"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.colorOption, CLIConfig::ColorOption::AUTO);
    ASSERT_FALSE(settings.exportSettings.outputNoColor); // Default behavior for AUTO
}

TEST_F(CLIConfigTest, CsvSeparatorAndFields) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--csv-sep", ";", "--csv-fields", "level,message"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.csvSeparator, ';');
    ASSERT_EQ(options.csvFields.size(), 2);
    ASSERT_EQ(options.csvFields[0], "level");
    ASSERT_EQ(options.csvFields[1], "message");
    ASSERT_EQ(settings.exportSettings.csvSeparator, ';');
    ASSERT_EQ(settings.exportSettings.csvFields.size(), 2);
    ASSERT_EQ(settings.exportSettings.csvFields[0], "level");
    ASSERT_EQ(settings.exportSettings.csvFields[1], "message");
}

TEST_F(CLIConfigTest, JsonFields) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--json-fields", "timestamp,level"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.jsonFields.size(), 2);
    ASSERT_EQ(options.jsonFields[0], "timestamp");
    ASSERT_EQ(options.jsonFields[1], "level");
    ASSERT_EQ(settings.exportSettings.jsonFields.size(), 2);
    ASSERT_EQ(settings.exportSettings.jsonFields[0], "timestamp");
    ASSERT_EQ(settings.exportSettings.jsonFields[1], "level");
}

TEST_F(CLIConfigTest, EnabledStatistics) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--stats", "unique_messages,top_messages:5"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second; // `settings.statisticConfigs` is complex
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

TEST_F(CLIConfigTest, StreamMode) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--stream"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_TRUE(options.streamMode);
    ASSERT_TRUE(settings.exportSettings.streamMode);
}

TEST_F(CLIConfigTest, CustomLogLevelMapping) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--map-level", "CRITICAL=FATAL", "--map-level", "VERBOSE=DEBUG"});
    ASSERT_TRUE(result.has_value());
    auto& settings = result.value().first;
    ASSERT_EQ(settings.customLogLevelMappings.size(), 2);
    ASSERT_EQ(settings.customLogLevelMappings.at("CRITICAL"), LogLevel::FATAL);
    ASSERT_EQ(settings.customLogLevelMappings.at("VERBOSE"), LogLevel::DEBUG);
}

TEST_F(CLIConfigTest, StatsWindow) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--stats-window", "300"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.statsWindow.has_value());
    ASSERT_EQ(options.statsWindow.value(), std::chrono::seconds(300));
    // Verification of settings.statisticConfigs is complex
}

TEST_F(CLIConfigTest, FindGapsDuration) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--find-gaps", "5000"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.findGapsDuration.has_value());
    ASSERT_EQ(options.findGapsDuration.value(), std::chrono::milliseconds(5000));
    // Verification of settings.statisticConfigs is complex
}

TEST_F(CLIConfigTest, OnParseErrorThrow) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--on-parse-error", "throw"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.parserErrorAction, CLIConfig::ParserErrorAction::Throw);
    ASSERT_EQ(settings.parserErrorAction, CLIConfig::ParserErrorAction::Throw);
}

TEST_F(CLIConfigTest, OnParseErrorWarn) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--on-parse-error", "warn"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.parserErrorAction, CLIConfig::ParserErrorAction::Warn);
    ASSERT_EQ(settings.parserErrorAction, CLIConfig::ParserErrorAction::Warn);
}

TEST_F(CLIConfigTest, TailMode) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--tail"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_TRUE(options.tailMode);
    ASSERT_TRUE(settings.exportSettings.tailMode);
}

TEST_F(CLIConfigTest, TailInterval) {
    auto result = parse({"log_analyzer", "dummy_log_file.log", "--tail", "--tail-interval", "500"});
    ASSERT_TRUE(result.has_value());
    auto& [settings, options] = result.value();
    ASSERT_EQ(options.tailInterval, std::chrono::milliseconds(500));
    ASSERT_EQ(settings.exportSettings.tailInterval, std::chrono::milliseconds(500));
}

TEST_F(CLIConfigTest, NonExistentFilePath) {
    auto result = parse({"log_analyzer", "non_existent_file.log"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.filePaths.size(), 1);
    ASSERT_EQ(options.filePaths[0], "non_existent_file.log");
    // LogAnalyzerSettings does not directly store filePaths
}
