#include "filter/Expression.h"
#include "utils/Core.h"
#include <regex>

namespace { // Unnamed namespace for internal helper functions

// --- Evaluation helpers for FilterExpression ---

std::optional<std::string> getFieldValue(const LogEntry& entry, const FilterCondition& cond) {
    switch (cond.field) {
        case LogEntryField::TIMESTAMP:
            if (entry.timestamp.has_value()) {
                return Utils::formatTimestamp(entry.timestamp.value());
            }
            return std::nullopt;
        case LogEntryField::LEVEL: return Utils::logLevelToString(entry.level);
        case LogEntryField::SOURCE_FILE: return entry.sourceFile;
        case LogEntryField::MESSAGE: return entry.message;
        case LogEntryField::THREAD_ID: {
             auto it = entry.customFields.find("thread_id");
                if (it != entry.customFields.end()) {
                    return it->second;
                }
            return std::nullopt;
        }
        case LogEntryField::CUSTOM:
            if (cond.customField) {
                auto it = entry.customFields.find(*cond.customField);
                if (it != entry.customFields.end()) {
                    return it->second;
                }
            }
            return std::nullopt;
        default:
            return std::nullopt;
    }
}

bool evaluateCondition(const FilterCondition& cond, const LogEntry& entry) {
    auto fieldValueOpt = getFieldValue(entry, cond);
    if (!fieldValueOpt) {
        return false; // Field doesn't exist or is not applicable
    }
    const std::string& fieldValue = *fieldValueOpt;
    const std::string& condValue = cond.value;

    if (cond.valueType == FilterValueType::NUMERIC) {
        try {
            double fieldNum = std::stod(fieldValue);
            double condNum = std::stod(condValue);
            switch (cond.op) {
                case FilterOperator::EQUALS: return std::abs(fieldNum - condNum) < 1e-9;
                case FilterOperator::NOT_EQUALS: return std::abs(fieldNum - condNum) >= 1e-9;
                case FilterOperator::GREATER_THAN: return fieldNum > condNum;
                case FilterOperator::LESS_THAN: return fieldNum < condNum;
                case FilterOperator::GREATER_THAN_OR_EQUAL: return fieldNum >= condNum;
                case FilterOperator::LESS_THAN_OR_EQUAL: return fieldNum <= condNum;
                default: return false;
            }
        } catch (const std::exception&) {
            return false; // Conversion to double failed
        }
    }

    // Default to STRING comparison
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
            return false;
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
