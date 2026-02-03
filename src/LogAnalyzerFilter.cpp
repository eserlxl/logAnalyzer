#include "LogAnalyzer.h"
#include "Filter.h"
#include <algorithm>
#include <memory>
#include <vector>
#include <expected>
#include <regex>

std::expected<std::vector<LogEntry>, LogParseError> LogAnalyzer::getFilteredEntries(const FilterCriteria& criteria) const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock for thread safety
    return getFilteredEntries_NoLock(criteria);
}

// Non-locking version for internal use
std::expected<std::vector<LogEntry>, LogParseError> LogAnalyzer::getFilteredEntries_NoLock(const FilterCriteria& criteria) const {
    std::vector<LogEntry> filtered;
    
    auto composite = std::make_shared<CompositeFilter>(CompositeFilter::Logic::AND);
    
    if (!criteria.levels.empty()) {
        auto levelSet = std::make_shared<CompositeFilter>(CompositeFilter::Logic::OR);
        for (auto l : criteria.levels) {
            levelSet->add(std::make_shared<LevelFilter>(l));
        }
        composite->add(levelSet);
    }
    if (!criteria.keyword.empty()) {
        composite->add(std::make_shared<KeywordFilter>(criteria.keyword, criteria.keywordCaseSensitive));
    }
    if (!criteria.regexPattern.empty()) {
        auto regexFilterResult = RegexFilter::create(criteria.regexPattern);
        if (!regexFilterResult.has_value()) {
            return std::unexpected(LogParseError{ParseError::INVALID_REGEX_PATTERN, regexFilterResult.error(), 0});
        }
        composite->add(regexFilterResult.value());
    }
    if (criteria.startTime || criteria.endTime) {
        composite->add(std::make_shared<TimeRangeFilter>(
            criteria.startTime.value_or(std::chrono::system_clock::time_point::min()),
            criteria.endTime.value_or(std::chrono::system_clock::time_point::max())
        ));
    }

    for (const auto& entry : entries_) {
        if (composite->matches(entry)) {
            filtered.push_back(entry);
        }
    }
    return filtered;
}

std::vector<LogEntry> LogAnalyzer::getSortedFilteredEntries(const FilterCriteria& criteria, SortBy sortBy, SortOrder sortOrder) const {
    auto filtered_expected = getFilteredEntries(criteria); // This already locks internally
    if (!filtered_expected) {
        // Handle the error case from getFilteredEntries.
        // For this function, it might be appropriate to return an empty vector or rethrow.
        // Returning empty vector for now.
        return {};
    }
    std::vector<LogEntry> filtered = filtered_expected.value();

    auto sortLambda = [&](const LogEntry& a, const LogEntry& b) {
        bool result = false;
        switch (sortBy) {
            case SortBy::TIMESTAMP:
                result = a.timestamp < b.timestamp;
                break;
            case SortBy::LEVEL:
                result = a.level < b.level;
                break;
            case SortBy::MESSAGE:
                result = a.message < b.message;
                break;
        }
        return (sortOrder == SortOrder::ASCENDING) ? result : !result;
    };

    std::sort(filtered.begin(), filtered.end(), sortLambda);

    return filtered;
}
