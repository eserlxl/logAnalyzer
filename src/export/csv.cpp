// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "export/core.h"
#include "utils/string.h"
#include <vector>
#include <string>

// Exports filtered log entries as CSV
void Exporter::exportAsCsv(
    std::ostream& os,
    const std::vector<LogEntry>& entries,
    const ExportSettings& settings) {

    std::vector<ExportFieldMapping> fieldsToConsider = getEffectiveExportFieldMappings(entries, settings);

    if (settings.includeHeader.value_or(true) && !fieldsToConsider.empty()) {
        for (size_t i = 0; i < fieldsToConsider.size(); ++i) {
            const auto& fieldMapping = fieldsToConsider[i];
            std::string headerName;
            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    headerName = fieldMapping.customHeader.empty() ? arg : fieldMapping.customHeader;
                } else if constexpr (std::is_same_v<T, LogEntryField>) {
                    headerName = fieldMapping.customHeader.empty() ? Utils::logEntryFieldToString(arg) : fieldMapping.customHeader;
                }
            }, fieldMapping.field);

            os << formatCsvField(headerName, settings.separator.value_or(','));
            if (i < fieldsToConsider.size() - 1) os << settings.separator.value_or(',');
        }
        os << '\n';
    }

    for (const auto& entry : entries) {
        for (size_t i = 0; i < fieldsToConsider.size(); ++i) {
            const auto& fieldMapping = fieldsToConsider[i];
            std::string value_str;
            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    if (entry.customFields.count(arg)) {
                        value_str = entry.customFields.at(arg);
                    }
                } else if constexpr (std::is_same_v<T, LogEntryField>) {
                    value_str = standardFieldValue(entry, arg, fieldMapping.datetimeFormat);
                }
            }, fieldMapping.field);

            os << formatCsvField(value_str, settings.separator.value_or(','));
            if (i < fieldsToConsider.size() - 1) os << settings.separator.value_or(',');
        }
        os << '\n';
    }
}
