#ifndef FILTER_H
#define FILTER_H

#include <vector> // Required for std::vector
#include <string>
#include <set> // Required for std::set
#include <functional>
#include <memory>
#include <regex> // Required for std::regex
#include <algorithm>
#include <chrono>
#include <expected>
#include <optional> // For std::optional
#include <nlohmann/json.hpp> // Include for nlohmann/json types
#include "LogTypes.h" // For LogEntryField, LogLevel and FieldMapping
#include "Utils.h" // For string conversions and time parsing utilities
#include "Utils.h" // For string conversions and time parsing utilities
#include "Utils.h" // For string conversions and time parsing utilities
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
    NOT // Unary operator, applied to the next expression
};

// New: Enum to indicate how a filter value should be interpreted
enum class FilterValueType {
    STRING,
    NUMERIC,
    DATETIME
};

// New: Represents a single filtering condition (leaf node in the filter tree)
struct FilterCondition {
    // We are using LogEntryField for now, assuming only built-in fields can be filtered
    // If custom fields need filtering, this would need to become a variant<LogEntryField, std::string>
    LogEntryField field = LogEntryField::UNKNOWN;        // The LogEntry field to apply the filter to
    FilterOperator op = FilterOperator::UNKNOWN;          // The comparison operator
    std::string value;          // The value to compare against (string representation)
    FilterValueType valueType = FilterValueType::STRING; // How to interpret 'value'
    bool caseSensitive = false; // Whether the comparison should be case-sensitive for string ops

    // For datetime fields, optional format string for parsing 'value'
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
            // This constructor doesn't take datetimeFormat, so it should be handled externally or via another overload
            // For now, it will be std::nullopt
        }
    }

    // Constructor for datetime conditions with format
    FilterCondition(LogEntryField f, FilterOperator o, std::string v, const std::string& dtFormat)
        : field(f), op(o), value(std::move(v)), valueType(FilterValueType::DATETIME), datetimeFormat(dtFormat) {}
};

// New: Represents a single filtering condition for JSON serialization (Iteration 5 specific)
struct FilterRule {
    LogEntryField field = LogEntryField::UNKNOWN;
    FilterOperator op = FilterOperator::UNKNOWN;
    std::string value;
    bool caseSensitive = false;
};

// Forward declarations to break circular dependency with Utils.h
namespace Utils {
    std::string logEntryFieldToString(LogEntryField field);
    LogEntryField stringToLogEntryField(const std::string& fieldStr);
    std::string filterOperatorToString(FilterOperator op);
    FilterOperator stringToFilterOperator(const std::string& opStr);
}

// JSON conversion for FilterRule
inline void to_json(nlohmann::json& j, const FilterRule& fr) {
    j = nlohmann::json{
        {"field", Utils::logEntryFieldToString(fr.field)},
        {"op", Utils::filterOperatorToString(fr.op)},
        {"value", fr.value},
        {"caseSensitive", fr.caseSensitive}
    };
}

inline void from_json(const nlohmann::json& j, FilterRule& fr) {
    std::vector<std::string> errors;

    if (j.contains("field") && j.at("field").is_string()) {
        fr.field = Utils::stringToLogEntryField(j.at("field").get<std::string>());
        if (fr.field == LogEntryField::UNKNOWN && j.at("field").get<std::string>() != "UNKNOWN") {
             errors.push_back("FilterRule has an unrecognized field: " + j.at("field").get<std::string>());
        }
    } else {
        errors.push_back("FilterRule is missing or has invalid 'field'.");
    }

    if (j.contains("op") && j.at("op").is_string()) {
        fr.op = Utils::stringToFilterOperator(j.at("op").get<std::string>());
    } else {
        errors.push_back("FilterRule is missing or has invalid 'op'.");
    }

    if (j.contains("value") && j.at("value").is_string()) {
        fr.value = j.at("value").get<std::string>();
    } else {
        errors.push_back("FilterRule is missing or has invalid 'value'.");
    }

    if (j.contains("caseSensitive") && j.at("caseSensitive").is_boolean()) {
        fr.caseSensitive = j.at("caseSensitive").get<bool>();
    } else {
        // Default to false if not specified or invalid
        fr.caseSensitive = false;
    }

    if (!errors.empty()) {
        throw nlohmann::json::exception(errors.size(), errors[0].c_str());
    }
}

