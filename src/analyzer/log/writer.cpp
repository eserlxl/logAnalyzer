// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <iostream>
#include <sstream>
#include <string_view>
#include <map>    // Required for std::map to store placeholders

#include "analyzer/core.h"
#include "analyzer/log/writer.h"
#include "filter/i_filter.h"
#include "utils/string.h"
#include "utils/time.h"

// ANSI Color Codes
namespace ansi_color {
    const std::string reset  = "\033[0m";
    const std::string red    = "\033[31m";
    const std::string yellow = "\033[33m";
    const std::string green  = "\033[32m";
    const std::string blue   = "\033[34m";
    const std::string cyan   = "\033[36m";
}

LogWriter::LogWriter(const LogAnalyzer& analyzer) : analyzer_(analyzer) {}

std::string LogWriter::formatEntry(const LogEntry& entry, const FormattingOptions& options) const {
    std::map<std::string_view, std::string> replacements;

    // Populate replacements map
    replacements["{timestamp}"] = entry.timestamp ?
        Utils::formatTimestamp(*entry.timestamp, options.dateTimeFormat) : "N/A";

    std::string levelString = Utils::logLevelToString(entry.level);
    if (options.useColor) {
        std::string colorCode;
        switch (entry.level) {
            case LogLevel::FATAL:
            case LogLevel::ERROR:   colorCode = ansi_color::red; break;
            case LogLevel::WARNING: colorCode = ansi_color::yellow; break;
            case LogLevel::INFO:    colorCode = ansi_color::green; break;
            case LogLevel::DEBUG:   colorCode = ansi_color::blue; break;
            case LogLevel::TRACE:   colorCode = ansi_color::cyan; break;
            default:                colorCode = ansi_color::reset;  break;
        }
        replacements["{level}"] = colorCode + levelString + ansi_color::reset;
    } else {
        replacements["{level}"] = levelString;
    }

    replacements["{message}"] = entry.message;
    replacements["{id}"] = entry.id ? std::to_string(*entry.id) : "";
    replacements["{sourceFile}"] = entry.sourceFile;
    replacements["{lineNumber}"] = entry.sourceLineNumber ? std::to_string(*entry.sourceLineNumber) : "";
    replacements["{threadId}"] = entry.threadId ? *entry.threadId : "";
    replacements["{module}"] = entry.module ? *entry.module : "";
    replacements["{host}"] = entry.host ? *entry.host : "";

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
        replacements["{customFields}"] = ss.str();
    } else {
        replacements["{customFields}"] = "";
    }

    // Now, perform a single pass replacement on options.overallFormat
    std::ostringstream resultStream;
    std::string_view formatView(options.overallFormat);
    size_t currentPos = 0;

    // Define a fixed order of placeholders to search to optimize find operations slightly
    // This avoids iterating through the whole map for every chunk
    const std::vector<std::string_view> placeholderKeys = {
        "{timestamp}", "{level}", "{message}", "{id}", "{sourceFile}",
        "{lineNumber}", "{threadId}", "{module}", "{host}", "{customFields}"
    };


    while (currentPos < formatView.length()) {
        size_t nextPlaceholderPos = std::string::npos;
        std::string_view foundPlaceholderKey;

        // Find the earliest occurring placeholder from the predefined keys
        for (const auto& key : placeholderKeys) {
            size_t pos = formatView.find(key, currentPos);
            if (pos != std::string::npos) {
                if (nextPlaceholderPos == std::string::npos || pos < nextPlaceholderPos) {
                    nextPlaceholderPos = pos;
                    foundPlaceholderKey = key;
                }
            }
        }

        if (nextPlaceholderPos != std::string::npos) {
            // Append the text before the placeholder
            resultStream << formatView.substr(currentPos, nextPlaceholderPos - currentPos);
            // Append the replacement value
            resultStream << replacements.at(foundPlaceholderKey);
            // Move current position past the found placeholder
            currentPos = nextPlaceholderPos + foundPlaceholderKey.length();
        } else {
            // No more placeholders found, append the rest of the string
            resultStream << formatView.substr(currentPos);
            currentPos = formatView.length(); // End loop
        }
    }

    return resultStream.str();
}

std::string LogWriter::formatEntry(const LogEntry& entry, std::string_view dateTimeFormat, bool useColor) const {
    FormattingOptions options;
    options.dateTimeFormat = dateTimeFormat;
    options.useColor = useColor;
    return formatEntry(entry, options);
}

void LogWriter::printFilteredEntriesInternal(std::ostream& out, const filter::FilterCriteria& criteria, const FormattingOptions& options) const {
    auto exprResult = analyzer_.createFilterExpressionFromCriteria(criteria);
    if (!exprResult) {
        out << "Error: Invalid filter criteria: " << exprResult.error().message << '\n';
        return;
    }
    auto filteredEntriesExpected = analyzer_.getFilteredEntries(*exprResult);

    if (filteredEntriesExpected) {
        const auto& filteredEntries = *filteredEntriesExpected;
        for (const auto& entry : filteredEntries) {
            out << formatEntry(entry, options) << '\n';
        }
    } else {
        // More informative error message
        out << "Error: Failed to retrieve filtered log entries or no entries matched the criteria." << '\n';
    }
}

void LogWriter::printFilteredEntries(std::ostream& out, const filter::FilterCriteria& criteria, const FormattingOptions& options) const {
    printFilteredEntriesInternal(out, criteria, options);
}

void LogWriter::printFilteredEntries(std::ostream& out, const filter::FilterCriteria& criteria, std::string_view overallFormatString) const {
    FormattingOptions options;
    // Set the overall format string from the parameter. Other options use their default values.
    options.overallFormat = overallFormatString;
    printFilteredEntriesInternal(out, criteria, options);
}
