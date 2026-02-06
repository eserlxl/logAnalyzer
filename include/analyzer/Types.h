// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef ANALYZER_TYPES_H
#define ANALYZER_TYPES_H

#include <string>

// Encapsulate formatting options
struct FormattingOptions {
    std::string dateTimeFormat = "%Y-%m-%d %H:%M:%S";
    std::string overallFormat = "{timestamp} {level}: {message}"; // Added for overall log format
    bool useColor = false;
    bool includeStructuredFields = true;
    std::string structuredFieldDelimiter = ", ";
    std::string structuredFieldKvDelimiter = "=";
};

#endif // ANALYZER_TYPES_H
