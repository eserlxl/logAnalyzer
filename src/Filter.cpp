#include "../include/Filter.h"
#include "../include/Utils.h"
#include <regex> // Added for std::regex, std::regex_match, std::regex_search, std::regex_constants
#include <set>   // Added for std::set


namespace { // Unnamed namespace for internal helper functions

// --- Evaluation helpers for FilterExpression ---

std::optional<std::string> getFieldValue(const LogEntry& entry, const FilterCondition& cond) {
    switch (cond.field) {
        case LogEntryField::TIMESTAMP:
            if (entry.timestamp.has_value()) {
                return Utils::formatTimestamp(entry.timestamp.value());
            }
            return std::nullopt;
        case LogEntryField::LEVEL: return Utils::logLevelToString(entry.level);
        case LogEntryField::SOURCE_FILE: return entry.sourceFile;
        case LogEntryField::MESSAGE: return entry.message;
        case LogEntryField::THREAD_ID: {
             auto it = entry.customFields.find("thread_id");
                if (it != entry.customFields.end()) {
                    return it->second;
                }
            return std::nullopt;
        }
        case LogEntryField::CUSTOM:
            if (cond.customField) {
                auto it = entry.customFields.find(*cond.customField);
                if (it != entry.customFields.end()) {
                    return it->second;
                }
            }
            return std::nullopt;
        default:
            return std::nullopt;
    }
}

bool evaluateCondition(const FilterCondition& cond, const LogEntry& entry) {
    auto fieldValueOpt = getFieldValue(entry, cond);
    if (!fieldValueOpt) {
        return false; // Field doesn't exist or is not applicable
    }
    const std::string& fieldValue = *fieldValueOpt;
    const std::string& condValue = cond.value;

    if (cond.valueType == FilterValueType::NUMERIC) {
        try {
            double fieldNum = std::stod(fieldValue);
            double condNum = std::stod(condValue);
            switch (cond.op) {
                case FilterOperator::EQUALS: return std::abs(fieldNum - condNum) < 1e-9;
                case FilterOperator::NOT_EQUALS: return std::abs(fieldNum - condNum) >= 1e-9;
                case FilterOperator::GREATER_THAN: return fieldNum > condNum;
                case FilterOperator::LESS_THAN: return fieldNum < condNum;
                case FilterOperator::GREATER_THAN_OR_EQUAL: return fieldNum >= condNum;
                case FilterOperator::LESS_THAN_OR_EQUAL: return fieldNum <= condNum;
                default: return false;
            }
        } catch (const std::exception&) {
            return false; // Conversion to double failed
        }
    }

    // Default to STRING comparison
    switch (cond.op) {
        case FilterOperator::EQUALS:
            return cond.caseSensitive ? (fieldValue == condValue) : Utils::caseInsensitiveEquals(fieldValue, condValue);
        case FilterOperator::NOT_EQUALS:
            return cond.caseSensitive ? (fieldValue != condValue) : !Utils::caseInsensitiveEquals(fieldValue, condValue);
        case FilterOperator::CONTAINS:
             if (cond.caseSensitive) {
                return fieldValue.find(condValue) != std::string::npos;
            } else {
                return Utils::caseInsensitiveSearch(fieldValue, condValue);
            }
        case FilterOperator::NOT_CONTAINS:
            if (cond.caseSensitive) {
                return fieldValue.find(condValue) == std::string::npos;
            } else {
                return !Utils::caseInsensitiveSearch(fieldValue, condValue);
            }
        case FilterOperator::STARTS_WITH:
             if (cond.caseSensitive) {
                return fieldValue.starts_with(condValue);
            } else {
                std::string lowerFieldValue = Utils::toLower(fieldValue);
                std::string lowerCondValue = Utils::toLower(condValue);
                return lowerFieldValue.starts_with(lowerCondValue);
            }
        case FilterOperator::ENDS_WITH:
            if (cond.caseSensitive) {
                return fieldValue.ends_with(condValue);
            } else {
                std::string lowerFieldValue = Utils::toLower(fieldValue);
                std::string lowerCondValue = Utils::toLower(condValue);
                return lowerFieldValue.ends_with(lowerCondValue);
            }
        case FilterOperator::REGEX_MATCH:
            try {
                auto flags = cond.caseSensitive ? std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase;
                std::regex re(condValue, flags);
                return std::regex_search(fieldValue, re);
            } catch (const std::regex_error&) {
                return false;
            }
        default:
            return false;
    }
}
    /**
     * @brief Retrieves a "nested" value from a LogEntry's custom fields.
     *
     * This function interprets "nested" access as looking up keys directly in
     * `LogEntry::customFields` that contain dots (e.g., "user.id"), rather than
     * traversing a hierarchical structure (like JSON). For example, if a fieldPath
     * is "user.id", it will look for a key "user.id" in customFields.
     *
     * @param entry The LogEntry to extract the value from.
     * @param fieldPath The dot-separated field path (e.g., "user.name", "request.id").
     * @return An optional string containing the value if found, std::nullopt otherwise.
     */
    std::optional<std::string> getNestedValue(const LogEntry& entry, const std::string& fieldPath) {
        // Since LogEntry::customFields is a map<string, string>,
        // "nested" access means searching for a key that matches the full path.
        // E.g., for "user.id", it looks for a key "user.id".
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
                // Attempt to convert string to double
                return std::stod(*strValueOpt);
            } catch (const std::invalid_argument& e) {
                // Not a valid number
            } catch (const std::out_of_range& e) {
                // Number out of range
            }
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
} // Unnamed namespace

