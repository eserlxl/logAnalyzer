// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer/core.h"
#include "export/json.h"
#include "export/csv.h"
#include "filter/expression.h"
#include <vector>
#include <ostream>

ErrorCode::Result<void> LogAnalyzer::exportAsJson(
    std::ostream& out, 
    const filter::FilterExpression& expression, 
    bool includeSummary) const 
{
    auto result = getFilteredEntries(expression);
    if (!result.has_value()) {
        return std::unexpected(result.error());
    }

    nlohmann::json j;
    if (includeSummary) {
        j["summary"]["total_entries"] = result.value().size();
    }
    j["entries"] = nlohmann::json::array();
    for (const auto& entry : result.value()) {
        j["entries"].push_back(entry.toJson());
    }

    out << j.dump(4);
    return {};
}

ErrorCode::Result<void> LogAnalyzer::exportAsCsv(
    std::ostream& out, 
    const filter::FilterExpression& expression, 
    bool includeHeader) const 
{
    auto result = getFilteredEntries(expression);
    if (!result.has_value()) {
        return std::unexpected(result.error());
    }

    // This is a placeholder implementation.
    // A more robust implementation would use a proper CSV library.
    if (includeHeader) {
        out << "id,timestamp,level,message,source_file\n";
    }
    for (const auto& entry : result.value()) {
        out << entry.id.value_or(0) << ","
            // << entry.timestamp << ","
            << static_cast<int>(entry.level) << ","
            << "\"" << entry.message << "\","
            << entry.sourceFile << "\n";
    }

    return {};
}
