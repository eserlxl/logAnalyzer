#include "gtest/gtest.h"
#include "config/CLIConfig.h"
#include "core/LogTypes.h"
#include "core/Error.h"

// Test fixture for CLIConfig tests
class CLIConfigTest : public ::testing::Test {
protected:
    // Helper function to call parseCLI with a vector of C-style strings
    ErrorCode::Result<std::pair<LogAnalyzerSettings, CLIConfig::CLIOptions>> parse(std::vector<const char*> args) {
        return CLIConfig::parseCLI(args.size(), const_cast<char**>(args.data()));
    }
};

TEST_F(CLIConfigTest, ParseLogLevel) {
    auto result = parse({"log_analyzer", "--level", "INFO"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.filterLevels.size(), 1);
    ASSERT_EQ(options.filterLevels[0], LogLevel::INFO);
}

TEST_F(CLIConfigTest, NoArgsReturnsError) {
    auto result = parse({"log_analyzer"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidArgument);
}

TEST_F(CLIConfigTest, ParseMultipleLogLevels) {
    auto result = parse({"log_analyzer", "--level", "INFO", "--level", "DEBUG"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_EQ(options.filterLevels.size(), 2);
    ASSERT_EQ(options.filterLevels[0], LogLevel::INFO);
    ASSERT_EQ(options.filterLevels[1], LogLevel::DEBUG);
}

TEST_F(CLIConfigTest, InvalidLogLevel) {
    auto result = parse({"log_analyzer", "--level", "INVALID"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidCLIOption);
}

TEST_F(CLIConfigTest, SortByTimestampAsc) {
    auto result = parse({"log_analyzer", "--sort-by", "time", "--order", "asc"});
    ASSERT_TRUE(result.has_value());
    auto& options = result.value().second;
    ASSERT_TRUE(options.sortBy.has_value());
    ASSERT_EQ(options.sortBy.value(), SortBy::TIMESTAMP);
    ASSERT_TRUE(options.sortOrder.has_value());
    ASSERT_EQ(options.sortOrder.value(), SortOrder::ASCENDING);
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
    auto result = parse({"log_analyzer", "--stdin", "some/file.log"});
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, Code::InvalidArgument);
}
