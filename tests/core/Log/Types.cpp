// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "core/Log/Types.h"
#include "core/Error.h"
#include <nlohmann/json.hpp>
#include <vector>

// Test fixture for LogEntry tests
class LogEntryTest : public ::testing::Test {
protected:
    LogEntry entry1;
    LogEntry entry2;

    void SetUp() override {
        // Initialize with identical values
        entry1 = {
            1,
            "test.log",
            100,
            std::chrono::system_clock::now(),
            LogLevel::INFO,
            "This is a test message.",
            "thread-1",
            "module-A",
            "host-1",
            {{"key1", "value1"}},
            "raw_structured_data",
            {ErrorCode::Error(Code::MalformedLogEntry, "Error parsing field")}
        };
        entry2 = entry1; // Exact copy
    }
};

TEST_F(LogEntryTest, EqualityOperator_IdenticalEntries) {
    // Using the defaulted operator==
    EXPECT_TRUE(entry1 == entry2);
}

TEST_F(LogEntryTest, EqualityOperator_DifferentId) {
    entry2.id = 2;
    EXPECT_FALSE(entry1 == entry2);
}

TEST_F(LogEntryTest, EqualityOperator_DifferentSourceFile) {
    entry2.sourceFile = "other.log";
    EXPECT_FALSE(entry1 == entry2);
}

TEST_F(LogEntryTest, EqualityOperator_DifferentLineNumber) {
    entry2.sourceLineNumber = 200;
    EXPECT_FALSE(entry1 == entry2);
}

TEST_F(LogEntryTest, EqualityOperator_DifferentTimestamp) {
    entry2.timestamp = std::chrono::system_clock::now() + std::chrono::seconds(1);
    EXPECT_FALSE(entry1 == entry2);
}

TEST_F(LogEntryTest, EqualityOperator_DifferentLogLevel) {
    entry2.level = LogLevel::WARNING;
    EXPECT_FALSE(entry1 == entry2);
}

TEST_F(LogEntryTest, EqualityOperator_DifferentMessage) {
    entry2.message = "A different message.";
    EXPECT_FALSE(entry1 == entry2);
}

TEST_F(LogEntryTest, EqualityOperator_DifferentThreadId) {
    entry2.threadId = "thread-2";
    EXPECT_FALSE(entry1 == entry2);
}

TEST_F(LogEntryTest, EqualityOperator_DifferentModule) {
    entry2.module = "module-B";
    EXPECT_FALSE(entry1 == entry2);
}

TEST_F(LogEntryTest, EqualityOperator_DifferentHost) {
    entry2.host = "host-2";
    EXPECT_FALSE(entry1 == entry2);
}

TEST_F(LogEntryTest, EqualityOperator_DifferentCustomFields) {
    entry2.customFields["key1"] = "newValue";
    EXPECT_FALSE(entry1 == entry2);
}

TEST_F(LogEntryTest, EqualityOperator_DifferentStructuredData) {
    entry2.structuredData = "different_raw_data";
    EXPECT_FALSE(entry1 == entry2);
}

TEST_F(LogEntryTest, EqualityOperator_DifferentParsingErrors) {
    entry2.parsingErrors.push_back(ErrorCode::Error(Code::Unknown, "Another error"));
    EXPECT_FALSE(entry1 == entry2);
}

TEST_F(LogEntryTest, GetParsingErrorsAsString) {
    entry1.parsingErrors = {
        ErrorCode::Error(Code::MalformedLogEntry, "Error 1"),
        ErrorCode::Error(Code::TimestampParsingFailed, "Error 2")
    };
    EXPECT_EQ(entry1.getParsingErrorsAsString(), "Error 1; Error 2");
}

TEST_F(LogEntryTest, GetParsingErrorsAsString_SingleError) {
    entry1.parsingErrors = {ErrorCode::Error(Code::MalformedLogEntry, "Single error")};
    EXPECT_EQ(entry1.getParsingErrorsAsString(), "Single error");
}

TEST_F(LogEntryTest, GetParsingErrorsAsString_NoErrors) {
    entry1.parsingErrors.clear();
    EXPECT_EQ(entry1.getParsingErrorsAsString(), "");
}


// --- FieldMapping Tests ---

class FieldMappingTest : public ::testing::Test {};

TEST_F(FieldMappingTest, FromJson_StandardField) {
    nlohmann::json j = {
        {"field", "TIMESTAMP"},
        {"groupIndex", 1},
        {"formats", {"%Y-%m-%d"}}
    };
    FieldMapping fm = j.get<FieldMapping>();
    ASSERT_TRUE(std::holds_alternative<LogEntryField>(fm.field));
    EXPECT_EQ(std::get<LogEntryField>(fm.field), LogEntryField::TIMESTAMP);
    EXPECT_EQ(fm.groupIndex, 1);
    EXPECT_EQ(fm.formats.size(), 1);
    EXPECT_EQ(fm.formats[0], "%Y-%m-%d");
}