bool FilterExpression::evaluate(const LogEntry& entry) const {
    switch (type_) {
        case ExpressionType::EMPTY:
            return true; // An empty filter matches everything
        case ExpressionType::CONDITION:
            return evaluateCondition(*condition_, entry);
        case ExpressionType::LOGICAL:
            switch (*logicalOperator_) {
                case FilterLogicalOperator::AND:
                    for (const auto& expr : expressions_) {
                        if (!expr.evaluate(entry)) {
                            return false; // Short-circuit
                        }
                    }
                    return true;
                case FilterLogicalOperator::OR:
                    for (const auto& expr : expressions_) {
                        if (expr.evaluate(entry)) {
                            return true; // Short-circuit
                        }
                    }
                    return false;
                case FilterLogicalOperator::NOT:
                    // 'NOT' should always have exactly one sub-expression
                    if (!expressions_.empty()) {
                        return !expressions_[0].evaluate(entry);
                    }
                    return true; // NOT applied to nothing is arguably true.
                default:
                    return false;
            }
        default:
            return false;
    }
}

NumericComparisonFilter::NumericComparisonFilter(std::string fieldKey, double value, Operator op)
    : fieldKey_(std::move(fieldKey)), value_(value), op_(op) {}

bool NumericComparisonFilter::matches(const LogEntry &entry) const {
    auto it = entry.customFields.find(fieldKey_);
    if (it == entry.customFields.end()) {
        return false; // Field not found
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
    } catch (const std::invalid_argument& e) {
        // Value is not a valid number, so it cannot match
    } catch (const std::out_of_range& e) {
        // Value is out of range for double, so it cannot match
    }
    return false;
}

BoolFilter::BoolFilter(std::string fieldKey, bool value)
    : fieldKey_(std::move(fieldKey)), value_(value) {}

bool BoolFilter::matches(const LogEntry &entry) const {
    auto it = entry.customFields.find(fieldKey_);
    if (it == entry.customFields.end()) {
        return false; // Field not found
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
        return false; // Nested field not found
    }

    const std::string& actualValue = *actualValueOpt;

    if (type_ == PatternType::Literal) {
        if (caseSensitive_) {
            return actualValue == valuePattern_;
        } else { // Handle case-insensitive literal match
            return Utils::caseInsensitiveEquals(actualValue, valuePattern_);
        }
    } else if (type_ == PatternType::Wildcard) {
        if (regexPattern_.has_value()) {
            return std::regex_search(actualValue, *regexPattern_);
        }
        return false; // Should not happen
    } else { // PatternType::Regex
        if (regexPattern_.has_value()) {
            return std::regex_search(actualValue, *regexPattern_);
        }
        return false; // Should not happen
    }
}

NestedNumericComparisonFilter::NestedNumericComparisonFilter(std::string fieldPath,
                                                           double value,
                                                           NumericComparisonFilter::Operator op)
    : fieldPath_(std::move(fieldPath)), value_(value), op_(op) {}

bool NestedNumericComparisonFilter::matches(const LogEntry &entry) const {
    auto actualValueOpt = getNestedNumericValue(entry, fieldPath_);
    if (!actualValueOpt) {
        return false; // Nested field not found or not a valid number
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
    return false; // Should be unreachable
}

NestedBoolFilter::NestedBoolFilter(std::string fieldPath, bool value)
    : fieldPath_(std::move(fieldPath)), value_(value) {}

bool NestedBoolFilter::matches(const LogEntry &entry) const {
    auto actualValueOpt = getNestedBoolValue(entry, fieldPath_);
    if (!actualValueOpt) {
        return false; // Nested field not found or not a valid boolean string
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
        return false; // Field not found
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
        return false; // Nested field not found
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
    } else if (type_ == PatternType::Wildcard) {
        if (regexPattern_.has_value()) {
            return std::regex_search(entry.sourceFile, *regexPattern_);
        }
        return false; // Should not happen if constructed correctly
    } else { // PatternType::Regex
        if (regexPattern_.has_value()) {
            return std::regex_search(entry.sourceFile, *regexPattern_);
        }
        return false; // Should not happen if constructed correctly
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
        return false; // Field not found
    }

    const std::string& actualValue = it->second;

    if (type_ == PatternType::Literal) {
        if (caseSensitive_) {
            return actualValue == valuePattern_;
        } else {
            return Utils::caseInsensitiveEquals(actualValue, valuePattern_);
        }
    } else if (type_ == PatternType::Wildcard || type_ == PatternType::Regex) {
        if (regexPattern_.has_value()) {
            return std::regex_search(actualValue, *regexPattern_);
        }
        // This case should ideally not be reached if the filter was constructed correctly,
        // but acts as a safeguard.
        return false;
    }
    // Fallback for any unhandled PatternType, though all expected types are covered above.
    return false;
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
    } else { // Logic::ALL
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
        // Use 'new' to call the private constructor, then wrap in shared_ptr
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
    // 'since' creates a range from the relative time up to now.
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
    /**
     * @brief Checks if a log entry matches the composite filter's conditions.
     *
     * If the filter list is empty:
     * - For Logic::AND, it returns true (no conditions means no constraints).
     * - For Logic::OR, it returns false (no condition can be met).
     *
     * @param entry The log entry to check.
     * @return True if the entry matches the composite filter, false otherwise.
     */
    if (filters_.empty()) {
        return logic_ == Logic::AND;
    }

    if (logic_ == Logic::AND) {
        for (const auto &filter : filters_) {
            if (!filter->matches(entry)) return false;
        }
        return true;
    } else { // OR
        for (const auto &filter : filters_) {
            if (filter->matches(entry)) return true;
        }
        return false;
    }
}
