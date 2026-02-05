// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 eserlxl

#include "config/ConfigUtils.h"
#include "utils/UtilsCore.h"
#include <sstream>
#include <regex>
#include <algorithm>

namespace ConfigUtils {

    ErrorCode::Result<FieldMapping> parseFieldMappingString(std::string_view fieldMapStr) {
        // Expected format: "1=timestamp:%Y-%m-%d %H:%M:%S" or "2=level" or "3=customField"
        // Regex: ^(\d+)=([^:]+)(?::(.*))?$
        
        static const std::regex mapRegex(R"(^(\d+)=([^:]+)(?::(.*))?$)");
        std::cmatch match;
        std::string str(fieldMapStr); // Regex requires std::string or const char*

        if (std::regex_match(str.c_str(), match, mapRegex)) {
            try {
                size_t groupIndex = std::stoul(match[1].str());
                std::string fieldName = match[2].str();
                std::string format = match[3].matched ? match[3].str() : "";

                // Trim whitespace from fieldName? Probably safer not to, user might want it.
                // But normally keys don't have spaces. Let's assume strict format for now.

                LogEntryField field = Utils::stringToLogEntryField(fieldName);
                
                if (field != LogEntryField::UNKNOWN) {
                    return FieldMapping(field, groupIndex, format);
                } else {
                    // Custom field
                    std::vector<std::string> formats;
                    if (!format.empty()) formats.push_back(format);
                    return FieldMapping(fieldName, std::make_optional(groupIndex), formats);
                }
            } catch (...) {
                return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Invalid group index in field map: " + str));
            }
        }

        return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, "Invalid field map format. Expected 'INDEX=FIELD[:FORMAT]', got: " + str));
    }

    ErrorCode::Result<std::vector<FieldMapping>> parseFieldMappingStrings(const std::vector<std::string>& fieldMapStrings) {
        std::vector<FieldMapping> mappings;
        for (const auto& s : fieldMapStrings) {
            auto res = parseFieldMappingString(s);
            if (!res) {
                return std::unexpected(res.error());
            }
            mappings.push_back(*res);
        }
        return mappings;
    }

} // namespace ConfigUtils
