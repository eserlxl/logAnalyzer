#ifndef FILTER_H
#define FILTER_H

#include <vector> // Required for std::vector
#include <string>
#include <set> // Required for std::set
#include <functional>
#include <memory>
#include <regex> // Required for std::regex
#include <chrono>
#include <expected>
#include <optional> // For std::optional
#include <nlohmann/json.hpp> // Include for nlohmann/json types
#include "Error.h"
#include "LogTypes.h" // For LogEntryField, LogLevel and FieldMapping
#include "Utils.h" // For string conversions and time parsing utilities



// Enum for sorting criteria.
enum class SortBy {
    TIMESTAMP,
    LEVEL,
    MESSAGE
};

// Enum for sorting order.
enum class SortOrder {
    ASCENDING,
    DESCENDING
};

enum class FilterOperator {
    EQUALS,             // ==
    NOT_EQUALS,         // !=
    CONTAINS,           // substring search
    NOT_CONTAINS,       // not substring search
    STARTS_WITH,        // prefix search
    ENDS_WITH,          // suffix search
    REGEX_MATCH,        // regex match
    LESS_THAN,          // < (numeric/datetime)
    GREATER_THAN,       // > (numeric/datetime)
    LESS_THAN_OR_EQUAL, // <= (numeric/datetime)
    GREATER_THAN_OR_EQUAL, // >= (numeric/datetime)
    UNKNOWN
};

// New: Enum for logical operators to combine filter expressions
enum class FilterLogicalOperator {
    AND,
    OR,
    NOT, // Unary operator, applied to the next expression
    UNKNOWN
};

// New: Enum to indicate how a filter value should be interpreted
enum class FilterValueType {
    STRING,
    NUMERIC,
    DATETIME,
    UNKNOWN // Added UNKNOWN enumerator
};

// New: Represents a single filtering condition (leaf node in the filter tree)
struct FilterCondition {
    LogEntryField field = LogEntryField::UNKNOWN;        // The LogEntry field to apply the filter to.
    FilterOperator op = FilterOperator::UNKNOWN;          // The comparison operator.
    
    // The value to compare against. Always stored as a string.
    // For NUMERIC and DATETIME valueTypes, this string representation will be parsed
    // into the respective type at the point of filter evaluation, not at construction.
    std::string value;
    
    FilterValueType valueType = FilterValueType::STRING; // How to interpret 'value'.
    bool caseSensitive = false; // Whether comparison is case-sensitive for string operations.

    // For DATETIME valueType, this provides the format for parsing 'value'.
    std::optional<std::string> datetimeFormat;

    // Default constructor
    FilterCondition() = default;

    // Constructor for string-based conditions
    FilterCondition(LogEntryField f, FilterOperator o, std::string v, bool cs = false)
        : field(f), op(o), value(std::move(v)), valueType(FilterValueType::STRING), caseSensitive(cs) {}

    // Constructor for numeric conditions
    FilterCondition(LogEntryField f, FilterOperator o, std::string v, FilterValueType vt)
        : field(f), op(o), value(std::move(v)), valueType(vt) {
        if (vt == FilterValueType::DATETIME) {
            throw std::invalid_argument("FilterCondition: DATETIME valueType requires a datetimeFormat.");
        }
    }

    // Constructor for datetime conditions with format
    FilterCondition(LogEntryField f, FilterOperator o, std::string v, const std::string& dtFormat)
        : field(f), op(o), value(std::move(v)), valueType(FilterValueType::DATETIME), datetimeFormat(dtFormat) {}
};

// Transitional: Represents a simplified filter rule for legacy JSON formats.
// This is part of a transitional phase and should not be used for new development.
// It will be fully replaced by FilterCondition and the FilterExpression system.
// Its role is to provide a temporary bridge for backward compatibility during migration.
struct FilterRule {
    LogEntryField field = LogEntryField::UNKNOWN;
    FilterOperator op = FilterOperator::UNKNOWN;
    std::string value;
    bool caseSensitive = false;
};

