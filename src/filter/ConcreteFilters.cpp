#include "filter/ConcreteFilters.h"
#include "utils/Core.h"
#include <regex>
#include <cmath>

namespace {
    /**
     * @brief Retrieves a "nested" value from a LogEntry's custom fields.
     */
    std::optional<std::string> getNestedValue(const LogEntry& entry, const std::string& fieldPath) {
        auto it = entry.customFields.find(fieldPath);
        if (it != entry.customFields.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::optional<double> getNestedNumericValue(const LogEntry& entry, const std::string& fieldPath) {
        auto strValueOpt = getNestedValue(entry, fieldPath);
        if (strValueOpt) {
            try {
                return std::stod(*strValueOpt);
            } catch (...) {}
        }
        return std::nullopt;
    }

    std::optional<bool> getNestedBoolValue(const LogEntry& entry, const std::string& fieldPath) {
        auto strValueOpt = getNestedValue(entry, fieldPath);
        if (strValueOpt) {
            std::string lowerStr = Utils::toLower(*strValueOpt);
            if (lowerStr == "true" || lowerStr == "1" || lowerStr == "t" || lowerStr == "yes") {
                return true;
            } else if (lowerStr == "false" || lowerStr == "0" || lowerStr == "f" || lowerStr == "no") {
                return false;
            }
        }
        return std::nullopt;
    }
}

NumericComparisonFilter::NumericComparisonFilter(std::string fieldKey, double value, Operator op)
    : fieldKey_(std::move(fieldKey)), value_(value), op_(op) {}

bool NumericComparisonFilter::matches(const LogEntry &entry) const {
    auto it = entry.customFields.find(fieldKey_);
    if (it == entry.customFields.end()) {
        return false;
    }

    try {
        double actualValue = std::stod(it->second);
        switch (op_) {
            case Operator::EQ:  return std::abs(actualValue - value_) < NumericComparisonFilter::EPSILON;
            case Operator::NEQ: return std::abs(actualValue - value_) >= NumericComparisonFilter::EPSILON;
            case Operator::GT:  return actualValue > value_;
            case Operator::LT:  return actualValue < value_;
            case Operator::GTE: return actualValue >= value_;
            case Operator::LTE: return actualValue <= value_;
        }
    } catch (...) {}
    return false;
}

BoolFilter::BoolFilter(std::string fieldKey, bool value)
    : fieldKey_(std::move(fieldKey)), value_(value) {}

bool BoolFilter::matches(const LogEntry &entry) const {
    auto it = entry.customFields.find(fieldKey_);
    if (it == entry.customFields.end()) {
        return false;
    }

    std::string lowerStr = Utils::toLower(it->second);
    if (value_) {
        return (lowerStr == "true" || lowerStr == "1" || lowerStr == "t" || lowerStr == "yes");
    } else {
        return (lowerStr == "false" || lowerStr == "0" || lowerStr == "f" || lowerStr == "no");
    }
}

NestedFieldValueFilter::NestedFieldValueFilter(std::string fieldPath,
                                               std::string valuePattern,
                                               PatternType type,
                                               bool caseSensitive)
    : fieldPath_(std::move(fieldPath)),
      valuePattern_(std::move(valuePattern)),
      type_(type),
      caseSensitive_(caseSensitive)
{
    if (type_ == PatternType::Regex) {
        auto flags = std::regex::ECMAScript;
        if (!caseSensitive_) {
            flags |= std::regex::icase;
        }
        regexPattern_.emplace(valuePattern_, flags);
    } else if (type_ == PatternType::Wildcard) {
        std::string regexStr = Utils::globToRegex(valuePattern_);
        regexPattern_.emplace(regexStr, caseSensitive_ ? std::regex::ECMAScript : std::regex::icase);
    }
}

bool NestedFieldValueFilter::matches(const LogEntry &entry) const {
    auto actualValueOpt = getNestedValue(entry, fieldPath_);
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
    } else {
        if (regexPattern_.has_value()) {
            return std::regex_search(actualValue, *regexPattern_);
        }
        return false;
    }
}

NestedNumericComparisonFilter::NestedNumericComparisonFilter(std::string fieldPath,
                                                           double value,
                                                           NumericComparisonFilter::Operator op)
    : fieldPath_(std::move(fieldPath)), value_(value), op_(op) {}

bool NestedNumericComparisonFilter::matches(const LogEntry &entry) const {
    auto actualValueOpt = getNestedNumericValue(entry, fieldPath_);
    if (!actualValueOpt) {
        return false;
    }

    double actualValue = *actualValueOpt;
    switch (op_) {
        case NumericComparisonFilter::Operator::EQ:  return std::abs(actualValue - value_) < NumericComparisonFilter::EPSILON;
        case NumericComparisonFilter::Operator::NEQ: return std::abs(actualValue - value_) >= NumericComparisonFilter::EPSILON;
        case NumericComparisonFilter::Operator::GT:  return actualValue > value_;
        case NumericComparisonFilter::Operator::LT:  return actualValue < value_;
        case NumericComparisonFilter::Operator::GTE: return actualValue >= value_;
        case NumericComparisonFilter::Operator::LTE: return actualValue <= value_;
    }
    return false;
}

NestedBoolFilter::NestedBoolFilter(std::string fieldPath, bool value)
    : fieldPath_(std::move(fieldPath)), value_(value) {}

bool NestedBoolFilter::matches(const LogEntry &entry) const {
    auto actualValueOpt = getNestedBoolValue(entry, fieldPath_);
    if (!actualValueOpt) {
        return false;
    }
    return *actualValueOpt == value_;
}

ValueSetFilter::ValueSetFilter(std::string fieldKey, std::set<std::string> values, bool caseSensitive)
    : fieldKey_(std::move(fieldKey)), caseSensitive_(caseSensitive) {
    if (caseSensitive_) {
        valueSet_ = std::move(values);
    } else {
        for (const auto& val : values) {
            valueSet_.insert(Utils::toLower(val));
        }
    }
}

bool ValueSetFilter::matches(const LogEntry &entry) const {
    auto it = entry.customFields.find(fieldKey_);
    if (it == entry.customFields.end()) {
        return false;
    }

    const std::string& actualValue = it->second;
    if (caseSensitive_) {
        return valueSet_.count(actualValue) > 0;
    } else {
        return valueSet_.count(Utils::toLower(actualValue)) > 0;
    }
}

NestedValueSetFilter::NestedValueSetFilter(std::string fieldPath, std::set<std::string> values, bool caseSensitive)
    : fieldPath_(std::move(fieldPath)), caseSensitive_(caseSensitive) {
    if (caseSensitive_) {
        valueSet_ = std::move(values);
    } else {
        for (const auto& val : values) {
            valueSet_.insert(Utils::toLower(val));
        }
    }
}

bool NestedValueSetFilter::matches(const LogEntry &entry) const {
    auto actualValueOpt = getNestedValue(entry, fieldPath_);
    if (!actualValueOpt) {
        return false;
    }

    const std::string& actualValue = *actualValueOpt;
    if (caseSensitive_) {
        return valueSet_.count(actualValue) > 0;
    } else {
        return valueSet_.count(Utils::toLower(actualValue)) > 0;
    }
}

SourceFileFilter::SourceFileFilter(std::string pattern, PatternType type, bool caseSensitive)
    : pattern_(std::move(pattern)), type_(type), caseSensitive_(caseSensitive) {
    if (type_ == PatternType::Regex) {
        auto flags = std::regex::ECMAScript;
        if (!caseSensitive_) {
            flags |= std::regex::icase;
        }
        regexPattern_.emplace(pattern_, flags);
    } else if (type_ == PatternType::Wildcard) {
        std::string regexStr = Utils::globToRegex(pattern_);
        regexPattern_.emplace(regexStr, caseSensitive_ ? std::regex::ECMAScript : std::regex::icase);
    }
}

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

FieldValueFilter::FieldValueFilter(std::string fieldKey, std::string valuePattern, PatternType type, bool caseSensitive)
    : fieldKey_(std::move(fieldKey)), valuePattern_(std::move(valuePattern)), type_(type), caseSensitive_(caseSensitive) {
    if (type_ == PatternType::Regex) {
        auto flags = std::regex::ECMAScript;
        if (!caseSensitive_) {
            flags |= std::regex::icase;
        }
        regexPattern_.emplace(valuePattern_, flags);
    } else if (type_ == PatternType::Wildcard) {
        std::string regexStr = Utils::globToRegex(valuePattern_);
        regexPattern_.emplace(regexStr, caseSensitive_ ? std::regex::ECMAScript : std::regex::icase);
    }
}

bool FieldValueFilter::matches(const LogEntry &entry) const {
    auto it = entry.customFields.find(fieldKey_);
    if (it == entry.customFields.end()) {
        return false;
    }

    const std::string& actualValue = it->second;

    if (type_ == PatternType::Literal) {
        if (caseSensitive_) {
            return actualValue == valuePattern_;
        } else {
            return Utils::caseInsensitiveEquals(actualValue, valuePattern_);
        }
    } else {
        if (regexPattern_.has_value()) {
            return std::regex_search(actualValue, *regexPattern_);
        }
        return false;
    }
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
    auto search_fn = [this](const std::string& text, const std::string& keyword) {
        if (isCaseSensitive_) {
            return text.find(keyword) != std::string::npos;
        } else {
            return Utils::caseInsensitiveSearch(text, keyword);
        }
    };

    if (logic_ == Logic::ANY) {
        for (const auto& keyword : keywords_) {
            if (search_fn(entry.message, keyword)) {
                return true;
            }
        }
        return false;
    } else {
        for (const auto& keyword : keywords_) {
            if (!search_fn(entry.message, keyword)) {
                return false;
            }
        }
        return true;
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
    return entry.timestamp >= startTime_ && entry.timestamp < endTime_;
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
        for (const auto &filter : filters_) {
            if (!filter->matches(entry)) return false;
        }
        return true;
    } else {
        for (const auto &filter : filters_) {
            if (filter->matches(entry)) return true;
        }
        return false;
    }
}
