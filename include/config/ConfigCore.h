#ifndef LOG_ANALYZER_CONFIG_H
#define LOG_ANALYZER_CONFIG_H

#include "config/Settings.h"

// This header acts as a compatibility facade, providing backward compatibility.
// It includes core settings and exposes the default log regex pattern.
// Actual implementation details are managed by config/Settings.h.

static constexpr std::string_view DEFAULT_LOG_REGEX_PATTERN_SV = DEFAULT_LOG_REGEX_PATTERN_INTERNAL;

#endif // LOG_ANALYZER_CONFIG_H
