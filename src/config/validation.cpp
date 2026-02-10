// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "config/settings.h"
#include "config/utils.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <regex>
#include <set>
#include <optional>

namespace {

std::optional<std::string> normalizeTargetFieldName(std::string_view rawField) {
    if (rawField.empty()) {
        return std::nullopt;
    }
    std::string field(rawField);
    const auto first = field.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return std::nullopt;
    }
    const auto last = field.find_last_not_of(" \t");
    field = field.substr(first, last - first + 1);
    std::transform(field.begin(), field.end(), field.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (field == "level") return "level";
    if (field == "message") return "message";
    if (field == "source" || field == "source_file" || field == "sourcefile") return "sourceFile";
    if (field == "timestamp" || field == "time") return "timestamp";
    if (field == "line" || field == "line_number" || field == "linenumber") return "lineNumber";
    if (field == "thread" || field == "thread_id" || field == "threadid" || field == "tid") return "threadId";
    if (field == "module") return "module";
    if (field == "host") return "host";
    if (field == "custom" || field == "custom_fields" || field == "customfields") return "customFields";
    return std::nullopt;
}

} // namespace

std::vector<std::string> LogAnalyzerSettings::validate() const {
    std::vector<std::string> errors;
    const auto isStrictInteger = [](const std::string& value) {
        if (value.empty()) {
            return false;
        }
        long long parsed = 0;
        const char* begin = value.data();
        const char* end = begin + value.size();
        auto [ptr, ec] = std::from_chars(begin, end, parsed);
        return ec == std::errc{} && ptr == end;
    };
    const auto isStrictPositiveInteger = [](const std::string& value) {
        if (value.empty()) {
            return false;
        }
        int parsed = 0;
        const char* begin = value.data();
        const char* end = begin + value.size();
        auto [ptr, ec] = std::from_chars(begin, end, parsed);
        return ec == std::errc{} && ptr == end && parsed > 0;
    };

    // Validate regex patterns
    try {
        std::regex re(lineParsePattern);
    } catch (const std::regex_error& e) {
        errors.push_back("Invalid regex pattern for 'lineParsePattern': " + std::string(e.what()));
    }

    if (logEntryStartPattern) {
        try {
            std::regex re(*logEntryStartPattern);
        } catch (const std::regex_error& e) {
            errors.push_back("Invalid regex pattern for 'logEntryStartPattern': " + std::string(e.what()));
        }
    }

    // Validate field mappings
    if (fieldMappings.empty()) {
        errors.push_back("'fieldMappings' cannot be empty for parsing to work.");
    }
    for (const auto& fm : fieldMappings) {
        if (!fm.groupIndex) {
            errors.push_back("FieldMapping is missing required 'groupIndex'.");
        }
    }

    // Validate filter rules
    for (const auto& fr : filterRules) {
        if (fr.field == LogEntryField::UNKNOWN) {
            errors.push_back("FilterRule has an unrecognized field.");
        }

        // Validate value type for numeric operators
        if (fr.op == filter::FilterOperator::GREATER_THAN || fr.op == filter::FilterOperator::LESS_THAN ||
            fr.op == filter::FilterOperator::GREATER_THAN_OR_EQUAL || fr.op == filter::FilterOperator::LESS_THAN_OR_EQUAL ||
            fr.op == filter::FilterOperator::EQUALS || fr.op == filter::FilterOperator::NOT_EQUALS) // Also applies to EQUALS and NOT_EQUALS for numeric fields
        {
            if (fr.field == LogEntryField::ID || fr.field == LogEntryField::LINE_NUMBER ||
                fr.field == LogEntryField::THREAD_ID)
            {
                if (!isStrictInteger(fr.value)) {
                    errors.push_back("FilterRule operator for field '" + Utils::logEntryFieldToString(fr.field) +
                                     "' requires a numeric value, but got '" + fr.value + "'.");
                }
            }
        }

        if (fr.op == filter::FilterOperator::REGEX) {
            try {
                std::regex re(fr.value);
            } catch (const std::regex_error& e) {
                errors.push_back("Invalid regex pattern in FilterRule: " + std::string(e.what()));
            }
        }
    }

    // Validate statistic configurations
    for (const auto& sc : statisticConfigs) {
        if (sc.type == StatisticType::UNKNOWN) {
            errors.push_back("StatisticConfig has an unrecognized type.");
            continue;
        }

        std::string typeStr = Utils::statisticTypeToString(sc.type);
        auto it_top_n = sc.params.find(std::string(config_keys::TOP_N));
        bool has_top_n = (it_top_n != sc.params.end());
        
        auto it_target_field = sc.params.find(std::string(config_keys::TARGET_FIELD));
        bool has_target_field = (it_target_field != sc.params.end());

        if (sc.type == StatisticType::TOP_MESSAGES || sc.type == StatisticType::TOP_N_FIELD_VALUES) {
            if (!has_top_n) {
                errors.push_back("Statistic '" + typeStr + "' requires a 'top_n' parameter.");
            } else {
                if (!isStrictPositiveInteger(it_top_n->second)) {
                    errors.push_back("Parameter 'top_n' for statistic '" + typeStr + "' must be a positive integer.");
                }
            }
        }

        if (sc.type == StatisticType::FIELD_VALUE_COUNT || sc.type == StatisticType::TOP_N_FIELD_VALUES) {
            if (!has_target_field) {
                errors.push_back("Statistic '" + typeStr + "' requires a 'target_field' parameter.");
            } else {
                const std::string& targetField = it_target_field->second;
                const auto normalizedTargetField = normalizeTargetFieldName(targetField);
                if (!normalizedTargetField) {
                    errors.push_back("Invalid 'target_field' value '" + targetField + "' for statistic '" + typeStr + "'.");
                } else if (*normalizedTargetField == "customFields") {
                    auto it_custom_field_key = sc.params.find(std::string(config_keys::CUSTOM_FIELD_KEY));
                    if (it_custom_field_key == sc.params.end()) {
                        errors.push_back("Statistic '" + typeStr + "' with target_field 'customFields' requires a 'custom_field_key' parameter.");
                    } else if (it_custom_field_key->second.empty()) {
                        errors.push_back("'custom_field_key' parameter cannot be empty.");
                    }
                }
            }
        }
    }

    return errors;
}
