#include "filter/Expression.h"
#include "filter/EnumStringConversions.h" // For new enum to string conversions
#include "utils/Core.h"
#include "utils/Time.h" // For datetime parsing
#include <regex>
#include <chrono>   // For std::chrono::system_clock::time_point
#include <limits>   // For std::numeric_limits
#include <cmath>    // For std::abs with doubles
#include <stdexcept> // For std::stod, std::stoll exceptions

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
        if (entry.id != std::numeric_limits<size_t>::max()) {
            return std::to_string(entry.id);
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
        if (entry.sourceLineNumber != 0) { // Assuming 0 means not set
            return std::to_string(entry.sourceLineNumber);
        }
        return std::nullopt;
    }
    if (cond.field == LogEntryField::THREAD_ID) {
         auto it = entry.customFields.find("thread_id");
            if (it != entry.customFields.end()) {
                return it->second;
            }
        return std::nullopt;
    }
    if (cond.field == LogEntryField::MESSAGE) return entry.message;
    if (cond.field == LogEntryField::MODULE) {
        auto it = entry.customFields.find("module"); // Assuming 'module' is stored as a custom field
        if (it != entry.customFields.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    if (cond.field == LogEntryField::HOST) {
        auto it = entry.customFields.find("host"); // Assuming 'host' is stored as a custom field
        if (it != entry.customFields.end()) {
            return it->second;
        }
        return std::nullopt;
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
            } catch (const std::exception&) {
                return false; // Conversion to long long failed
            }
        }
        case FilterValueType::DOUBLE: {
            try {
                double fieldNum = std::stod(fieldValue);
                double condNum = std::stod(condValue);
                const double EPSILON = 1e-9; // For floating point comparison
                switch (cond.op) {
                    case FilterOperator::EQUALS: return std::abs(fieldNum - condNum) < EPSILON;
                    case FilterOperator::NOT_EQUALS: return std::abs(fieldNum - condNum) >= EPSILON;
                    case FilterOperator::GREATER_THAN: return fieldNum > condNum;
                    case FilterOperator::LESS_THAN: return fieldNum < condNum;
                    case FilterOperator::GREATER_THAN_OR_EQUAL: return fieldNum >= condNum;
                    case FilterOperator::LESS_THAN_OR_EQUAL: return fieldNum <= condNum;
                    default: return false; // Invalid operator for DOUBLE type
                }
            } catch (const std::exception&) {
                return false; // Conversion to double failed
            }
        }
        case FilterValueType::BOOL: {
            std::optional<bool> fieldBool = stringToBool(fieldValue);
            std::optional<bool> condBool = stringToBool(condValue);
            if (!fieldBool.has_value() || !condBool.has_value()) {
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
        case FilterValueType::STRING:
        default: // Default to STRING comparison if type is UNKNOWN or not specified
            switch (cond.op) {
                case FilterOperator::EQUALS:
                    return cond.caseSensitive ? (fieldValue == condValue) : Utils::caseInsensitiveEquals(fieldValue, condValue);
                case FilterOperator::NOT_EQUALS:
                    return cond.caseSensitive ? (fieldValue != condValue) : !Utils::caseInsensitiveEquals(fieldValue, condValue);
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
                case FilterOperator::STARTS_WITH:
                    if (cond.caseSensitive) {
                        return fieldValue.starts_with(condValue);
                    } else {
                        std::string lowerFieldValue = Utils::toLower(fieldValue);
                        std::string lowerCondValue = Utils::toLower(condValue);
                        return lowerFieldValue.starts_with(lowerCondValue);
                    }
                case FilterOperator::ENDS_WITH:
                    if (cond.caseSensitive) {
                        return fieldValue.ends_with(condValue);
                    } else {
                        std::string lowerFieldValue = Utils::toLower(fieldValue);
                        std::string lowerCondValue = Utils::toLower(condValue);
                        return lowerFieldValue.ends_with(lowerCondValue);
                    }
                case FilterOperator::REGEX_MATCH:
                    try {
                        auto flags = cond.caseSensitive ? std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase;
                        std::regex re(condValue, flags);
                        return std::regex_search(fieldValue, re);
                    } catch (const std::regex_error&) {
                        return false;
                    }
                default:
                    return false; // Invalid operator for STRING type
            }
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
