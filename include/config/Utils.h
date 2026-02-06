// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef CONFIG_UTILS_H
#define CONFIG_UTILS_H

#include "core/Error.h"
#include "core/LogTypes.h"
#include <string>
#include <vector>
#include <string_view>

namespace ConfigUtils {

    /**
     * @brief Parses a single field mapping string into a FieldMapping object.
     * @param fieldMapStr The string to parse, e.g., "1=timestamp:%Y-%m-%d %H:%M:%S".
     * @return A Result containing the parsed FieldMapping or an ErrorCode::Error if parsing fails.
     */
    ErrorCode::Result<FieldMapping> parseFieldMappingString(std::string_view fieldMapStr);

    /**
     * @brief Parses a vector of field mapping strings.
     * @param fieldMapStrings A vector of strings, each in the format "groupIndex=fieldName[:format]".
     * @return A Result containing a vector of FieldMapping objects or the first ErrorCode::Error encountered.
     */
    ErrorCode::Result<std::vector<FieldMapping>> parseFieldMappingStrings(const std::vector<std::string>& fieldMapStrings);

} // namespace ConfigUtils

#endif // CONFIG_UTILS_H
