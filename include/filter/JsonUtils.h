#ifndef FILTER_JSON_UTILS_H
#define FILTER_JSON_UTILS_H

#include <nlohmann/json.hpp>
#include <string>
#include "core/Error.h"

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

} // namespace FilterJsonUtils

#endif // FILTER_JSON_UTILS_H
