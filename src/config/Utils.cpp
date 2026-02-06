// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "config/Utils.h"
#include "utils/Core.h"
#include <algorithm>
#include <charconv>
#include <string>
#include <format>

namespace ConfigUtils {

    ErrorCode::Result<FieldMapping> parseFieldMappingString(std::string_view fieldMapStr) {
        // Expected format: "1=timestamp:%Y-%m-%d %H:%M:%S" or "2=level" or "3=customField"
        
        size_t eqPos = fieldMapStr.find('=');
        if (eqPos == std::string_view::npos) {
             return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, std::format("Invalid field map format. Expected 'INDEX=FIELD[:FORMAT]', got: {}", fieldMapStr)));
        }

        size_t groupIndex = 0;
        auto result = std::from_chars(fieldMapStr.data(), fieldMapStr.data() + eqPos, groupIndex);
        if (result.ec != std::errc() || result.ptr != fieldMapStr.data() + eqPos) {
             return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, std::format("Invalid group index in field map: {}", fieldMapStr)));
        }

        std::string_view remainder = fieldMapStr.substr(eqPos + 1);
        size_t colonPos = remainder.find(':');
        
        std::string_view fieldNameView = (colonPos == std::string_view::npos) ? remainder : remainder.substr(0, colonPos);
        
        if (fieldNameView.empty()) {
             return std::unexpected(ErrorCode::Error(::Code::InvalidArgument, std::format("Invalid field map format. Field name cannot be empty: {}", fieldMapStr)));
        }

        std::string fieldName(fieldNameView);
        std::string format;
        if (colonPos != std::string_view::npos) {
            format = std::string(remainder.substr(colonPos + 1));
        }

        LogEntryField field = Utils::stringToLogEntryField(fieldName);
        
        if (field != LogEntryField::UNKNOWN) {
            std::vector<std::string> formats;
            if (!format.empty()) {
                formats.push_back(std::move(format));
            }
            return FieldMapping(field, std::make_optional(groupIndex), formats);
        }             // Custom field
            std::vector<std::string> formats;
            if (!format.empty()) formats.push_back(std::move(format));
            return FieldMapping(std::move(fieldName), std::make_optional(groupIndex), std::move(formats));
       
    }

    ErrorCode::Result<std::vector<FieldMapping>> parseFieldMappingStrings(const std::vector<std::string>& fieldMapStrings) {
        std::vector<FieldMapping> mappings;
        mappings.reserve(fieldMapStrings.size());
        for (const auto& s : fieldMapStrings) {
            auto res = parseFieldMappingString(s);
            if (!res) {
                return std::unexpected(res.error());
            }
            mappings.push_back(std::move(*res));
        }
        return mappings;
    }

} // namespace ConfigUtils
