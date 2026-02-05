#ifndef LOG_ANALYZER_CONFIG_H
#define LOG_ANALYZER_CONFIG_H

#include "config/Settings.h"

// This header remains for backward compatibility and as a convenience facade.
// Implementation has been moved to src/config/Core.cpp

static constexpr std::string_view DEFAULT_LOG_REGEX_PATTERN_SV = DEFAULT_LOG_REGEX_PATTERN_INTERNAL;

#endif // LOG_ANALYZER_CONFIG_H
