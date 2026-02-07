// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 eserlxl

#include "core/LogTypes.h"
#include "utils/Time.h" // For Utils::formatTimestamp

nlohmann::json LogEntry::toJson() const {
    nlohmann::json j;

    if (id) j["id"] = *id;
    j["sourceFile"] = sourceFile;
    if (sourceLineNumber) j["sourceLineNumber"] = *sourceLineNumber;
    if (timestamp) j["timestamp"] = Utils::formatTimestamp(*timestamp);
    j["level"] = Utils::logLevelToString(level);
    j["message"] = message;
    if (threadId) j["threadId"] = *threadId;
    if (module) j["module"] = *module;
    if (host) j["host"] = *host;

    if (!customFields.empty()) {
        nlohmann::json custom_json;
        for (const auto& [key, value] : customFields) {
            // Attempt to parse value as JSON if it looks like one, otherwise keep as string
            try {
                if (value.length() > 1 && ((value.front() == '{' && value.back() == '}') || (value.front() == '[' && value.back() == ']'))) {
                    custom_json[key] = nlohmann::json::parse(value);
                } else {
                    custom_json[key] = value;
                }
            } catch (const nlohmann::json::parse_error&) {
                custom_json[key] = value;
            }
        }
        j["customFields"] = custom_json;
    }

    if (structuredData) {
        // Attempt to parse structuredData as JSON if it looks like one
        try {
            if (structuredData->length() > 1 && ((structuredData->front() == '{' && structuredData->back() == '}') || (structuredData->front() == '[' && structuredData->back() == ']'))) {
                j["structuredData"] = nlohmann::json::parse(*structuredData);
            } else {
                j["structuredData"] = *structuredData;
            }
        } catch (const nlohmann::json::parse_error&) {
            j["structuredData"] = *structuredData;
        }
    }

    if (!parsingErrors.empty()) {
        nlohmann::json errors_json = nlohmann::json::array();
        for (const auto& err : parsingErrors) {
            errors_json.push_back({
                {"code", ErrorCode::toString(err.code)},
                {"message", err.message}
            });
        }
        j["parsingErrors"] = errors_json;
    }

    return j;
}
