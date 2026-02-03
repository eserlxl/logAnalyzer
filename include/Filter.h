#ifndef FILTER_H
#define FILTER_H

#include "LogTypes.h"
#include <vector>
#include <string>
#include <memory>
#include <regex>
#include <algorithm>
#include <chrono>

class IFilter {
public:
    virtual ~IFilter() = default;
    virtual bool matches(const LogEntry &entry) const = 0;
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
    explicit KeywordFilter(std::string keyword, bool caseSensitive = false)
        : keyword_(std::move(keyword)), caseSensitive_(caseSensitive) {}
    bool matches(const LogEntry &entry) const override;
private:
    std::string keyword_;
    bool caseSensitive_;
};

class RegexFilter : public IFilter {
public:
    explicit RegexFilter(std::string pattern, std::regex_constants::syntax_option_type flags = std::regex::ECMAScript);
    bool matches(const LogEntry &entry) const override;
private:
    std::regex pattern_;
};

class TimeRangeFilter : public IFilter {
public:
    TimeRangeFilter(std::chrono::system_clock::time_point start,
                    std::chrono::system_clock::time_point end)
        : startTime_(start), endTime_(end) {}
    bool matches(const LogEntry &entry) const override {
        return entry.timestamp >= startTime_ && entry.timestamp < endTime_;
    }
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
