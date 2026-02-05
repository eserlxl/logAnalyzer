// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2024 eserlxl

#include "analyzer/LogWriter.h"
#include "analyzer/Core.h"
#include "utils/String.h"
#include "utils/Time.h"
#include <iostream>
#include <sstream>
#include <string_view>
#include "filter/IFilter.h" // For FilterCriteria
#include "core/Error.h" // For ErrorCode

LogWriter::LogWriter(const LogAnalyzer& analyzer) : analyzer_(analyzer) {}

std::string LogWriter::formatEntry(const LogEntry& entry, std::string_view format, const FormattingOptions& options) const {
    std::string formattedString(format);

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

std::string LogWriter::formatEntry(const LogEntry& entry, std::string_view dateTimeFormat, bool useColor) const {
    FormattingOptions options;
    options.useColor = useColor;
    options.dateTimeFormat = dateTimeFormat.empty() ? "%Y-%m-%d %H:%M:%S" : std::string(dateTimeFormat);

    std::string overallFormat = "{timestamp} {level}: {message}"; 
    return formatEntry(entry, overallFormat, options);
}

void LogWriter::printFilteredEntries(std::ostream& out, const FilterCriteria& criteria, const FormattingOptions& options) const {
    auto filteredEntriesExpected = analyzer_.getFilteredEntries(criteria);
    if (filteredEntriesExpected.has_value()) {
        const auto& filteredEntries = filteredEntriesExpected.value();
        for (const auto& entry : filteredEntries) {
            out << formatEntry(entry, "{timestamp} {level}: {message}", options) << std::endl;
        }
    } else {
        out << "Error filtering entries: " << filteredEntriesExpected.error().message << std::endl;
    }
}

void LogWriter::printFilteredEntries(std::ostream& out, const FilterCriteria& criteria, std::string_view formatString) const {
    FormattingOptions options;
    options.dateTimeFormat = std::string(formatString);
    printFilteredEntries(out, criteria, options);
}
