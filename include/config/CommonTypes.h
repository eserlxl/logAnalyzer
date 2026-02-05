#ifndef CONFIG_COMMON_TYPES_H
#define CONFIG_COMMON_TYPES_H

#include "core/LogTypes.h" // For LogLevel
#include "filter/Types.h"  // For SortBy, SortOrder
#include "filter/Core.h"   // For CompositeFilter::Logic
#include "config/Settings.h" // For ParserErrorAction (nested enum)
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

    // Case-insensitive map for Filter Logic strings
    const static std::map<std::string, CompositeFilter::Logic, LogAnalyzerInternal::ci_less> FilterLogicMap = {
        {"AND", CompositeFilter::Logic::AND}, {"OR", CompositeFilter::Logic::OR}
    };

    // Case-insensitive map for SortBy strings
    const static std::map<std::string, SortBy, LogAnalyzerInternal::ci_less> SortByMap = {
        {"time", SortBy::TIMESTAMP}, {"timestamp", SortBy::TIMESTAMP}, 
        {"level", SortBy::LEVEL}, {"msg", SortBy::MESSAGE}, {"message", SortBy::MESSAGE},
        {"source", SortBy::SOURCE}, {"source_file", SortBy::SOURCE},
        {"thread", SortBy::THREAD_ID}, {"thread_id", SortBy::THREAD_ID}
    };

    // Case-insensitive map for SortOrder strings
    const static std::map<std::string, SortOrder, LogAnalyzerInternal::ci_less> SortOrderMap = {
        {"asc", SortOrder::ASCENDING}, {"ascending", SortOrder::ASCENDING},
        {"desc", SortOrder::DESCENDING}, {"descending", SortOrder::DESCENDING}
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
