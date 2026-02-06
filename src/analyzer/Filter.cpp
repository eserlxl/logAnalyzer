// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer/Core.h"
#include "filter/Core.h"
#include <algorithm>
#include <memory>
#include <vector>
#include <expected>
#include <regex>

#include "core/Error.h" // Explicitly include Error.h


ErrorCode::Result<std::vector<LogEntry>> LogAnalyzer::getFilteredEntries(const FilterCriteria& criteria) const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_); // Lock for thread safety (read-only)
    return getFilteredEntries_NoLock(criteria);
}

ErrorCode::Result<std::vector<LogEntry>> LogAnalyzer::getFilteredEntries_NoLock(const FilterCriteria& criteria) const {
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
            // Forward the error from RegexFilter::create, which is std::string
            return std::unexpected(ErrorCode::Error(Code::InvalidRegex, regexFilterResult.error().toString()));
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
    std::shared_lock<std::shared_mutex> lock(stateMutex_); // Lock for thread safety
    std::vector<LogEntry> filtered;

    // Manually construct the filter logic to avoid calling the deprecated getFilteredEntries
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
            // In case of an invalid regex, we return an empty list as we can't filter.
            return {};
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
            case SortBy::SOURCE:
                result = a.sourceFile < b.sourceFile;
                break;
            case SortBy::THREAD_ID:
                // Handle optional: nullopt is considered "less than" a value.
                if (a.threadId.has_value() && b.threadId.has_value()) {
                    result = *a.threadId < *b.threadId;
                } else {
                    result = a.threadId.has_value() < b.threadId.has_value();
                }
                break;
        }
        return (sortOrder == SortOrder::ASCENDING) ? result : !result;
    };

    std::sort(filtered.begin(), filtered.end(), sortLambda);

    return filtered;
}


// New implementations using FilterExpression
ErrorCode::Result<std::vector<LogEntry>> LogAnalyzer::getFilteredEntries(const FilterExpression& expression) const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    return getFilteredEntries_NoLock(expression);
}

ErrorCode::Result<std::vector<LogEntry>> LogAnalyzer::getFilteredEntries_NoLock(const FilterExpression& expression) const {
    std::vector<LogEntry> filtered;
    for (const auto& entry : entries_) {
        auto result = expression.evaluate(entry);
        if (result.has_value() && result.value()) {
            filtered.push_back(entry);
        } else if (!result.has_value()) {
            // Handle evaluation error, for now, we can log it or ignore the entry
            // Depending on desired strictness. Let's return the error.
            return std::unexpected(result.error());
        }
    }
    return filtered;
}

std::vector<LogEntry> LogAnalyzer::getSortedFilteredEntries(const FilterExpression& expression, SortBy sortBy, SortOrder sortOrder) const {
    auto filtered_expected = getFilteredEntries(expression);
    if (!filtered_expected) {
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
            case SortBy::SOURCE:
                result = a.sourceFile < b.sourceFile;
                break;
            case SortBy::THREAD_ID:
                if (a.threadId.has_value() && b.threadId.has_value()) {
                    result = *a.threadId < *b.threadId;
                } else {
                    result = a.threadId.has_value() < b.threadId.has_value();
                }
                break;
        }
        return (sortOrder == SortOrder::ASCENDING) ? result : !result;
    };

    std::sort(filtered.begin(), filtered.end(), sortLambda);

    return filtered;
}
