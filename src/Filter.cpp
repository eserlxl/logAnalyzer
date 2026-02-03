#include "../include/Filter.h"
#include "../include/Utils.h"
#include <algorithm>
#include <cctype>

namespace {
// Helper for case-insensitive string comparison
bool caseInsensitiveEquals(const std::string& str1, const std::string& str2) {
    return std::equal(str1.begin(), str1.end(),
                      str2.begin(), str2.end(),
                      [](char a, char b){
                          return std::tolower(a) == std::tolower(b);
                      });
}

// Helper for case-insensitive substring search
bool caseInsensitiveSearch(const std::string& text, const std::string& keyword) {
    auto it = std::search(
        text.begin(), text.end(),
        keyword.begin(), keyword.end(),
        [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); }
    );
    return it != text.end();
}
} // anonymous namespace

SourceFileFilter::SourceFileFilter(std::string pattern, PatternType type, bool caseSensitive)
    : pattern_(std::move(pattern)), type_(type), caseSensitive_(caseSensitive) {
    if (type_ == PatternType::Regex) {
        auto flags = std::regex::ECMAScript;
        if (!caseSensitive_) {
            flags |= std::regex::icase;
        }
        regexPattern_ = std::regex(pattern_, flags);
    } else if (type_ == PatternType::Glob) {
        // Convert glob pattern to regex for internal use
        std::string regexStr;
        for (char c : pattern_) {
            switch (c) {
                case '*':  regexStr += ".*"; break;
                case '?':  regexStr += "."; break;
                case '.':  regexStr += "\\."; break;
                // Escape other regex special characters if they appear in a literal glob part
                case '+':
                case '(':
                case ')':
                case '[':
                case ']':
                case '{':
                case '}':
                case '^':
                case '$':
                case '|':
                case '\\':
                    regexStr += '\\';
                    regexStr += c;
                    break;
                default:   regexStr += c; break;
            }
        }
        regexPattern_ = std::regex(regexStr, caseSensitive_ ? std::regex::ECMAScript : std::regex::icase);
    }
}

bool SourceFileFilter::matches(const LogEntry &entry) const {
    if (type_ == PatternType::Literal) {
        if (caseSensitive_) {
            return entry.sourceFile == pattern_;
        } else {
            return caseInsensitiveEquals(entry.sourceFile, pattern_);
        }
    } else if (type_ == PatternType::Glob) {
        // We converted glob to regex in the constructor
        return std::regex_match(entry.sourceFile, regexPattern_);
    } else { // PatternType::Regex
        return std::regex_search(entry.sourceFile, regexPattern_);
    }
}

FieldExistsFilter::FieldExistsFilter(std::string fieldKey) : fieldKey_(std::move(fieldKey)) {}

bool FieldExistsFilter::matches(const LogEntry &entry) const {
    return entry.structuredFields.count(fieldKey_) > 0;
}

FieldValueFilter::FieldValueFilter(std::string fieldKey, std::string valuePattern, PatternType type, bool caseSensitive)
    : fieldKey_(std::move(fieldKey)), valuePattern_(std::move(valuePattern)), type_(type), caseSensitive_(caseSensitive) {
    if (type_ == PatternType::Regex) {
        auto flags = std::regex::ECMAScript;
        if (!caseSensitive_) {
            flags |= std::regex::icase;
        }
        regexPattern_ = std::regex(valuePattern_, flags);
    } else if (type_ == PatternType::Glob) {
        // Convert glob pattern to regex for internal use
        std::string regexStr;
        for (char c : valuePattern_) {
            switch (c) {
                case '*':  regexStr += ".*"; break;
                case '?':  regexStr += "."; break;
                case '.':  regexStr += "\\."; break;
                // Escape other regex special characters if they appear in a literal glob part
                case '+':
                case '(':
                case ')':
                case '[':
                case ']':
                case '{':
                case '}':
                case '^':
                case '$':
                case '|':
                case '\\':
                    regexStr += '\\';
                    regexStr += c;
                    break;
                default:   regexStr += c; break;
            }
        }
        regexPattern_ = std::regex(regexStr, caseSensitive_ ? std::regex::ECMAScript : std::regex::icase);
    }
}

bool FieldValueFilter::matches(const LogEntry &entry) const {
    auto it = entry.structuredFields.find(fieldKey_);
    if (it == entry.structuredFields.end()) {
        return false; // Field not found
    }

    const std::string& actualValue = it->second;

    if (type_ == PatternType::Literal) {
        if (caseSensitive_) {
            return actualValue == valuePattern_;
        } else {
            return caseInsensitiveEquals(actualValue, valuePattern_);
        }
    } else if (type_ == PatternType::Glob) {
        return std::regex_match(actualValue, regexPattern_);
    } else { // PatternType::Regex
        return std::regex_search(actualValue, regexPattern_);
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
            return caseInsensitiveSearch(text, keyword);
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

RegexFilter::RegexFilter(std::string pattern, std::regex_constants::syntax_option_type flags)
    : pattern_(pattern, flags) {}

RegexFilter::RegexFilter(std::string pattern, bool caseSensitive)
    : pattern_(pattern, caseSensitive ? std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase) {}
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
        return std::unexpected(startTime.error());
    }

    auto endTime = Utils::parseAbsoluteTime(end);
    if (!endTime) {
        return std::unexpected(endTime.error());
    }

    return TimeRangeFilter(*startTime, *endTime);
}

std::expected<TimeRangeFilter, std::string> TimeRangeFilter::since(const std::string& relativeTime) {
    auto startTime = Utils::parseRelativeTime(relativeTime);
    if (!startTime) {
        return std::unexpected(startTime.error());
    }
    // 'since' creates a range from the relative time up to now.
    return TimeRangeFilter(*startTime, std::chrono::system_clock::now());
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
    } else { // OR
        for (const auto &filter : filters_) {
            if (filter->matches(entry)) return true;
        }
        return false;
    }
}