TEST_F(FieldMappingTest, FromJson_CustomField) {
    nlohmann::json j = {
        {"field", "my_custom_field"},
        {"groupIndex", 2},
        {"customFieldType", "string"}
    };
    FieldMapping fm = j.get<FieldMapping>();
    ASSERT_TRUE(std::holds_alternative<std::string>(fm.field));
    EXPECT_EQ(std::get<std::string>(fm.field), "my_custom_field");
    EXPECT_EQ(fm.groupIndex, 2);
    ASSERT_TRUE(fm.customFieldType.has_value());
    EXPECT_EQ(fm.customFieldType.value(), "string");
}

TEST_F(FieldMappingTest, FromJson_CustomFieldTypeRejectedForStandardField) {
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"groupIndex", 1},
        {"customFieldType", "string"}
    };
    EXPECT_THROW(j.get<FieldMapping>(), nlohmann::json::parse_error);
}

TEST_F(FieldMappingTest, FromJson_CustomFieldTypeMustBeStringOrNull) {
    nlohmann::json j = {
        {"field", "my_custom"},
        {"groupIndex", 1},
        {"customFieldType", 123}
    };
    EXPECT_THROW(j.get<FieldMapping>(), nlohmann::json::type_error);
}

TEST_F(FieldMappingTest, FromJson_CustomFieldTypeIsTrimmed) {
    nlohmann::json j = {
        {"field", "my_custom"},
        {"groupIndex", 1},
        {"customFieldType", "  integer  "}
    };
    FieldMapping fm = j.get<FieldMapping>();
    ASSERT_TRUE(fm.customFieldType.has_value());
    EXPECT_EQ(fm.customFieldType.value(), "integer");
}

TEST_F(FieldMappingTest, FromJson_EmptyCustomFieldTypeRejected) {
    nlohmann::json j = {
        {"field", "my_custom"},
        {"groupIndex", 1},
        {"customFieldType", "   "}
    };
    EXPECT_THROW(j.get<FieldMapping>(), nlohmann::json::parse_error);
}

TEST_F(FieldMappingTest, FromJson_FieldNameIsTrimmed) {
    nlohmann::json j = {
        {"field", "  MESSAGE \t"},
        {"groupIndex", 2}
    };
    FieldMapping fm = j.get<FieldMapping>();
    ASSERT_TRUE(std::holds_alternative<LogEntryField>(fm.field));
    EXPECT_EQ(std::get<LogEntryField>(fm.field), LogEntryField::MESSAGE);
}

TEST_F(FieldMappingTest, FromJson_StructuredFieldWithRegex) {
    nlohmann::json j = {
        {"field", "STRUCTURED_FIELD"},
        {"groupIndex", 3},
        {"formats", {"(user|id)=(\\w+)"}} // The regex pattern
    };
    FieldMapping fm = j.get<FieldMapping>();
    ASSERT_TRUE(std::holds_alternative<LogEntryField>(fm.field));
    EXPECT_EQ(std::get<LogEntryField>(fm.field), LogEntryField::STRUCTURED_FIELD);
    ASSERT_TRUE(fm.compiledKvPattern != nullptr); // Check that the regex was compiled

    // Test the compiled regex
    std::string test_str = "id=testuser";
    std::smatch match;
    EXPECT_TRUE(std::regex_search(test_str, match, *fm.compiledKvPattern));
    ASSERT_EQ(match.size(), 3);
    EXPECT_EQ(match[1].str(), "id");
    EXPECT_EQ(match[2].str(), "testuser");
}

TEST_F(FieldMappingTest, FromJson_StructuredFieldUsesFirstNonEmptyRegexPattern) {
    nlohmann::json j = {
        {"field", "STRUCTURED_FIELD"},
        {"groupIndex", 3},
        {"formats", {"   ", R"((key)=(\w+))"}}
    };
    FieldMapping fm = j.get<FieldMapping>();
    ASSERT_TRUE(fm.compiledKvPattern != nullptr);
    std::string test_str = "key=value";
    std::smatch match;
    EXPECT_TRUE(std::regex_search(test_str, match, *fm.compiledKvPattern));
    ASSERT_EQ(match.size(), 3);
    EXPECT_EQ(match[1].str(), "key");
    EXPECT_EQ(match[2].str(), "value");
}

TEST_F(FieldMappingTest, FromJson_InvalidRegex) {
    nlohmann::json j = {
        {"field", "STRUCTURED_FIELD"},
        {"groupIndex", 1},
        {"formats", {"["}} // Invalid regex
    };
    EXPECT_THROW(j.get<FieldMapping>(), nlohmann::json::parse_error);
}

TEST_F(FieldMappingTest, FromJson_StructuredFieldMissingFormatsRejected) {
    nlohmann::json j = {
        {"field", "STRUCTURED_FIELD"},
        {"groupIndex", 1}
    };
    EXPECT_THROW(j.get<FieldMapping>(), nlohmann::json::parse_error);
}

TEST_F(FieldMappingTest, FromJson_StructuredFieldEmptyFormatsRejected) {
    nlohmann::json j = {
        {"field", "STRUCTURED_FIELD"},
        {"groupIndex", 1},
        {"formats", {"", "   "}}
    };
    EXPECT_THROW(j.get<FieldMapping>(), nlohmann::json::parse_error);
}