// Legacy: A wrapper for older filter settings.
// This struct exists for backward compatibility and is being phased out.
// All new filtering logic must use the FilterExpression system. This struct and its
// related logic will be removed once the migration to FilterExpression is complete.
struct FilterCriteria {
    std::vector<LogLevel> levels;
    std::string keyword;
    bool keywordCaseSensitive = false;
    std::string regexPattern;
    std::optional<std::chrono::system_clock::time_point> startTime;
    std::optional<std::chrono::system_clock::time_point> endTime;

    // The 'rules' member is part of the legacy system. It is commented out
    // as its functionality is being replaced by FilterExpression. It was intended
    // for a future iteration (e.g., Iteration 5) but that plan has been superseded
    // by the more robust FilterExpression architecture.
    // std::vector<FilterRule> rules;
    // std::optional<FilterExpression> expression; // Future integration
};



// JSON conversion for FilterRule
inline void to_json(nlohmann::json& j, const FilterRule& fr) {
    j = nlohmann::json{
        {"field", Utils::logEntryFieldToString(fr.field)},
        {"op", Utils::filterOperatorToString(fr.op)},
        {"value", fr.value},
        {"caseSensitive", fr.caseSensitive}
    };
}

inline ErrorCode::Result<void> from_json(const nlohmann::json& j, FilterRule& fr) {
    if (!j.contains("field") || !j.at("field").is_string()) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterRule is missing or has invalid 'field'."));
    }
    fr.field = Utils::stringToLogEntryField(j.at("field").get<std::string>());
    if (fr.field == LogEntryField::UNKNOWN) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterRule has an unrecognized field: " + j.at("field").get<std::string>()));
    }

    if (!j.contains("op") || !j.at("op").is_string()) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterRule is missing or has invalid 'op'."));
    }
    fr.op = Utils::stringToFilterOperator(j.at("op").get<std::string>());
    if (fr.op == FilterOperator::UNKNOWN) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterRule has an unrecognized 'op' string: " + j.at("op").get<std::string>()));
    }

    if (!j.contains("value") || !j.at("value").is_string()) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterRule is missing or has invalid 'value'."));
    }
    fr.value = j.at("value").get<std::string>();

    fr.caseSensitive = j.value("caseSensitive", false);

    return {}; // Success
}

// Helper to convert FilterCondition to JSON
inline void to_json(nlohmann::json& j, const FilterCondition& fc) {
    j = nlohmann::json{
        {"field", Utils::logEntryFieldToString(fc.field)},
        {"op", Utils::filterOperatorToString(fc.op)},
        {"value", fc.value},
        {"value_type", Utils::filterValueTypeToString(fc.valueType)},
        {"caseSensitive", fc.caseSensitive}
    };
    if (fc.datetimeFormat) {
        j["datetimeFormat"] = *fc.datetimeFormat;
    }
}

