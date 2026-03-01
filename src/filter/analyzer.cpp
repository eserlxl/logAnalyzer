// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer/core.h"
#include "core/error.h"
#include "filter/core.h"
#include "filter/concrete_filters.h"
#include "filter/expression.h"
#include <algorithm>
#include <chrono>
#include <memory>
#include <vector>

namespace {
// Helper for sorting, extracted to avoid duplication.
void sortEntries(std::vector<LogEntry>& entries, filter::SortBy sortBy, filter::SortOrder sortOrder) {
    auto sortLambda = [&](const LogEntry& a, const LogEntry& b) {
        if (sortOrder == filter::SortOrder::ASCENDING) {
            return LogAnalyzer::lessByField(a, b, sortBy);
        }
        return LogAnalyzer::lessByField(b, a, sortBy);
    };

    std::ranges::sort(entries, sortLambda);
}

// Helper for creating a composite filter from criteria, extracted to avoid duplication.
ErrorCode::Result<std::shared_ptr<filter::CompositeFilter>> createFilterFromCriteria(const filter::FilterCriteria& criteria) {
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
            return std::unexpected(ErrorCode::Error(Code::InvalidRegex, regexFilterResult.error().toString()));
        }
        composite->add(regexFilterResult.value());
    }
    if (criteria.startTime.has_value() || criteria.endTime.has_value()) {
        composite->add(std::make_shared<filter::TimeRangeFilter>(
            criteria.startTime.value_or(std::chrono::system_clock::time_point::min()),
            criteria.endTime.value_or(std::chrono::system_clock::time_point::max())));
    }
    return composite;
}
} // namespace

ErrorCode::Result<filter::FilterExpression> LogAnalyzer::createFilterExpressionFromCriteria(const filter::FilterCriteria& criteria) const {
    std::vector<filter::FilterExpression> expressions;

    if (!criteria.levels.empty()) {
        std::vector<filter::FilterExpression> levelExpressions;
        for (auto l : criteria.levels) {
            levelExpressions.emplace_back(filter::FilterCondition(
                LogEntryField::LEVEL,
                filter::FilterOperator::EQUALS,
                Utils::logLevelToString(l),
                filter::FilterValueType::LOG_LEVEL
            ));
        }
        if (levelExpressions.size() > 1) {
            expressions.emplace_back(filter::FilterExpression(filter::FilterLogicalOperator::OR, std::move(levelExpressions)));
        } else if (!levelExpressions.empty()) {
            expressions.push_back(std::move(levelExpressions[0]));
        }
    }

    if (!criteria.keyword.empty()) {
        expressions.emplace_back(filter::FilterCondition(
            LogEntryField::MESSAGE,
            filter::FilterOperator::CONTAINS,
            criteria.keyword,
            filter::FilterValueType::STRING,
            criteria.keywordCaseSensitive
        ));
    }

    if (!criteria.regexPattern.empty()) {
        try {
            std::regex re(criteria.regexPattern);
        } catch (const std::regex_error& e) {
            return std::unexpected(ErrorCode::Error(Code::InvalidRegex, "Invalid regex pattern: " + criteria.regexPattern));
        }
        expressions.emplace_back(filter::FilterCondition(
            LogEntryField::MESSAGE,
            filter::FilterOperator::REGEX,
            criteria.regexPattern,
            filter::FilterValueType::REGEX
        ));
    }

    if (criteria.startTime.has_value()) {
        filter::FilterCondition startCond(
            LogEntryField::TIMESTAMP,
            filter::FilterOperator::GREATER_THAN_OR_EQUAL,
            Utils::formatTimestamp(criteria.startTime.value()),
            filter::FilterValueType::DATETIME
        );
        startCond.datetimeFormat = "%Y-%m-%d %H:%M:%S";
        expressions.emplace_back(std::move(startCond));
    }

    if (criteria.endTime.has_value()) {
        filter::FilterCondition endCond(
            LogEntryField::TIMESTAMP,
            filter::FilterOperator::LESS_THAN_OR_EQUAL,
            Utils::formatTimestamp(criteria.endTime.value()),
            filter::FilterValueType::DATETIME
        );
        endCond.datetimeFormat = "%Y-%m-%d %H:%M:%S";
        expressions.emplace_back(std::move(endCond));
    }

    if (expressions.empty()) {
        return filter::FilterExpression::makeEmpty(); // Always true if no criteria
    }

    if (expressions.size() == 1) {
        return expressions[0];
    }

    return filter::FilterExpression(filter::FilterLogicalOperator::AND, std::move(expressions));
}

