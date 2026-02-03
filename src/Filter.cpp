#include "../include/Filter.h"
#include <algorithm>
#include <cctype>

bool KeywordFilter::matches(const LogEntry &entry) const {
    std::string msg = entry.message;
    std::string key = keyword_;
    if (!caseSensitive_) {
        std::transform(msg.begin(), msg.end(), msg.begin(), 
                       [](unsigned char c){ return std::tolower(c); });
        std::transform(key.begin(), key.end(), key.begin(), 
                       [](unsigned char c){ return std::tolower(c); });
    }
    return msg.find(key) != std::string::npos;
}

RegexFilter::RegexFilter(std::string pattern, std::regex_constants::syntax_option_type flags)
    : pattern_(pattern, flags) {}

bool RegexFilter::matches(const LogEntry &entry) const {
    return std::regex_search(entry.message, pattern_);
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
