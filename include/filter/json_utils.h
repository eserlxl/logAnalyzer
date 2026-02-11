// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector> // Required for std::vector in validateTimestamp
#include "core/error.h"
#include "utils/time.h" // For Utils::parseTimeWithFormats

namespace filter {
namespace FilterJsonUtils {

    /**
     * @brief Creates an error with a properly formatted JSON path.
     */
    inline ErrorCode::Error makeError(Code c, const std::string& msg, const std::string& currentPath, const std::string& fieldName) {
        std::string path = currentPath;
        if (path.empty()) {
            path = "/" + fieldName;
        } else if (path == "/") {
            path = "/" + fieldName;
        } else if (!fieldName.empty()) {
            path += "/" + fieldName;
        }
        return ErrorCode::Error(c, msg, path);
    }

    /**
     * @brief Gets a required value from JSON or returns an error.
     */
    template <typename T>
    inline ErrorCode::Result<T> getRequired(const nlohmann::json& j, const std::string& key, const std::string& currentPath) {
        if (!j.contains(key)) {
            return std::unexpected(makeError(Code::InvalidArgument, "Missing required key: '" + key + "'", currentPath, key));
        }
        try {
            return j.at(key).get<T>();
        } catch (...) {
            return std::unexpected(makeError(Code::InvalidArgument, "Invalid type for key: '" + key + "'", currentPath, key));
        }
    }

    /**
     * @brief Gets an optional value from JSON.
     */
    template <typename T>
    inline std::optional<T> getOptional(const nlohmann::json& j, const std::string& key) {
        if (j.contains(key) && !j.at(key).is_null()) {
            try {
                return j.at(key).get<T>();
            } catch (...) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Validates a datetime string against a set of formats.
     * @param datetimeStr The string to validate.
     * @param formats A vector of format strings to try.
     * @param currentPath The JSON path for error reporting.
     * @param fieldName The field name for error reporting.
     * @return ErrorCode::Result<void> indicating success or failure.
     */
    inline ErrorCode::Result<void> validateTimestamp(const std::string& datetimeStr, const std::vector<std::string>& formats, const std::string& currentPath, const std::string& fieldName) {
        if (formats.empty()) {
            return std::unexpected(makeError(Code::InvalidArgument, "No datetime formats provided for validation.", currentPath, fieldName));
        }
        
        auto parseResult = Utils::parseTimeWithFormats(datetimeStr, formats);
        if (!parseResult) {
            return std::unexpected(makeError(Code::InvalidArgument, "Failed to parse datetime value: " + parseResult.error().message, currentPath, fieldName));
        }
        return {}; // Success
    }

} // namespace FilterJsonUtils
} // namespace filter
