#ifndef FILTER_H
#define FILTER_H

#include "LogTypes.h"
#include <vector>
#include <string>
#include <set>
#include <functional>
#include <memory>
#include <regex>
#include <algorithm>
#include <chrono>
#include <expected>
#include <optional> // For std::optional
#include "Utils.h" // For time parsing utilities

// Defines criteria for filtering log entries.
struct FilterCriteria {
    std::set<LogLevel> levels;
    std::optional<std::chrono::system_clock::time_point> startTime;
    std::optional<std::chrono::system_clock::time_point> endTime;
    std::string keyword;
    bool keywordCaseSensitive = false;
    std::string regexPattern;
};

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

class IFilter {
public:
    virtual ~IFilter() = default;
    virtual bool matches(const LogEntry &entry) const = 0;
};

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

#include <functional> // For std::function

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

#include <set> // For std::set

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

#include <expected>

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