TEST_F(FieldMappingTest, FromJson_MissingField) {
    nlohmann::json j = {
        {"groupIndex", 1}
    };
    EXPECT_THROW(j.get<FieldMapping>(), nlohmann::json::parse_error);
}

TEST_F(FieldMappingTest, FromJson_EmptyFieldRejected) {
    nlohmann::json j = {
        {"field", "   "},
        {"groupIndex", 1}
    };
    EXPECT_THROW(j.get<FieldMapping>(), nlohmann::json::parse_error);
}

TEST_F(FieldMappingTest, FromJson_MissingGroupIndex) {
    nlohmann::json j = {
        {"field", "MESSAGE"}
    };
    EXPECT_THROW(j.get<FieldMapping>(), nlohmann::json::parse_error);
}

TEST_F(FieldMappingTest, FromJson_InvalidGroupIndexType) {
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"groupIndex", "one"}
    };
    EXPECT_THROW(j.get<FieldMapping>(), nlohmann::json::type_error);
}

TEST_F(FieldMappingTest, FromJson_NegativeGroupIndexRejected) {
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"groupIndex", -1}
    };
    EXPECT_THROW(j.get<FieldMapping>(), nlohmann::json::type_error);
}

TEST_F(FieldMappingTest, FromJson_InvalidFormatsType) {
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"groupIndex", 1},
        {"formats", "not-an-array"}
    };
    EXPECT_THROW(j.get<FieldMapping>(), nlohmann::json::type_error);
}

TEST_F(FieldMappingTest, FromJson_NullFormatsClearsFormats) {
    FieldMapping fm(LogEntryField::MESSAGE, std::make_optional<size_t>(1), std::vector<std::string>{"%Y-%m-%d"});
    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"groupIndex", 1},
        {"formats", nullptr}
    };
    j.get_to(fm);
    EXPECT_TRUE(fm.formats.empty());
}

TEST_F(FieldMappingTest, FromJsonGetTo_ResetsPreviousState) {
    std::vector<std::string> formats = {"%Y-%m-%d"};
    FieldMapping fm("old_custom", std::make_optional<size_t>(7), formats, std::make_optional<std::string>("string"));
    fm.compiledKvPattern = std::make_shared<const std::regex>("old");

    nlohmann::json j = {
        {"field", "MESSAGE"},
        {"groupIndex", 2}
    };

    j.get_to(fm);
    ASSERT_TRUE(std::holds_alternative<LogEntryField>(fm.field));
    EXPECT_EQ(std::get<LogEntryField>(fm.field), LogEntryField::MESSAGE);
    EXPECT_EQ(fm.groupIndex, 2);
    EXPECT_TRUE(fm.formats.empty());
    EXPECT_FALSE(fm.customFieldType.has_value());
    EXPECT_TRUE(fm.compiledKvPattern == nullptr);
}


TEST_F(FieldMappingTest, ToJson_StandardField) {
    FieldMapping fm(LogEntryField::MESSAGE, std::make_optional<size_t>(2));
    nlohmann::json j = fm;
    EXPECT_EQ(j["field"], "MESSAGE");
    EXPECT_EQ(j["groupIndex"], 2);
}

TEST_F(FieldMappingTest, ToJson_CustomField) {
    // Explicitly use std::vector<std::string> to avoid ambiguity
    std::vector<std::string> formats = {};
    FieldMapping fm("my_field", std::make_optional<size_t>(3), formats, std::make_optional<std::string>("integer"));
    nlohmann::json j = fm;
    EXPECT_EQ(j["field"], "my_field");
    EXPECT_EQ(j["groupIndex"], 3);
    EXPECT_EQ(j["customFieldType"], "integer");
}

TEST_F(FieldMappingTest, CopyConstructor_SharesRegex) {
    FieldMapping fm1;
    fm1.field = LogEntryField::STRUCTURED_FIELD;
    fm1.formats = {"test_regex"};
    fm1.compiledKvPattern = std::make_shared<std::regex>("test_regex");

    FieldMapping fm2 = fm1; // Copy constructor

    // The shared_ptr should be copied, not the regex object itself.
    // Both pointers should point to the same memory address.
    EXPECT_EQ(fm1.compiledKvPattern.get(), fm2.compiledKvPattern.get());
    EXPECT_EQ(fm1.compiledKvPattern.use_count(), 2);
}

TEST(DeprecatedConstructors, FieldMappingLegacy) {
    // This test is to ensure the deprecated constructors still compile and work.
    // Suppress deprecation warnings for this specific test case.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

    FieldMapping fm1(LogEntryField::MESSAGE, 1, "%s");
    EXPECT_EQ(fm1.groupIndex.value(), 1);
    EXPECT_EQ(fm1.formats[0], "%s");

    const char* fmt = "%d";
    FieldMapping fm2(LogEntryField::LINE_NUMBER, 2, fmt);
     EXPECT_EQ(fm2.groupIndex.value(), 2);
    EXPECT_EQ(fm2.formats[0], "%d");


#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