ErrorCode::Result<std::vector<LogEntry>> LogAnalyzer::getFilteredEntries(const filter::FilterCriteria& criteria) const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    return getFilteredEntries_NoLock(criteria);
}

ErrorCode::Result<std::vector<LogEntry>> LogAnalyzer::getFilteredEntries_NoLock(const filter::FilterCriteria& criteria) const {
    auto filterResult = createFilterFromCriteria(criteria);
    if (!filterResult.has_value()) {
        return std::unexpected(filterResult.error());
    }
    auto filter = filterResult.value();

    std::vector<LogEntry> filtered;
    for (const auto& entry : entries_) {
        if (filter->matches(entry)) {
            filtered.push_back(entry);
        }
    }
    return filtered;
}

ErrorCode::Result<std::vector<LogEntry>> LogAnalyzer::getFilteredEntries(const filter::FilterExpression& expression) const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    return getFilteredEntries_NoLock(expression);
}

ErrorCode::Result<std::vector<LogEntry>> LogAnalyzer::getFilteredEntries_NoLock(const filter::FilterExpression& expression) const {
    auto validationResult = expression.validate();
    if (!validationResult) {
        // Use InvalidArgument as a proxy for Validation Error if not defined
        return std::unexpected(validationResult.error());
    }

    std::vector<LogEntry> filtered;
    for (const auto& entry : entries_) {
        auto evalResult = expression.evaluate(entry);
        if (!evalResult.has_value()) {
            // Propagate evaluation errors if any
            return std::unexpected(evalResult.error());
        }
        if (evalResult.value()) {
            filtered.push_back(entry);
        }
    }
    return filtered;
}

ErrorCode::Result<std::vector<LogEntry>> LogAnalyzer::getSortedFilteredEntries(const filter::FilterCriteria& criteria, filter::SortBy sortBy, filter::SortOrder sortOrder) const {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    auto filteredResult = getFilteredEntries(criteria);
#ifdef __clang__
#pragma clang diagnostic pop
#elifdef __GNUC__
#pragma GCC diagnostic pop
#endif

    if (!filteredResult.has_value()) {
        return filteredResult; // Propagate error
    }

    auto entries = filteredResult.value();
    sortEntries(entries, sortBy, sortOrder);
    return entries;
}

ErrorCode::Result<std::vector<LogEntry>> LogAnalyzer::getSortedFilteredEntries(const filter::FilterExpression& expression, filter::SortBy sortBy, filter::SortOrder sortOrder) const {
    auto filteredResult = getFilteredEntries(expression);
    if (!filteredResult.has_value()) {
        return filteredResult; // Propagate error
    }

    auto entries = filteredResult.value();
    sortEntries(entries, sortBy, sortOrder);
    return entries;
}

bool LogAnalyzer::lessByField(const LogEntry& lhs, const LogEntry& rhs, filter::SortBy key) {
    switch (key) {
        case filter::SortBy::TIMESTAMP:
            return lhs.timestamp < rhs.timestamp;
        case filter::SortBy::LEVEL:
            return Utils::logLevelToString(lhs.level) < Utils::logLevelToString(rhs.level);
        case filter::SortBy::MESSAGE:
            return lhs.message < rhs.message;
        case filter::SortBy::SOURCE:
            return lhs.sourceFile < rhs.sourceFile;
        case filter::SortBy::THREAD_ID:
            if (!lhs.threadId.has_value() && !rhs.threadId.has_value()) return false;
            if (!lhs.threadId.has_value()) return true;
            if (!rhs.threadId.has_value()) return false;
            return lhs.threadId.value() < rhs.threadId.value();
        default:
            return false;
    }
}