// Helper to convert JSON to FilterCondition
inline ErrorCode::Result<void> from_json(const nlohmann::json& j, FilterCondition& fc) {
    if (!j.contains("field") || !j.at("field").is_string()) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterCondition is missing or has invalid 'field'."));
    }
    fc.field = Utils::stringToLogEntryField(j.at("field").get<std::string>());
    if (fc.field == LogEntryField::UNKNOWN) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterCondition has an unrecognized 'field' string: " + j.at("field").get<std::string>()));
    }

    if (!j.contains("op") || !j.at("op").is_string()) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterCondition is missing or has invalid 'op'."));
    }
    fc.op = Utils::stringToFilterOperator(j.at("op").get<std::string>());
    if (fc.op == FilterOperator::UNKNOWN) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterCondition has an unrecognized 'op' string: " + j.at("op").get<std::string>()));
    }

    if (!j.contains("value") || !j.at("value").is_string()) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterCondition is missing or has invalid 'value'."));
    }
    fc.value = j.at("value").get<std::string>();

    if (!j.contains("value_type")) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterCondition is missing 'value_type'."));
    }

    if (j.at("value_type").is_string()) {
        fc.valueType = Utils::stringToFilterValueType(j.at("value_type").get<std::string>());
        if (fc.valueType == FilterValueType::UNKNOWN) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterCondition has an unrecognized 'value_type' string: " + j.at("value_type").get<std::string>()));
        }
    } else if (j.at("value_type").is_number_integer()) {
        int vt_int = j.at("value_type").get<int>();
        if (vt_int >= static_cast<int>(FilterValueType::STRING) && vt_int <= static_cast<int>(FilterValueType::DATETIME)) {
            fc.valueType = static_cast<FilterValueType>(vt_int);
        } else {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterCondition has an invalid integer for 'value_type'. Must be between 0 and 2."));
        }
    } else {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterCondition 'value_type' must be a string or integer."));
    }

    fc.caseSensitive = j.value("caseSensitive", false);

    if (j.contains("datetimeFormat")) {
        if (j.at("datetimeFormat").is_string()) {
            fc.datetimeFormat = j.at("datetimeFormat").get<std::string>();
        } else if (!j.at("datetimeFormat").is_null()) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterCondition 'datetimeFormat' must be a string or null."));
        }
    }

    // Final validation: Enforce that DATETIME valueType requires a datetimeFormat.
    if (fc.valueType == FilterValueType::DATETIME && !fc.datetimeFormat.has_value()) {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterCondition with DATETIME valueType requires a 'datetimeFormat'."));
    }

    return {}; // Success
}

// New: Represents a composite filter expression (tree-like structure)
class FilterExpression {
public:
    // Default constructor (represents an empty/no-op filter)
    FilterExpression() : type_(ExpressionType::EMPTY) {}

    // Constructor for a single condition (leaf node)
    FilterExpression(FilterCondition condition) : type_(ExpressionType::CONDITION), condition_(std::move(condition)) {}

    // Constructor for logical operations
    FilterExpression(FilterLogicalOperator op, std::vector<FilterExpression> expressions = {})
        : type_(ExpressionType::LOGICAL), logicalOperator_(op), expressions_(std::move(expressions)) {}

    // Fluent builders for complex expressions
    static FilterExpression create(FilterCondition condition) {
        return FilterExpression(std::move(condition));
    }

    FilterExpression And(FilterExpression other) const {
        if (type_ == ExpressionType::LOGICAL && logicalOperator_ == FilterLogicalOperator::AND) {
            FilterExpression newExpr = *this;
            newExpr.expressions_.push_back(std::move(other));
            return newExpr;
        }
        return FilterExpression(FilterLogicalOperator::AND, {*this, std::move(other)});
    }

    FilterExpression Or(FilterExpression other) const {
        if (type_ == ExpressionType::LOGICAL && logicalOperator_ == FilterLogicalOperator::OR) {
            FilterExpression newExpr = *this;
            newExpr.expressions_.push_back(std::move(other));
            return newExpr;
        }
        return FilterExpression(FilterLogicalOperator::OR, {*this, std::move(other)});
    }

    // Applies 'NOT' to 'this' expression
    FilterExpression Not() const {
        return FilterExpression(FilterLogicalOperator::NOT, {*this});
    }

    // Forward declarations for recursive JSON conversion
    friend void to_json(nlohmann::json& j, const FilterExpression& fe);
    friend inline ErrorCode::Result<void> from_json(const nlohmann::json& j, FilterExpression& fe);

    // Accessors for internal components (useful for evaluation)
    enum class ExpressionType { EMPTY, CONDITION, LOGICAL };
    ExpressionType getType() const { return type_; }
    const std::optional<FilterCondition>& getCondition() const { return condition_; }
    const std::optional<FilterLogicalOperator>& getLogicalOperator() const { return logicalOperator_; }
    const std::vector<FilterExpression>& getExpressions() const { return expressions_; }

