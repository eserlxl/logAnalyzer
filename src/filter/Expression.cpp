#include "filter/Expression.h"
#include "filter/EnumStringConversions.h" // For new enum to string conversions
#include "utils/Core.h"
#include "utils/Time.h" // For datetime parsing
#include "utils/String.h" // For string utility functions
#include "utils/Version.h" // For SemanticVersion parsing and comparison
#include "utils/IpAddress.h" // For IpAddress parsing and comparison
#include <regex>
#include <chrono>   // For std::chrono::system_point
#include <limits>   // For std::numeric_limits
#include <cmath>    // For std::abs with doubles
#include <stdexcept> // For std::stod, std::stoll exceptions
#include <iostream> // For temporary logging to cerr

namespace { // Unnamed namespace for internal helper functions

// Helper to convert string to bool
std::optional<bool> stringToBool(const std::string& s) {
    std::string lowerS = Utils::toLower(s);
    if (lowerS == "true" || lowerS == "1") return true;
    if (lowerS == "false" || lowerS == "0") return false;
    return std::nullopt;
}

// --- Evaluation helpers for FilterExpression ---

std::optional<std::string> getFieldValue(const LogEntry& entry, const FilterCondition& cond) {
    if (cond.field == LogEntryField::ID) {
        if (entry.id.has_value()) {
            return std::to_string(entry.id.value());
        }
        return std::nullopt;
    }
    if (cond.field == LogEntryField::TIMESTAMP) {
        if (entry.timestamp.has_value()) {
            return Utils::formatTimestamp(entry.timestamp.value());
        }
        return std::nullopt;
    }
    if (cond.field == LogEntryField::LEVEL) return Utils::logLevelToString(entry.level);
    if (cond.field == LogEntryField::SOURCE_FILE) return entry.sourceFile;
    if (cond.field == LogEntryField::LINE_NUMBER) {
        if (entry.sourceLineNumber.has_value()) {
            return std::to_string(entry.sourceLineNumber.value());
        }
        return std::nullopt;
    }
    if (cond.field == LogEntryField::THREAD_ID) {
        return entry.threadId; // Now a direct optional member
    }
    if (cond.field == LogEntryField::MESSAGE) return entry.message;
    if (cond.field == LogEntryField::MODULE) {
        return entry.module; // Now a direct optional member
    }
    if (cond.field == LogEntryField::HOST) {
        return entry.host; // Now a direct optional member
    }
    if (cond.field == LogEntryField::CUSTOM) {
        if (cond.customField) {
            auto it = entry.customFields.find(*cond.customField);
            if (it != entry.customFields.end()) {
                return it->second;
            }
        }
        return std::nullopt;
    }
    // For STRUCTURED_FIELD, we might need a more complex lookup
    // For now, UNKNOWN and STRUCTURED_FIELD return nullopt
    return std::nullopt;
}

bool evaluateCondition(const FilterCondition& cond, const LogEntry& entry) {
    // Handle IS_PRESENT and IS_ABSENT operators first, as they don't require a value for comparison
    auto fieldValueOpt = getFieldValue(entry, cond);

    if (cond.op == FilterOperator::IS_PRESENT) {
        return fieldValueOpt.has_value();
    }
    if (cond.op == FilterOperator::IS_ABSENT) {
        return !fieldValueOpt.has_value();
    }

    // For all other operators, if the field value is not present, the condition cannot be met
    if (!fieldValueOpt) {
        return false;
    }
    const std::string& fieldValue = *fieldValueOpt;
    const std::string& condValue = cond.value;

    switch (cond.valueType) {
        case FilterValueType::INT: {
            try {
                long long fieldNum = std::stoll(fieldValue);
                long long condNum = std::stoll(condValue);
                switch (cond.op) {
                    case FilterOperator::EQUALS: return fieldNum == condNum;
                    case FilterOperator::NOT_EQUALS: return fieldNum != condNum;
                    case FilterOperator::GREATER_THAN: return fieldNum > condNum;
                    case FilterOperator::LESS_THAN: return fieldNum < condNum;
                    case FilterOperator::GREATER_THAN_OR_EQUAL: return fieldNum >= condNum;
                    case FilterOperator::LESS_THAN_OR_EQUAL: return fieldNum <= condNum;
                    default: return false; // Invalid operator for INT type
                }
            } catch (const std::exception& e) {
                std::cerr << "Warning: Failed to convert INT field value '" << fieldValue << "' or condition value '" << condValue << "' to number: " << e.what() << std::endl;
                return false; // Conversion to long long failed
            }
        }
        case FilterValueType::DOUBLE: {
            try {
                double fieldNum = std::stod(fieldValue);
                double condNum = std::stod(condValue);
                const double DOUBLE_COMPARISON_EPSILON = 1e-9; // For floating point comparison
                switch (cond.op) {
                    case FilterOperator::EQUALS: return std::abs(fieldNum - condNum) < DOUBLE_COMPARISON_EPSILON;
                    case FilterOperator::NOT_EQUALS: return std::abs(fieldNum - condNum) >= DOUBLE_COMPARISON_EPSILON;
                    case FilterOperator::GREATER_THAN: return fieldNum > condNum;
                    case FilterOperator::LESS_THAN: return fieldNum < condNum;
                    case FilterOperator::GREATER_THAN_OR_EQUAL: return fieldNum >= condNum;
                    case FilterOperator::LESS_THAN_OR_EQUAL: return fieldNum <= condNum;
                    default: return false; // Invalid operator for DOUBLE type
                }
            } catch (const std::exception& e) {
                std::cerr << "Warning: Failed to convert DOUBLE field value '" << fieldValue << "' or condition value '" << condValue << "' to number: " << e.what() << std::endl;
                return false; // Conversion to double failed
            }
        }
        case FilterValueType::BOOL: {
            std::optional<bool> fieldBool = stringToBool(fieldValue);
            std::optional<bool> condBool = stringToBool(condValue);
            if (!fieldBool.has_value() || !condBool.has_value()) {
                std::cerr << "Warning: Failed to convert BOOL field value '" << fieldValue << "' or condition value '" << condValue << "' to boolean." << std::endl;
                return false; // Cannot convert to boolean
            }
            switch (cond.op) {
                case FilterOperator::EQUALS: return fieldBool.value() == condBool.value();
                case FilterOperator::NOT_EQUALS: return fieldBool.value() != condBool.value();
                default: return false; // Invalid operator for BOOL type
            }
        }
        case FilterValueType::DATETIME: {
            auto fieldTimeResult = Utils::parseTime(fieldValue);
            auto condTimeResult = Utils::parseTime(condValue);
            
            if (!fieldTimeResult.has_value() || !condTimeResult.has_value()) {
                std::cerr << "Warning: Failed to parse DATETIME field value '" << fieldValue << "' or condition value '" << condValue << "'." << std::endl;
                return false; // Date/time parsing failed
            }
            
            auto fieldTime = fieldTimeResult.value();
            auto condTime = condTimeResult.value();

            switch (cond.op) {
                case FilterOperator::EQUALS: return fieldTime == condTime;
                case FilterOperator::NOT_EQUALS: return fieldTime != condTime;
                case FilterOperator::GREATER_THAN: return fieldTime > condTime;
                case FilterOperator::LESS_THAN: return fieldTime < condTime;
                case FilterOperator::GREATER_THAN_OR_EQUAL: return fieldTime >= condTime;
                case FilterOperator::LESS_THAN_OR_EQUAL: return fieldTime <= condTime;
                default: return false; // Invalid operator for DATETIME type
            }
        }
        case FilterValueType::VERSION: {
            auto fieldVersion = Utils::parseSemanticVersion(fieldValue);
            auto condVersion = Utils::parseSemanticVersion(condValue);

            if (!fieldVersion.has_value() || !condVersion.has_value()) {
                throw std::runtime_error("Filter error: Failed to parse VERSION field value '" + fieldValue + "' or condition value '" + condValue + "'.");
            }
            
            switch (cond.op) {
                case FilterOperator::EQUALS: return fieldVersion.value() == condVersion.value();
                case FilterOperator::NOT_EQUALS: return fieldVersion.value() != condVersion.value();
                case FilterOperator::GREATER_THAN: return fieldVersion.value() > condVersion.value();
                case FilterOperator::LESS_THAN: return fieldVersion.value() < condVersion.value();
                case FilterOperator::GREATER_THAN_OR_EQUAL: return fieldVersion.value() >= condVersion.value();
                case FilterOperator::LESS_THAN_OR_EQUAL: return fieldVersion.value() <= condVersion.value();
                default: return false; // Invalid operator for VERSION type
            }
        }
        case FilterValueType::IP_ADDRESS: {
            auto fieldIp = Utils::parseIpAddress(fieldValue);
            auto condIp = Utils::parseIpAddress(condValue);

            if (!fieldIp.has_value() || !condIp.has_value()) {
                throw std::runtime_error("Filter error: Failed to parse IP_ADDRESS field value \'" + fieldValue + "\' or condition value \'" + condValue + "\'.");
            }

            switch (cond.op) {
                case FilterOperator::EQUALS: return fieldIp.value() == condIp.value();
                case FilterOperator::NOT_EQUALS: return fieldIp.value() != condIp.value();
                case FilterOperator::GREATER_THAN: return fieldIp.value() > condIp.value();
                case FilterOperator::LESS_THAN: return fieldIp.value() < condIp.value();
                case FilterOperator::GREATER_THAN_OR_EQUAL: return fieldIp.value() >= condIp.value();
                case FilterOperator::LESS_THAN_OR_EQUAL: return fieldIp.value() <= condIp.value();
                default: return false; // Invalid operator for IP_ADDRESS type
            }
        }
        case FilterValueType::STRING:
            // Explicitly handle STRING type.
            switch (cond.op) {
                case FilterOperator::EQUALS:
                    return cond.caseSensitive ? (fieldValue == condValue) : Utils::caseInsensitiveEquals(fieldValue, condValue);
                case FilterOperator::NOT_EQUALS:
                    return cond.caseSensitive ? (fieldValue != condValue) : !Utils::caseInsensitiveEquals(fieldValue, condValue);
                case FilterOperator::EQUALS_I: // Case-insensitive EQUALS
                    return Utils::caseInsensitiveEquals(fieldValue, condValue);
                case FilterOperator::NOT_EQUALS_I: // Case-insensitive NOT_EQUALS
                    return !Utils::caseInsensitiveEquals(fieldValue, condValue);

                case FilterOperator::CONTAINS:
                    if (cond.caseSensitive) {
                        return fieldValue.find(condValue) != std::string::npos;
                    } else {
                        return Utils::caseInsensitiveSearch(fieldValue, condValue);
                    }
                case FilterOperator::NOT_CONTAINS:
                    if (cond.caseSensitive) {
                        return fieldValue.find(condValue) == std::string::npos;
                    } else {
                        return !Utils::caseInsensitiveSearch(fieldValue, condValue);
                    }
                case FilterOperator::CONTAINS_I: // Case-insensitive CONTAINS
                    return Utils::caseInsensitiveSearch(fieldValue, condValue);
                case FilterOperator::NOT_CONTAINS_I: // Case-insensitive NOT_CONTAINS
                    return !Utils::caseInsensitiveSearch(fieldValue, condValue);

                case FilterOperator::STARTS_WITH:
                    return cond.caseSensitive ? fieldValue.starts_with(condValue) : Utils::caseInsensitiveStarts(fieldValue, condValue);
                case FilterOperator::ENDS_WITH:
                    return cond.caseSensitive ? fieldValue.ends_with(condValue) : Utils::caseInsensitiveEnds(fieldValue, condValue);
                case FilterOperator::STARTS_WITH_I: // Case-insensitive STARTS_WITH
                    return Utils::caseInsensitiveStarts(fieldValue, condValue);
                case FilterOperator::ENDS_WITH_I: // Case-insensitive ENDS_WITH
                    return Utils::caseInsensitiveEnds(fieldValue, condValue);

                case FilterOperator::IN: { // Set-based IN
                    try {
                        auto jsonArray = nlohmann::json::parse(condValue);
                        if (!jsonArray.is_array()) {
                            std::cerr << "Warning: FilterOperator::IN expects a JSON array, got something else for value '" << condValue << "'." << std::endl;
                            return false; // Expected an array
                        }
                        for (const auto& item : jsonArray) {
                            if (item.is_string()) { // Only compare strings
                                if (cond.caseSensitive ? (fieldValue == item.get<std::string>()) : Utils::caseInsensitiveEquals(fieldValue, item.get<std::string>())) {
                                    return true; // Found a match
                                }
                            }
                        } // No match found in the set
                        return false;
                    } catch (const nlohmann::json::parse_error& e) {
                        std::cerr << "Warning: Failed to parse JSON for FilterOperator::IN condition value '" << condValue << "': " << e.what() << std::endl;
                        return false; // Invalid JSON format
                    }
                }
                case FilterOperator::NOT_IN: { // Set-based NOT_IN
                    try {
                        auto jsonArray = nlohmann::json::parse(condValue);
                        if (!jsonArray.is_array()) {
                            std::cerr << "Warning: FilterOperator::NOT_IN expects a JSON array, got something else for value '" << condValue << "'." << std::endl;
                            return false; // Expected an array
                        }
                        for (const auto& item : jsonArray) {
                            if (item.is_string()) { // Only compare strings
                                if (cond.caseSensitive ? (fieldValue == item.get<std::string>()) : Utils::caseInsensitiveEquals(fieldValue, item.get<std::string>())) {
                                    return false; // Found a match, so NOT_IN condition fails
                                }
                            }
                        }
                        return true; // No match found in the set, so NOT_IN condition passes
                    } catch (const nlohmann::json::parse_error& e) {
                        std::cerr << "Warning: Failed to parse JSON for FilterOperator::NOT_IN condition value '" << condValue << "': " << e.what() << std::endl;
                        return false; // Invalid JSON format
                    }
                }
                
                case FilterOperator::REGEX_MATCH:
                    try {
                        auto flags = cond.caseSensitive ? std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase;
                        std::regex re(condValue, flags);
                        return std::regex_search(fieldValue, re);
                    } catch (const std::regex_error& e) {
                        std::cerr << "Warning: Invalid regex pattern '" << condValue << "': " << e.what() << std::endl;
                        return false;
                    }
                default:
                    return false; // Invalid operator for STRING type
            }
        
        case FilterValueType::AUTO:
        case FilterValueType::UNKNOWN: // Explicitly handle AUTO and UNKNOWN by falling through to STRING logic
            // Fallthrough to STRING logic
            [[fallthrough]];

        default: // Catches any FilterValueType not explicitly handled above (INT, DOUBLE, etc., or new types)
            // This ensures truly unhandled types throw an error as recommended.
            std::cerr << "Error: Unhandled FilterValueType detected. This should not happen: " << static_cast<int>(cond.valueType) << std::endl;
            throw std::logic_error("Unhandled FilterValueType encountered.");
    }
}

} // Unnamed namespace

bool FilterExpression::evaluate(const LogEntry& entry) const {
    bool result;
    switch (type_) {
        case ExpressionType::EMPTY:
            result = true; // An empty filter matches everything
            break;
        case ExpressionType::CONDITION:
            result = evaluateCondition(*condition_, entry);
            break;
        case ExpressionType::LOGICAL:
            switch (*logicalOperator_) {
                case FilterLogicalOperator::AND:
                    result = true;
                    for (const auto& expr : expressions_) {
                        if (!expr.evaluate(entry)) {
                            result = false;
                            break; // Short-circuit
                        }
                    }
                    break;
                case FilterLogicalOperator::OR:
                    result = false;
                    for (const auto& expr : expressions_) {
                        if (expr.evaluate(entry)) {
                            result = true;
                            break; // Short-circuit
                        }
                    }
                    break;
                // No default needed as FilterLogicalOperator only has AND/OR now.
            }
            break;
        default:
            result = false; // Should not be reached
            break;
    }

    if (negated_) {
        return !result;
    }
    return result;
}
