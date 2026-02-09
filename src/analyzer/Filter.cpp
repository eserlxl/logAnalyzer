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


ErrorCode::Result<std::vector<LogEntry>> LogAnalyzer::getFilteredEntries(const filter::FilterCriteria& criteria) const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_); // Lock for thread safety (read-only)
    return getFilteredEntries_NoLock(criteria);
}

ErrorCode::Result<std::vector<LogEntry>> LogAnalyzer::getFilteredEntries_NoLock(const filter::FilterCriteria& criteria) const {
    std::vector<LogEntry> filtered;
    
    auto composite = std::make_shared<filter::CompositeFilter>(filter::CompositeFilter::Logic::AND);
    
    if (!criteria.levels.empty()) {
        auto levelSet = std::make_shared<filter::CompositeFilter>(filter::CompositeFilter::Logic::OR);
        for (auto l : criteria.levels) {
            levelSet->add(std::make_shared<filter::LevelFilter>(l));
        }
        composite->add(levelSet);
    }
    if (!criteria.keyword.empty()) {
        composite->add(std::make_shared<filter::KeywordFilter>(criteria.keyword, criteria.keywordCaseSensitive));
    }
    if (!criteria.regexPattern.empty()) {
        auto regexFilterResult = filter::RegexFilter::create(criteria.regexPattern);
        if (!regexFilterResult.has_value()) {
            // Forward the error from RegexFilter::create, which is std::string
            return std::unexpected(ErrorCode::Error(Code::InvalidRegex, regexFilterResult.error().toString()));
        }
        composite->add(regexFilterResult.value());
    }
    if (criteria.startTime || criteria.endTime) {
        composite->add(std::make_shared<filter::TimeRangeFilter>(
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

std::vector<LogEntry> LogAnalyzer::getSortedFilteredEntries(const filter::FilterCriteria& criteria, filter::SortBy sortBy, filter::SortOrder sortOrder) const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_); // Lock for thread safety
    std::vector<LogEntry> filtered;

    // Manually construct the filter logic to avoid calling the deprecated getFilteredEntries
    auto composite = std::make_shared<filter::CompositeFilter>(filter::CompositeFilter::Logic::AND);
    if (!criteria.levels.empty()) {
        auto levelSet = std::make_shared<filter::CompositeFilter>(filter::CompositeFilter::Logic::OR);
        for (auto l : criteria.levels) {
            levelSet->add(std::make_shared<filter::LevelFilter>(l));
        }
        composite->add(levelSet);
    }
    if (!criteria.keyword.empty()) {
        composite->add(std::make_shared<filter::KeywordFilter>(criteria.keyword, criteria.keywordCaseSensitive));
    }
    if (!criteria.regexPattern.empty()) {
        auto regexFilterResult = filter::RegexFilter::create(criteria.regexPattern);
        if (!regexFilterResult.has_value()) {
            // In case of an invalid regex, we return an empty list as we can't filter.
            return {};
        }
        composite->add(regexFilterResult.value());
    }
    if (criteria.startTime || criteria.endTime) {
        composite->add(std::make_shared<filter::TimeRangeFilter>(
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
        auto lessByField = [](const LogEntry& lhs, const LogEntry& rhs, filter::SortBy key) {
            bool result = false;
            switch (key) {
                case filter::SortBy::TIMESTAMP:
                    result = lhs.timestamp < rhs.timestamp;
                    break;
                case filter::SortBy::LEVEL:
                    result = lhs.level < rhs.level;
                    break;
                case filter::SortBy::MESSAGE:
                    result = lhs.message < rhs.message;
                    break;
                case filter::SortBy::SOURCE:
                    result = lhs.sourceFile < rhs.sourceFile;
                    break;
                case filter::SortBy::THREAD_ID:
                    // nullopt is ordered before a present value.
                    if (lhs.threadId.has_value() && rhs.threadId.has_value()) {
                        result = *lhs.threadId < *rhs.threadId;
                    } else {
                        result = lhs.threadId.has_value() < rhs.threadId.has_value();
                    }
                    break;
            }
            return result;
        };

        if (sortOrder == filter::SortOrder::ASCENDING) {
            return lessByField(a, b, sortBy);
        }
        return lessByField(b, a, sortBy);
    };

    std::sort(filtered.begin(), filtered.end(), sortLambda);

    return filtered;
}


// New implementations using FilterExpression
ErrorCode::Result<std::vector<LogEntry>> LogAnalyzer::getFilteredEntries(const filter::FilterExpression& expression) const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    return getFilteredEntries_NoLock(expression);
}

ErrorCode::Result<std::vector<LogEntry>> LogAnalyzer::getFilteredEntries_NoLock(const filter::FilterExpression& expression) const {
    if (auto res = expression.validate(); !res) {
        return std::unexpected(res.error());
    }
    std::vector<LogEntry> filtered;
    for (const auto& entry : entries_) {
        auto result = expression.evaluate(entry);
        if (result && *result) {
            filtered.push_back(entry);
        } else if (!result) {
            // Handle evaluation error, for now, we can log it or ignore the entry
            // Depending on desired strictness. Let's return the error.
            return std::unexpected(result.error());
        }
    }
    return filtered;
}

std::vector<LogEntry> LogAnalyzer::getSortedFilteredEntries(const filter::FilterExpression& expression, filter::SortBy sortBy, filter::SortOrder sortOrder) const {
    auto filtered_expected = getFilteredEntries(expression);
    if (!filtered_expected) {
        return {};
    }
    std::vector<LogEntry> filtered = filtered_expected.value();

    auto sortLambda = [&](const LogEntry& a, const LogEntry& b) {
        auto lessByField = [](const LogEntry& lhs, const LogEntry& rhs, filter::SortBy key) {
            bool result = false;
            switch (key) {
                case filter::SortBy::TIMESTAMP:
                    result = lhs.timestamp < rhs.timestamp;
                    break;
                case filter::SortBy::LEVEL:
                    result = lhs.level < rhs.level;
                    break;
                case filter::SortBy::MESSAGE:
                    result = lhs.message < rhs.message;
                    break;
                case filter::SortBy::SOURCE:
                    result = lhs.sourceFile < rhs.sourceFile;
                    break;
                case filter::SortBy::THREAD_ID:
                    if (lhs.threadId.has_value() && rhs.threadId.has_value()) {
                        result = *lhs.threadId < *rhs.threadId;
                    } else {
                        result = lhs.threadId.has_value() < rhs.threadId.has_value();
                    }
                    break;
            }
            return result;
        };

        if (sortOrder == filter::SortOrder::ASCENDING) {
            return lessByField(a, b, sortBy);
        }
        return lessByField(b, a, sortBy);
    };

    std::sort(filtered.begin(), filtered.end(), sortLambda);

    return filtered;
}