    bool isCondition() const { return type_ == ExpressionType::CONDITION; }
    bool isLogical() const { return type_ == ExpressionType::LOGICAL; }

private:
    ExpressionType type_;
    std::optional<FilterCondition> condition_;
    std::optional<FilterLogicalOperator> logicalOperator_;
    std::vector<FilterExpression> expressions_;
};



// JSON conversion for FilterExpression (recursive)
// JSON conversion for FilterExpression (recursive)
inline void to_json(nlohmann::json& j, const FilterExpression& fe) {
    if (fe.getType() == FilterExpression::ExpressionType::CONDITION) {
        j["condition"] = *fe.getCondition();
    } else if (fe.getType() == FilterExpression::ExpressionType::LOGICAL) {
        j["operator"] = Utils::filterLogicalOperatorToString(*fe.getLogicalOperator());
        if (!fe.getExpressions().empty()) {
                    j["operands"] = nlohmann::json::array();
                    for (const auto& operand : fe.getExpressions()) {                nlohmann::json operand_j;
                to_json(operand_j, operand); // Recursive call
                j["operands"].push_back(operand_j);
            }
        }
    }
}

inline ErrorCode::Result<void> from_json(const nlohmann::json& j, FilterExpression& fe) {
    if (j.contains("condition") && j.at("condition").is_object()) {
        FilterCondition fc;
        ErrorCode::Result<void> result = from_json(j.at("condition"), fc);
        if (!result.has_value()) {
            return std::unexpected(result.error());
        }
        fe = FilterExpression(std::move(fc));
    } else if (j.contains("operator") && j.at("operator").is_string()) {
        FilterLogicalOperator op = Utils::stringToFilterLogicalOperator(j.at("operator").get<std::string>());
        if (op == FilterLogicalOperator::UNKNOWN) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Unknown filter logical operator: " + j.at("operator").get<std::string>()));
        } else {
            std::vector<FilterExpression> operands;
            if (j.contains("operands") && j.at("operands").is_array()) {
                for (const auto& operand_j : j.at("operands")) {
                    FilterExpression operand_fe;
                    ErrorCode::Result<void> result = from_json(operand_j, operand_fe); // Recursive call
                    if (!result.has_value()) {
                        return std::unexpected(result.error());
                    }
                    operands.push_back(std::move(operand_fe));
                }
            } else {
                return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterExpression with operator must contain 'operands' array."));
            }
            fe = FilterExpression(op, std::move(operands));
        }
    } else {
        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "FilterExpression must contain either 'condition' or 'operator' with 'operands'."));
    }
    
    return {}; // Success
}

class IFilter {
public:
    virtual ~IFilter() = default;
    virtual bool matches(const LogEntry &entry) const = 0;
};

// All existing concrete IFilter implementations will be deprecated or refactored
// to use the new FilterExpression system internally.
// For now, I will leave them as is, but they will eventually be replaced
// by a single LogAnalyzerFilter component that evaluates a FilterExpression.

// Deprecate FilterRule
// struct FilterRule { ... }; // Removed as per design

// Existing concrete IFilter implementations (will be refactored/replaced by FilterExpression evaluation)
// Note on pattern matching consistency:
// For PatternType::Wildcard (after glob-to-regex conversion) and PatternType::Regex,
// std::regex_search is used, meaning patterns can match any substring of the input.
// This provides consistent substring matching behavior across these pattern types.
class SourceFileFilter : public IFilter {
public:
    /**
     * @brief Constructs a SourceFileFilter.
     * @param pattern The pattern to match against the LogEntry's source file.
     * @param type The type of pattern (Literal, Wildcard, Regex).
     *             For Wildcard and Regex, uses std::regex_search for substring matching.
     * @param caseSensitive Whether the comparison should be case-sensitive.
     */
    explicit SourceFileFilter(std::string pattern,
                              PatternType type = PatternType::Literal,
                              bool caseSensitive = false);
    /**
     * @brief Checks if the log entry's source file matches the configured pattern.
     * @param entry The log entry to check.
     * @return True if the source file matches, false otherwise.
     */
    bool matches(const LogEntry &entry) const override;
private:
    std::string pattern_;
    PatternType type_;
    bool caseSensitive_;
    std::optional<std::regex> regexPattern_;
};

