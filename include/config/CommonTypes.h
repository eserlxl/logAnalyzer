// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef CONFIG_COMMON_TYPES_H
#define CONFIG_COMMON_TYPES_H

#include "core/Log/Types.h" // For LogLevel
#include "filter/Types.h"  // For SortBy, SortOrder, FilterLogicalOperator
#include "core/CiLess.h" // For LogAnalyzerInternal::ci_less

#include <map>
#include <string>

namespace Config {

    enum class ColorOption {
        ALWAYS, AUTO, NEVER
    };

    // Case-insensitive map for LogLevel strings
    const static std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> LogLevelMap = {
        {"TRACE", LogLevel::TRACE}, {"DEBUG", LogLevel::DEBUG}, {"INFO", LogLevel::INFO},
        {"WARNING", LogLevel::WARNING}, {"ERROR", LogLevel::ERROR}, {"FATAL", LogLevel::FATAL},
        {"CRITICAL", LogLevel::CRITICAL}, // Added CRITICAL to match Utils
        {"UNKNOWN", LogLevel::UNKNOWN}
    };

    // Case-insensitive map for filter expression logic strings
    const static std::map<std::string, filter::FilterLogicalOperator, LogAnalyzerInternal::ci_less> FilterLogicMap = {
        {"AND", filter::FilterLogicalOperator::AND}, {"OR", filter::FilterLogicalOperator::OR}
    };

    // Case-insensitive map for SortBy strings
    const static std::map<std::string, filter::SortBy, LogAnalyzerInternal::ci_less> SortByMap = {
        {"time", filter::SortBy::TIMESTAMP}, {"timestamp", filter::SortBy::TIMESTAMP}, 
        {"level", filter::SortBy::LEVEL}, {"msg", filter::SortBy::MESSAGE}, {"message", filter::SortBy::MESSAGE},
        {"source", filter::SortBy::SOURCE}, {"source_file", filter::SortBy::SOURCE},
        {"thread", filter::SortBy::THREAD_ID}, {"thread_id", filter::SortBy::THREAD_ID}
    };

    // Case-insensitive map for SortOrder strings
    const static std::map<std::string, filter::SortOrder, LogAnalyzerInternal::ci_less> SortOrderMap = {
        {"asc", filter::SortOrder::ASCENDING}, {"ascending", filter::SortOrder::ASCENDING},
        {"desc", filter::SortOrder::DESCENDING}, {"descending", filter::SortOrder::DESCENDING}
    };

    // Case-insensitive map for ColorOption strings
    const static std::map<std::string, ColorOption, LogAnalyzerInternal::ci_less> ColorOptionMap = {
        {"always", ColorOption::ALWAYS}, {"auto", ColorOption::AUTO}, {"never", ColorOption::NEVER}
    };

    // Case-insensitive map for ParserErrorAction strings
    const static std::map<std::string, ParserErrorAction, LogAnalyzerInternal::ci_less> ParserErrorActionMap = {
        {"ignore", ParserErrorAction::Ignore}, {"skip", ParserErrorAction::Ignore}, // Alias 'skip' to 'ignore' as per CLI help text
        {"warn", ParserErrorAction::Warn}, {"log", ParserErrorAction::Warn}, // Alias 'log' to 'warn'
        {"throw", ParserErrorAction::Throw}, {"fail", ParserErrorAction::Throw} // Alias 'fail' to 'throw'
    };

} // namespace Config

#endif // CONFIG_COMMON_TYPES_H
