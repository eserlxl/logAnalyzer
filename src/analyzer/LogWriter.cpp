// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <iostream>
#include <sstream>
#include <string_view>

#include "analyzer/Core.h"
#include "analyzer/LogWriter.h"
#include "core/Error.h"
#include "filter/IFilter.h"
#include "utils/String.h"
#include "utils/Time.h"

LogWriter::LogWriter(const LogAnalyzer& analyzer) : analyzer_(analyzer) {}

std::string LogWriter::formatEntry(const LogEntry& entry, const FormattingOptions& options) const {
    std::string formattedString(options.overallFormat);

    std::string timestampStr = entry.timestamp.has_value() ? 
        Utils::formatTimestamp(entry.timestamp.value(), options.dateTimeFormat) : "N/A";
    Utils::replaceAll(formattedString, "{timestamp}", timestampStr);
    
    std::string levelString = Utils::logLevelToString(entry.level);
    if (options.useColor) {
        std::string colorCode;
        switch (entry.level) {
            case LogLevel::FATAL:
            case LogLevel::ERROR:   colorCode = "\033[31m"; break;
            case LogLevel::WARNING: colorCode = "\033[33m"; break;
            case LogLevel::INFO:    colorCode = "\033[32m"; break;
            case LogLevel::DEBUG:   colorCode = "\033[34m"; break;
            case LogLevel::TRACE:   colorCode = "\033[36m"; break;
            default:                colorCode = "\033[0m";  break;
        }
        Utils::replaceAll(formattedString, "{level}", colorCode + levelString + "\033[0m");
    } else {
        Utils::replaceAll(formattedString, "{level}", levelString);
    }
    
    Utils::replaceAll(formattedString, "{message}", entry.message);
    Utils::replaceAll(formattedString, "{id}", entry.id.has_value() ? std::to_string(entry.id.value()) : "");
    Utils::replaceAll(formattedString, "{sourceFile}", entry.sourceFile);
    Utils::replaceAll(formattedString, "{lineNumber}", entry.sourceLineNumber.has_value() ? std::to_string(entry.sourceLineNumber.value()) : "");
    Utils::replaceAll(formattedString, "{threadId}", entry.threadId.has_value() ? entry.threadId.value() : "");
    Utils::replaceAll(formattedString, "{module}", entry.module.has_value() ? entry.module.value() : "");
    Utils::replaceAll(formattedString, "{host}", entry.host.has_value() ? entry.host.value() : "");

    if (options.includeStructuredFields && !entry.customFields.empty()) {
        std::ostringstream ss;
        bool firstField = true;
        for (const auto& [key, value] : entry.customFields) {
            if (!firstField) {
                ss << options.structuredFieldDelimiter;
            }
            ss << key << options.structuredFieldKvDelimiter << value;
            firstField = false;
        }
        Utils::replaceAll(formattedString, "{customFields}", ss.str());
    } else {
        Utils::replaceAll(formattedString, "{customFields}", "");
    }
    
    return formattedString;
}

void LogWriter::printFilteredEntriesInternal(std::ostream& out, const filter::FilterCriteria& criteria, const FormattingOptions& options) const {
    filter::FilterExpression combinedExpression = analyzer_.createFilterExpressionFromCriteria(criteria);
    auto filteredEntriesExpected = analyzer_.getFilteredEntries(combinedExpression);

    if (filteredEntriesExpected.has_value()) {
        const auto& filteredEntries = filteredEntriesExpected.value();
        for (const auto& entry : filteredEntries) {
            out << formatEntry(entry, options) << std::endl;
        }
    } else {
        // Generic, user-friendly error message
        out << "An error occurred while filtering log entries." << std::endl;
    }
}

void LogWriter::printFilteredEntries(std::ostream& out, const filter::FilterCriteria& criteria, const FormattingOptions& options) const {
    printFilteredEntriesInternal(out, criteria, options);
}

void LogWriter::printFilteredEntries(std::ostream& out, const filter::FilterCriteria& criteria, std::string_view overallFormatString) const {
    FormattingOptions options;
    // Set the overall format string from the parameter
    options.overallFormat = overallFormatString; 
    printFilteredEntriesInternal(out, criteria, options);
}
