// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef LOGANALYZER_ANALYZER_LOGWRITER_H
#define LOGANALYZER_ANALYZER_LOGWRITER_H

#include "core/LogTypes.h"
#include "analyzer/Types.h"
#include "filter/Legacy.h"
#include <string>
#include <string_view>
#include <ostream>

class LogAnalyzer;

class LogWriter {
public:
    explicit LogWriter(const LogAnalyzer& analyzer);

    std::string formatEntry(const LogEntry& entry, std::string_view format, const FormattingOptions& options) const;
    std::string formatEntry(const LogEntry& entry, std::string_view dateTimeFormat = "%Y-%m-%d %H:%M:%S", bool useColor = false) const;
    
    void printFilteredEntries(std::ostream& out, const FilterCriteria& criteria, const FormattingOptions& options) const;
    void printFilteredEntries(std::ostream& out, const FilterCriteria& criteria, std::string_view formatString) const;

private:
    const LogAnalyzer& analyzer_;
};

#endif // LOGANALYZER_ANALYZER_LOGWRITER_H