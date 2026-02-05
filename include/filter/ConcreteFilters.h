#ifndef CONCRETE_FILTERS_H
#define CONCRETE_FILTERS_H

#include "filter/IFilter.h"
#include "core/LogTypes.h" // For LogLevel, PatternType etc.
#include "core/Error.h"    // For ErrorCode::Result
#include "utils/UtilsCore.h"    // For ci_less
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
/**
 * @brief Filters log entries based on their source file name.
 *
 * Supports literal, wildcard (glob-like), and regex patterns.
 *
 * For PatternType::Wildcard, glob patterns (e.g., "*.log", "server*") are converted
 * into regular expressions. The matching is then performed using `std::regex_search`,
 * which inherently looks for a substring match. For example, a wildcard pattern
 * "server*" will be converted to `server.*` regex, and `std::regex_search` will
 * match this regex if it appears anywhere in the source file string.
 * This effectively makes "server*" match "my-server.log", "server.log",
 * and "log-from-server.log" if the `server.*` regex is found as a substring.
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

// --- Nested Filters: Support dot-notation (currently flat map lookup) ---

class NestedFieldValueFilter : public FieldValueFilter {
public:
    NestedFieldValueFilter(std::string fieldPath,
                           std::string valuePattern,
                           PatternType type = PatternType::Literal,
                           bool caseSensitive = false);
};

class NestedNumericComparisonFilter : public NumericComparisonFilter {
public:
    explicit NestedNumericComparisonFilter(std::string fieldPath, double value, NumericComparisonFilter::Operator op);
};

class NestedBoolFilter : public BoolFilter {
public:
    explicit NestedBoolFilter(std::string fieldPath, bool value);
};

class NestedValueSetFilter : public ValueSetFilter {
public:
    NestedValueSetFilter(std::string fieldPath, std::set<std::string> values, bool caseSensitive = false);
};

#endif // CONCRETE_FILTERS_H
