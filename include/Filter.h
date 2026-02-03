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
#include "Utils.h" // For time parsing utilities

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
    std::regex regexPattern_;
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
    std::regex regexPattern_;
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
    explicit RegexFilter(std::string pattern, std::regex_constants::syntax_option_type flags = std::regex::ECMAScript);
    explicit RegexFilter(std::string pattern, bool caseSensitive);
    bool matches(const LogEntry &entry) const override;
private:
    std::regex pattern_;
};

#include <expected>

class TimeRangeFilter : public IFilter {
public:
    TimeRangeFilter(std::chrono::system_clock::time_point start,
                    std::chrono::system_clock::time_point end);

    static std::expected<TimeRangeFilter, std::string> fromStrings(const std::string& start, const std::string& end);
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

#endif // FILTER_H
