// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "filter/ConcreteFilters.h"
#include "utils/String.h"
#include "utils/Core.h"
#include <charconv>
#include <regex>
#include <cmath>
#include <limits>

namespace filter {

namespace {
    // A more robust floating-point comparison
    bool areAlmostEqual(double a, double b) {
        constexpr double relative_epsilon = 1e-9;
        return std::abs(a - b) <= relative_epsilon * std::max(1.0, std::max(std::abs(a), std::abs(b)));
    }

    std::optional<std::string> getFieldValue(const LogEntry& entry, const std::string& fieldKey) {
        if (auto it = entry.customFields.find(fieldKey); it != entry.customFields.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::optional<double> tryParseDouble(std::string_view str) {
        double value;
        auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
        if (ec == std::errc()) {
            return value;
        }
        return std::nullopt;
    }

    std::optional<bool> tryParseBool(std::string_view str) {
        std::string trimmedStr = Utils::trim(std::string(str), " \t\n\r\f\v");
        std::string lowerStr = Utils::toLower(trimmedStr);

        if (lowerStr == "true" || lowerStr == "1" || lowerStr == "t" || lowerStr == "yes") {
            return true;
        } else if (lowerStr == "false" || lowerStr == "0" || lowerStr == "f" || lowerStr == "no") {
            return false;
        }
        return std::nullopt;
    }

    std::optional<std::regex> createRegexForPattern(const std::string& pattern, PatternType type, bool caseSensitive) {
        if (type == PatternType::Literal) {
            return std::nullopt;
        }

        auto flags = std::regex::ECMAScript;
        if (!caseSensitive) {
            flags |= std::regex::icase;
        }

        if (type == PatternType::Wildcard) {
            std::string regexStr = Utils::globToRegex(pattern);
            return std::regex(regexStr, flags);
        } else { // Regex
            return std::regex(pattern, flags);
        }
    }
}

NumericComparisonFilter::NumericComparisonFilter(std::string fieldKey, double value, Operator op)
    : fieldKey_(std::move(fieldKey)), value_(value), op_(op) {}

bool NumericComparisonFilter::matches(const LogEntry &entry) const {
    auto valueStrOpt = getFieldValue(entry, fieldKey_);
    if (!valueStrOpt) {
        return false;
    }

    if (auto actualValueOpt = tryParseDouble(*valueStrOpt)) {
        double actualValue = *actualValueOpt;
        switch (op_) {
            case Operator::EQ:  return areAlmostEqual(actualValue, value_);
            case Operator::NEQ: return !areAlmostEqual(actualValue, value_);
            case Operator::GT:  return actualValue > value_;
            case Operator::LT:  return actualValue < value_;
            case Operator::GTE: return actualValue >= value_ || areAlmostEqual(actualValue, value_);
            case Operator::LTE: return actualValue <= value_ || areAlmostEqual(actualValue, value_);
        }
    }
    return false;
}

BoolFilter::BoolFilter(std::string fieldKey, bool value, bool caseSensitive)
    : fieldKey_(std::move(fieldKey)), value_(value), caseSensitive_(caseSensitive) {}

bool BoolFilter::matches(const LogEntry &entry) const {
    auto valueStrOpt = getFieldValue(entry, fieldKey_);
    if (!valueStrOpt) {
        return false;
    }

    if (caseSensitive_) {
        // Strict parsing for case-sensitive mode
        if (value_) {
            return (*valueStrOpt == "true" || *valueStrOpt == "1");
        } else {
            return (*valueStrOpt == "false" || *valueStrOpt == "0");
        }
    }

    if (auto actualValueOpt = tryParseBool(*valueStrOpt)) {
        return *actualValueOpt == value_;
    }
    return false;
}


FieldValueFilter::FieldValueFilter(std::string fieldKey, std::string valuePattern, PatternType type, bool caseSensitive)
    : fieldKey_(std::move(fieldKey)),
      valuePattern_(std::move(valuePattern)),
      type_(type),
      caseSensitive_(caseSensitive),
      regexPattern_(createRegexForPattern(valuePattern_, type_, caseSensitive_)) {}

bool FieldValueFilter::matches(const LogEntry &entry) const {
    auto actualValueOpt = getFieldValue(entry, fieldKey_);
    if (!actualValueOpt) {
        return false;
    }

    const std::string& actualValue = *actualValueOpt;

    if (type_ == PatternType::Literal) {
        if (caseSensitive_) {
            return actualValue == valuePattern_;
        } else {
            return Utils::caseInsensitiveEquals(actualValue, valuePattern_);
        }
    }
    else {
        if (regexPattern_.has_value()) {
            return std::regex_search(actualValue, *regexPattern_);
        }
        return false; // Should not happen if constructor logic is correct
    }
}

// --- Dotted-Key Filters: Implemented by passing the dotted path to the base filter ---

DottedKeyFieldValueFilter::DottedKeyFieldValueFilter(std::string fieldPath,
                                               std::string valuePattern,
                                               PatternType type,
                                               bool caseSensitive)
    : FieldValueFilter(std::move(fieldPath), std::move(valuePattern), type, caseSensitive) {}


DottedKeyNumericComparisonFilter::DottedKeyNumericComparisonFilter(std::string fieldPath,
                                                           double value,
                                                           NumericComparisonFilter::Operator op)
    : NumericComparisonFilter(std::move(fieldPath), value, op) {}


DottedKeyBoolFilter::DottedKeyBoolFilter(std::string fieldPath, bool value)
    : BoolFilter(std::move(fieldPath), value, false) {} // Keep original behavior: case-insensitive


ValueSetFilter::ValueSetFilter(std::string fieldKey, std::set<std::string> values, bool caseSensitive)
    : fieldKey_(std::move(fieldKey)), caseSensitive_(caseSensitive) {
    if (caseSensitive_) {
        // For case-sensitive, we need a different set
        valueSetSensitive_ = std::move(values);
    } else {
        for (const auto& val : values) {
            valueSetInsensitive_.insert(val);
        }
    }
}

bool ValueSetFilter::matches(const LogEntry &entry) const {
    auto actualValueOpt = getFieldValue(entry, fieldKey_);
    if (!actualValueOpt) {
        return false;
    }

    if (caseSensitive_) {
        return valueSetSensitive_.count(*actualValueOpt) > 0;
    } else {
        return valueSetInsensitive_.count(*actualValueOpt) > 0;
    }
}

DottedKeyValueSetFilter::DottedKeyValueSetFilter(std::string fieldPath, std::set<std::string> values, bool caseSensitive)
    : ValueSetFilter(std::move(fieldPath), std::move(values), caseSensitive) {}


SourceFileFilter::SourceFileFilter(std::string pattern, PatternType type, bool caseSensitive)
    : pattern_(std::move(pattern)),
      type_(type),
      caseSensitive_(caseSensitive),
      regexPattern_(createRegexForPattern(pattern_, type_, caseSensitive_)) {}

bool SourceFileFilter::matches(const LogEntry &entry) const {
    if (type_ == PatternType::Literal) {
        if (caseSensitive_) {
            return entry.sourceFile == pattern_;
        } else {
            return Utils::caseInsensitiveEquals(entry.sourceFile, pattern_);
        }
    } else {
        if (regexPattern_.has_value()) {
            return std::regex_search(entry.sourceFile, *regexPattern_);
        }
        return false;
    }
}

FieldExistsFilter::FieldExistsFilter(std::string fieldKey) : fieldKey_(std::move(fieldKey)) {}

bool FieldExistsFilter::matches(const LogEntry &entry) const {
    return entry.customFields.count(fieldKey_) > 0;
}

PredicateFilter::PredicateFilter(PredicateFilter::Predicate predicate) : predicate_(std::move(predicate)) {}

bool PredicateFilter::matches(const LogEntry &entry) const {
    if (predicate_) {
        return predicate_(entry);
    }
    return false;
}

LogLevelSetFilter::LogLevelSetFilter(std::set<LogLevel> allowedLevels) : allowedLevels_(std::move(allowedLevels)) {}

bool LogLevelSetFilter::matches(const LogEntry &entry) const {
    return allowedLevels_.count(entry.level) > 0;
}

KeywordFilter::KeywordFilter(std::string keyword, bool isCaseSensitive)
    : keywords_({std::move(keyword)}), logic_(Logic::ANY), isCaseSensitive_(isCaseSensitive) {}

KeywordFilter::KeywordFilter(std::vector<std::string> keywords, Logic logic, bool isCaseSensitive)
    : keywords_(std::move(keywords)), logic_(logic), isCaseSensitive_(isCaseSensitive) {}

bool KeywordFilter::matches(const LogEntry &entry) const {
    if (keywords_.empty()) {
        return logic_ == Logic::ALL; // Consistent with CompositeFilter
    }
    
    auto search_fn = [this](const std::string& text, const std::string& keyword) {
        if (isCaseSensitive_) {
            return text.find(keyword) != std::string::npos;
        } else {
            return Utils::caseInsensitiveSearch(text, keyword);
        }
    };

    if (logic_ == Logic::ANY) {
        return std::any_of(keywords_.begin(), keywords_.end(), [&](const auto& kw) {
            return search_fn(entry.message, kw);
        });
    } else { // ALL
        return std::all_of(keywords_.begin(), keywords_.end(), [&](const auto& kw) {
            return search_fn(entry.message, kw);
        });
    }
}

ErrorCode::Result<std::shared_ptr<RegexFilter>> RegexFilter::create(std::string pattern, bool caseSensitive) {
    try {
        return std::shared_ptr<RegexFilter>(new RegexFilter(std::move(pattern), caseSensitive));
    } catch (const std::regex_error& e) {
        return std::unexpected(ErrorCode::Error(Code::InvalidRegex, "Invalid regex pattern: " + std::string(e.what())));
    }
}

RegexFilter::RegexFilter(std::string pattern, std::regex_constants::syntax_option_type flags)
    : pattern_(std::move(pattern), flags) {}

RegexFilter::RegexFilter(std::string pattern, bool caseSensitive)
    : pattern_(std::move(pattern), caseSensitive ? std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase) {}
bool RegexFilter::matches(const LogEntry &entry) const {
    return std::regex_search(entry.message, pattern_);
}

TimeRangeFilter::TimeRangeFilter(std::chrono::system_clock::time_point start, std::chrono::system_clock::time_point end)
    : startTime_(start), endTime_(end) {}

bool TimeRangeFilter::matches(const LogEntry &entry) const {
    // Only match if the entry has a valid timestamp
    return entry.timestamp.has_value() &&
           entry.timestamp.value() >= startTime_ &&
           entry.timestamp.value() < endTime_;
}

std::expected<TimeRangeFilter, std::string> TimeRangeFilter::fromStrings(const std::string& start, const std::string& end) {
    auto startTime = Utils::parseAbsoluteTime(start);
    if (!startTime) {
        return std::unexpected(startTime.error().toString());
    }

    auto endTime = Utils::parseAbsoluteTime(end);
    if (!endTime) {
        return std::unexpected(endTime.error().toString());
    }

    return TimeRangeFilter(*startTime, *endTime);
}

std::expected<TimeRangeFilter, std::string> TimeRangeFilter::since(const std::string& relativeTime) {
    auto startTime = Utils::parseRelativeTime(relativeTime);
    if (!startTime) {
        return std::unexpected(startTime.error().toString());
    }
    return TimeRangeFilter(*startTime, std::chrono::system_clock::now());
}

std::expected<TimeRangeFilter, std::string> TimeRangeFilter::forDay(const std::string& dateString) {
    auto dayRange = Utils::parseDayRange(dateString);
    if (!dayRange) {
        return std::unexpected(dayRange.error().toString());
    }
    return TimeRangeFilter(dayRange->first, dayRange->second);
}

bool CompositeFilter::matches(const LogEntry &entry) const {
    if (filters_.empty()) {
        return logic_ == Logic::AND;
    }

    if (logic_ == Logic::AND) {
        return std::all_of(filters_.begin(), filters_.end(), [&](const auto& filter) {
            return filter->matches(entry);
        });
    } else { // OR
        return std::any_of(filters_.begin(), filters_.end(), [&](const auto& filter) {
            return filter->matches(entry);
        });
    }
}

// --- ExpressionFilter Implementation ---

#include "filter/Expression.h"
#include <nlohmann/json.hpp>
#include <iostream>

ExpressionFilter::ExpressionFilter(FilterExpression expression)
    : expression_(std::move(expression)) {}

ErrorCode::Result<std::shared_ptr<ExpressionFilter>> ExpressionFilter::create(const nlohmann::json& json_spec) {
    FilterExpression expr;
    if (auto result = from_json(json_spec, expr); !result) {
        return std::unexpected(result.error());
    }

    if (auto validationResult = expr.validate(); !validationResult) {
        return std::unexpected(validationResult.error());
    }
    
    return std::make_shared<ExpressionFilter>(std::move(expr));
}

bool ExpressionFilter::matches(const LogEntry &entry) const {
    auto result = expression_.evaluate(entry);
    if (!result) {
        // As per design, log error and fail-open (return true)
        std::cerr << "Error evaluating filter expression: " << result.error().message << std::endl;
        return true;
    }
    return *result;
}

} // namespace filter