class FieldExistsFilter : public IFilter {
public:
    explicit FieldExistsFilter(std::string fieldKey);
    bool matches(const LogEntry &entry) const override;
private:
    std::string fieldKey_;
};

class FieldValueFilter : public IFilter {
public:
    /**
     * @brief Constructs a FieldValueFilter.
     * @param fieldKey The key of the custom field to filter on.
     * @param valuePattern The pattern to match against the field's value.
     * @param type The type of pattern (Literal, Wildcard, Regex).
     *             For Wildcard and Regex, uses std::regex_search for substring matching.
     * @param caseSensitive Whether the comparison should be case-sensitive.
     */
    explicit FieldValueFilter(std::string fieldKey,
                              std::string valuePattern,
                              PatternType type = PatternType::Literal,
                              bool caseSensitive = false);
    /**
     * @brief Checks if the custom field's value in the log entry matches the configured pattern.
     * @param entry The log entry to check.
     * @return True if the field exists and its value matches, false otherwise.
     */
    bool matches(const LogEntry &entry) const override;
private:
    std::string fieldKey_;
    std::string valuePattern_;
    PatternType type_;
    bool caseSensitive_;
    std::optional<std::regex> regexPattern_;
};

class PredicateFilter : public IFilter {
public:
    using Predicate = std::function<bool(const LogEntry&)>;
    explicit PredicateFilter(Predicate predicate);
    bool matches(const LogEntry &entry) const override;
private:
    Predicate predicate_;
};

class LogLevelSetFilter : public IFilter {
public:
    explicit LogLevelSetFilter(std::set<LogLevel> allowedLevels);
    bool matches(const LogEntry &entry) const override;
private:
    std::set<LogLevel> allowedLevels_;
};

class LevelFilter : public IFilter {
public:
    explicit LevelFilter(LogLevel level) : targetLevel_(level) {}
    bool matches(const LogEntry &entry) const override {
        return entry.level == targetLevel_;
    }
private:
    LogLevel targetLevel_;
};

class MinLevelFilter : public IFilter {
public:
    explicit MinLevelFilter(LogLevel level) : minLevel_(level) {}
    bool matches(const LogEntry &entry) const override {
        return entry.level >= minLevel_;
    }
private:
    LogLevel minLevel_;
};

class KeywordFilter : public IFilter {
public:
    enum class Logic { ANY, ALL };

    explicit KeywordFilter(std::string keyword, bool isCaseSensitive = false);
    explicit KeywordFilter(std::vector<std::string> keywords,
                           Logic logic = Logic::ANY,
                           bool isCaseSensitive = false);
    bool matches(const LogEntry &entry) const override;
private:
    std::vector<std::string> keywords_;
    Logic logic_;
    bool isCaseSensitive_;
};

class RegexFilter : public IFilter {
public:
    static ErrorCode::Result<std::shared_ptr<RegexFilter>> create(std::string pattern, bool caseSensitive = false);
    bool matches(const LogEntry &entry) const override;

private:
    explicit RegexFilter(std::string pattern, std::regex_constants::syntax_option_type flags);
    explicit RegexFilter(std::string pattern, bool caseSensitive);
    std::regex pattern_;
};

class TimeRangeFilter : public IFilter {
public:
    TimeRangeFilter(std::chrono::system_clock::time_point start,
                    std::chrono::system_clock::time_point end);

    static std::expected<TimeRangeFilter, std::string> fromStrings(const std::string& start, const std::string& end);
    static std::expected<TimeRangeFilter, std::string> forDay(const std::string& dateString);
    static std::expected<TimeRangeFilter, std::string> since(const std::string& relativeTime);

    bool matches(const LogEntry &entry) const override;
private:
    std::chrono::system_clock::time_point startTime_;
    std::chrono::system_clock::time_point endTime_;
};

