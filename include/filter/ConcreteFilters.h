#ifndef CONCRETE_FILTERS_H
#define CONCRETE_FILTERS_H

#include "filter/IFilter.h"
#include "core/LogTypes.h" // For LogLevel, PatternType etc.
#include "core/Error.h"    // For ErrorCode::Result
#include <string>
#include <vector>
#include <set>
#include <memory>
#include <functional>
#include <chrono>
#include <expected>
#include <optional>
#include <regex>

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

#endif // CONCRETE_FILTERS_H
