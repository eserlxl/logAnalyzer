// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef CONCRETE_FILTERS_H
#define CONCRETE_FILTERS_H

#include "filter/IFilter.h"
#include "filter/Expression.h"
#include "core/LogTypes.h" // For LogLevel, PatternType etc.
#include "core/Error.h"    // For ErrorCode::Result
#include "utils/Core.h"    // For ci_less
#include <string>
#include <vector>
#include <set>
#include <memory>
#include <functional>
#include <chrono>
#include <expected>
#include <optional>
#include <regex>
#include <nlohmann/json_fwd.hpp>

/**
 * @brief An IFilter implementation that evaluates log entries against a FilterExpression tree.
 *
 * This class serves as a bridge between the modern, data-driven FilterExpression
 * system and legacy components that operate on the IFilter interface.
 */
class ExpressionFilter : public IFilter {
public:
    /**
     * @brief Constructs an ExpressionFilter from a FilterExpression.
     * @param expression The filter expression tree to evaluate. It is moved into the filter.
     */
    explicit ExpressionFilter(FilterExpression expression);

    /**
     * @brief Factory method to create an ExpressionFilter from a JSON object.
     *
     * This method deserializes a JSON object into a FilterExpression and validates it
     * before constructing the filter.
     *
     * @param json_spec The nlohmann::json object representing the filter expression.
     * @return A result containing a shared_ptr to the new ExpressionFilter or an error.
     */
    static ErrorCode::Result<std::shared_ptr<ExpressionFilter>> create(const nlohmann::json& json_spec);

    /**
     * @brief Evaluates the log entry against the stored filter expression.
     *
     * Logs an error to stderr and returns `true` (fail-open) if the expression
     * evaluation fails for an unexpected reason.
     *
     * @param entry The log entry to check.
     * @return True if the entry matches the expression, false otherwise.
     */
    bool matches(const LogEntry &entry) const override;

private:
    FilterExpression expression_;
};

// Note on pattern matching consistency:
// For PatternType::Wildcard (after glob-to-regex conversion) and PatternType::Regex,
// std::regex_search is used, meaning patterns can match any substring of the input.
// This provides consistent substring matching behavior across these pattern types.
/**
 * @brief Filters log entries based on their source file name.
 *
 * Supports literal, wildcard (glob-like), and regex patterns.
 *
 * For PatternType::Wildcard, glob patterns (e.g., "*.log", "server*") are converted
 * into regular expressions. The conversion adds start (^) and end ($) anchors, meaning
 * the pattern must match the *entire* source file name.
 * For example, "server*" becomes `^server.*$`, which matches "server.log" but NOT
 * "log-from-server.log". To match substrings with wildcards, use "*server*".
 * The matching is performed using `std::regex_search` on the anchored regex.
 *
 * For PatternType::Regex, the provided pattern is used directly with `std::regex_search`.
 *
 * Case sensitivity can be configured.
 */
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
protected:
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
protected:
    std::string fieldKey_;
    double value_;
    Operator op_;
};

class BoolFilter : public IFilter {
public:
    explicit BoolFilter(std::string fieldKey, bool value, bool caseSensitive = false);
    bool matches(const LogEntry &entry) const override;
protected:
    std::string fieldKey_;
    bool value_;
    bool caseSensitive_;
};

class ValueSetFilter : public IFilter {
public:
    ValueSetFilter(std::string fieldKey, std::set<std::string> values, bool caseSensitive = false);
    bool matches(const LogEntry &entry) const override;
protected:
    std::string fieldKey_;
    std::set<std::string, LogAnalyzerInternal::ci_less> valueSetInsensitive_;
    std::set<std::string> valueSetSensitive_;
    bool caseSensitive_;
};

// --- Dotted-Key Filters: Support dot-notation for flat map key lookup ---

/**
 * @brief A filter that matches a field value from the `customFields` map using a dotted key.
 *
 * This filter does not perform a true nested lookup into a JSON object. Instead, it treats the
 * `fieldPath` as a single key to be looked up in the `customFields` map. For example, a `fieldPath`
 * of `"a.b.c"` will match the key `"a.b.c"` in the map, not a nested field `c` inside `b` inside `a`.
 */
class DottedKeyFieldValueFilter : public FieldValueFilter {
public:
    DottedKeyFieldValueFilter(std::string fieldPath,
                              std::string valuePattern,
                              PatternType type = PatternType::Literal,
                              bool caseSensitive = false);
};

/**
 * @brief A filter that performs a numeric comparison on a field from the `customFields` map using a dotted key.
 *
 * This filter does not perform a true nested lookup. It treats `fieldPath` as a single key.
 */
class DottedKeyNumericComparisonFilter : public NumericComparisonFilter {
public:
    DottedKeyNumericComparisonFilter(std::string fieldPath, double value, NumericComparisonFilter::Operator op);
};

/**
 * @brief A filter that performs a boolean check on a field from the `customFields` map using a dotted key.
 *
 * This filter does not perform a true nested lookup. It treats `fieldPath` as a single key.
 */
class DottedKeyBoolFilter : public BoolFilter {
public:
    explicit DottedKeyBoolFilter(std::string fieldPath, bool value);
};

/**
 * @brief A filter that checks if a field's value is in a set, using a dotted key from the `customFields` map.
 *
 * This filter does not perform a true nested lookup. It treats `fieldPath` as a single key.
 */
class DottedKeyValueSetFilter : public ValueSetFilter {
public:
    DottedKeyValueSetFilter(std::string fieldPath, std::set<std::string> values, bool caseSensitive = false);
};

#endif // CONCRETE_FILTERS_H