// New: Represents a composite filter expression (tree-like structure)
class FilterExpression {
public:
    // Default constructor (represents an empty/no-op filter)
    FilterExpression() : type_(ExpressionType::EMPTY) {}

    // Constructor for a single condition (leaf node)
    FilterExpression(FilterCondition condition) : type_(ExpressionType::CONDITION), condition_(std::move(condition)) {}

    // Constructor for logical operations
    FilterExpression(FilterLogicalOperator op, std::vector<FilterExpression> operands = {})
        : type_(ExpressionType::LOGICAL), logicalOperator_(op), operands_(std::move(operands)) {}

    // Fluent builders for complex expressions
    static FilterExpression create(FilterCondition condition) {
        return FilterExpression(std::move(condition));
    }

    FilterExpression And(FilterExpression other) const {
        if (type_ == ExpressionType::LOGICAL && logicalOperator_ == FilterLogicalOperator::AND) {
            FilterExpression newExpr = *this;
            newExpr.operands_.push_back(std::move(other));
            return newExpr;
        }
        return FilterExpression(FilterLogicalOperator::AND, {*this, std::move(other)});
    }

    FilterExpression Or(FilterExpression other) const {
        if (type_ == ExpressionType::LOGICAL && logicalOperator_ == FilterLogicalOperator::OR) {
            FilterExpression newExpr = *this;
            newExpr.operands_.push_back(std::move(other));
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
    friend void from_json(const nlohmann::json& j, FilterExpression& fe);

    // Accessors for internal components (useful for evaluation)
    enum class ExpressionType { EMPTY, CONDITION, LOGICAL };
    ExpressionType getType() const { return type_; }
    const std::optional<FilterCondition>& getCondition() const { return condition_; }
    const std::optional<FilterLogicalOperator>& getLogicalOperator() const { return logicalOperator_; }
    const std::vector<FilterExpression>& getOperands() const { return operands_; }

private:
    ExpressionType type_;
    std::optional<FilterCondition> condition_;
    std::optional<FilterLogicalOperator> logicalOperator_;
    std::vector<FilterExpression> operands_;
};

// Forward declarations for recursive JSON conversion
inline void to_json(nlohmann::json& j, const FilterExpression& fe);
inline void from_json(const nlohmann::json& j, FilterExpression& fe);

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
class SourceFileFilter : public IFilter {
public:
    explicit SourceFileFilter(std::string pattern,
                              PatternType type = PatternType::Literal,
                              bool caseSensitive = false);
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
    explicit FieldValueFilter(std::string fieldKey,
                              std::string valuePattern,
                              PatternType type = PatternType::Literal,
                              bool caseSensitive = false);
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
    static std::expected<std::shared_ptr<RegexFilter>, std::string> create(std::string pattern, bool caseSensitive = false);
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
    bool matches(const LogEntry &entry) const override;
private:
    Logic logic_;
    std::vector<std::shared_ptr<IFilter>> filters_;
};

class NumericComparisonFilter : public IFilter {
public:
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

// Internal helper for nested field access (declaration only)
namespace Detail {
    std::optional<std::string> getNestedValue(const LogEntry& entry, const std::string& fieldPath);
    std::optional<double> getNestedNumericValue(const LogEntry& entry, const std::string& fieldPath);
    std::optional<bool> getNestedBoolValue(const LogEntry& entry, const std::string& fieldPath);
}

class NestedFieldValueFilter : public IFilter {
public:
    NestedFieldValueFilter(std::string fieldPath,
                           std::string valuePattern,
                           PatternType type = PatternType::Literal,
                           bool caseSensitive = false);
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