class ExclusionFilter : public IFilter {
public:
    explicit ExclusionFilter(std::shared_ptr<IFilter> filter) : filter_(std::move(filter)) {}
    bool matches(const LogEntry &entry) const override {
        return !filter_->matches(entry);
    }
private:
    std::shared_ptr<IFilter> filter_;
};

class CompositeFilter : public IFilter {
public:
    enum class Logic { AND, OR };
    explicit CompositeFilter(Logic logic = Logic::AND) : logic_(logic) {}
    void add(std::shared_ptr<IFilter> filter) {
        filters_.push_back(std::move(filter));
    }
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
    bool matches(const LogEntry &entry) const override;
private:
    Logic logic_;
    std::vector<std::shared_ptr<IFilter>> filters_;
};

class NumericComparisonFilter : public IFilter {
public:
    // Epsilon for floating-point comparisons. A common choice for relative comparisons.
    static constexpr double EPSILON = 1e-9; 

    enum class Operator {
        EQ,  // Equal to
        NEQ, // Not equal to
        GT,  // Greater than
        LT,  // Less than
        GTE, // Greater than or equal to
        LTE  // Less than or equal to
    };

    NumericComparisonFilter(std::string fieldKey, double value, Operator op);
    bool matches(const LogEntry &entry) const override;
private:
    std::string fieldKey_;
    double value_;
    Operator op_;
};

class BoolFilter : public IFilter {
public:
    explicit BoolFilter(std::string fieldKey, bool value);
    bool matches(const LogEntry &entry) const override;
private:
    std::string fieldKey_;
    bool value_;
};



class NestedFieldValueFilter : public IFilter {
public:
    /**
     * @brief Constructs a NestedFieldValueFilter.
     * @param fieldPath The dot-separated path to the nested field (e.g., "user.id").
     * @param valuePattern The pattern to match against the nested field's value.
     * @param type The type of pattern (Literal, Wildcard, Regex).
     *             For Wildcard and Regex, uses std::regex_search for substring matching.
     * @param caseSensitive Whether the comparison should be case-sensitive.
     */
    NestedFieldValueFilter(std::string fieldPath,
                           std::string valuePattern,
                           PatternType type = PatternType::Literal,
                           bool caseSensitive = false);
    /**
     * @brief Checks if the nested field's value in the log entry matches the configured pattern.
     * @param entry The log entry to check.
     * @return True if the nested field exists and its value matches, false otherwise.
     */
    bool matches(const LogEntry &entry) const override;
private:
    std::string fieldPath_;
    std::string valuePattern_;
    PatternType type_;
    bool caseSensitive_;
    std::optional<std::regex> regexPattern_;
};

class NestedNumericComparisonFilter : public IFilter {
public:
    // Operator enum as in NumericComparisonFilter
    explicit NestedNumericComparisonFilter(std::string fieldPath, double value, NumericComparisonFilter::Operator op);
    bool matches(const LogEntry &entry) const override;
private:
    std::string fieldPath_;
    double value_;
    NumericComparisonFilter::Operator op_;
};

class NestedBoolFilter : public IFilter {
public:
    explicit NestedBoolFilter(std::string fieldPath, bool value);
    bool matches(const LogEntry &entry) const override;
private:
    std::string fieldPath_;
    bool value_;
};

class ValueSetFilter : public IFilter {
public:
    ValueSetFilter(std::string fieldKey, std::set<std::string> values, bool caseSensitive = false);
    bool matches(const LogEntry &entry) const override;
private:
    std::string fieldKey_;
    std::set<std::string> valueSet_;
    bool caseSensitive_;
};

class NestedValueSetFilter : public IFilter {
public:
    NestedValueSetFilter(std::string fieldPath, std::set<std::string> values, bool caseSensitive = false);
    bool matches(const LogEntry &entry) const override;
private:
    std::string fieldPath_;
    std::set<std::string> valueSet_;
    bool caseSensitive_;
};

#endif // FILTER_H
