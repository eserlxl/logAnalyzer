#ifndef ANALYZER_TYPES_H
#define ANALYZER_TYPES_H

#include <string>

// Encapsulate formatting options
struct FormattingOptions {
    std::string dateTimeFormat = "%Y-%m-%d %H:%M:%S";
    bool useColor = false;
    bool includeStructuredFields = true;
    std::string structuredFieldDelimiter = ", ";
    std::string structuredFieldKvDelimiter = "=";
};

#endif // ANALYZER_TYPES_H
