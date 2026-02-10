// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef ANALYZER_LOG_WRITER_H
#define ANALYZER_LOG_WRITER_H

#include "core/log/types.h"
#include "filter/core.h"
#include "analyzer/types.h"
#include <string>
#include <string_view>
#include <ostream>

// Forward declarations
class LogAnalyzer;

class LogWriter {
public:
    explicit LogWriter(const LogAnalyzer& analyzer);

    std::string formatEntry(const LogEntry& entry, const FormattingOptions& options) const;
    std::string formatEntry(const LogEntry& entry, std::string_view dateTimeFormat = "%Y-%m-%d %H:%M:%S", bool useColor = false) const;

    void printFilteredEntries(std::ostream& out, const filter::FilterCriteria& criteria, const FormattingOptions& options) const;
    void printFilteredEntries(std::ostream& out, const filter::FilterCriteria& criteria, std::string_view overallFormatString) const;

private:
    const LogAnalyzer& analyzer_;
    void printFilteredEntriesInternal(std::ostream& out, const filter::FilterCriteria& criteria, const FormattingOptions& options) const;
};

#endif // ANALYZER_LOG_WRITER_H
