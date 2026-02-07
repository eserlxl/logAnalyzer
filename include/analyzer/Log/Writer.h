// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef LOGANALYZER_ANALYZER_LOGWRITER_H
#define LOGANALYZER_ANALYZER_LOGWRITER_H

#include "core/Log/Types.h"
#include "analyzer/Types.h"
#include "filter/Legacy.h"
#include <string>
#include <string_view>
#include <ostream>

class LogAnalyzer;

class LogWriter {
public:
    explicit LogWriter(const LogAnalyzer& analyzer);

    static std::string formatEntry(const LogEntry& entry, const FormattingOptions& options) ;
    
    void printFilteredEntries(std::ostream& out, const filter::FilterCriteria& criteria, const FormattingOptions& options) const;
    void printFilteredEntries(std::ostream& out, const filter::FilterCriteria& criteria, std::string_view overallFormatString) const;

private:
    void printFilteredEntriesInternal(std::ostream& out, const filter::FilterCriteria& criteria, const FormattingOptions& options) const;

private:
    const LogAnalyzer& analyzer_;
};

#endif // LOGANALYZER_ANALYZER_LOGWRITER_H