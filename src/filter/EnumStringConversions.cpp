#include "filter/EnumStringConversions.h"
#include <algorithm> // For std::transform
#include <cctype>    // For std::tolower

namespace { // Anonymous namespace for helper function
    std::string toUpper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return std::toupper(c); });
        return s;
    }
}

// FilterOperator conversions
std::string toString(FilterOperator op) {
    switch (op) {
        case FilterOperator::EQUALS: return "EQUALS";
        case FilterOperator::NOT_EQUALS: return "NOT_EQUALS";
        case FilterOperator::CONTAINS: return "CONTAINS";
        case FilterOperator::NOT_CONTAINS: return "NOT_CONTAINS";
        case FilterOperator::STARTS_WITH: return "STARTS_WITH";
        case FilterOperator::ENDS_WITH: return "ENDS_WITH";
        case FilterOperator::REGEX_MATCH: return "REGEX_MATCH";
        case FilterOperator::LESS_THAN: return "LESS_THAN";
        case FilterOperator::GREATER_THAN: return "GREATER_THAN";
        case FilterOperator::LESS_THAN_OR_EQUAL: return "LESS_THAN_OR_EQUAL";
        case FilterOperator::GREATER_THAN_OR_EQUAL: return "GREATER_THAN_OR_EQUAL";
        case FilterOperator::IS_PRESENT: return "IS_PRESENT";
        case FilterOperator::IS_ABSENT: return "IS_ABSENT";
        // Default case should ideally not be reached if all enums are covered
        default: return "UNKNOWN_OPERATOR"; 
    }
}

std::optional<FilterOperator> fromStringToFilterOperator(const std::string& opStr) {
    std::string upperOpStr = toUpper(opStr);
    if (upperOpStr == "EQUALS") return FilterOperator::EQUALS;
    if (upperOpStr == "NOT_EQUALS") return FilterOperator::NOT_EQUALS;
    if (upperOpStr == "CONTAINS") return FilterOperator::CONTAINS;
    if (upperOpStr == "NOT_CONTAINS") return FilterOperator::NOT_CONTAINS;
    if (upperOpStr == "STARTS_WITH") return FilterOperator::STARTS_WITH;
    if (upperOpStr == "ENDS_WITH") return FilterOperator::ENDS_WITH;
    if (upperOpStr == "REGEX_MATCH") return FilterOperator::REGEX_MATCH;
    if (upperOpStr == "LESS_THAN") return FilterOperator::LESS_THAN;
    if (upperOpStr == "GREATER_THAN") return FilterOperator::GREATER_THAN;
    if (upperOpStr == "LESS_THAN_OR_EQUAL" || upperOpStr == "LTE") return FilterOperator::LESS_THAN_OR_EQUAL;
    if (upperOpStr == "GREATER_THAN_OR_EQUAL" || upperOpStr == "GTE") return FilterOperator::GREATER_THAN_OR_EQUAL;
    if (upperOpStr == "IS_PRESENT") return FilterOperator::IS_PRESENT;
    if (upperOpStr == "IS_ABSENT") return FilterOperator::IS_ABSENT;
    return std::nullopt;
}

// FilterLogicalOperator conversions
std::string toString(FilterLogicalOperator op) {
    switch (op) {
        case FilterLogicalOperator::AND: return "AND";
        case FilterLogicalOperator::OR: return "OR";
        default: return "UNKNOWN_LOGICAL_OPERATOR";
    }
}

std::optional<FilterLogicalOperator> fromStringToFilterLogicalOperator(const std::string& opStr) {
    std::string upperOpStr = toUpper(opStr);
    if (upperOpStr == "AND") return FilterLogicalOperator::AND;
    if (upperOpStr == "OR") return FilterLogicalOperator::OR;
    return std::nullopt;
}

// FilterValueType conversions
std::string toString(FilterValueType type) {
    switch (type) {
        case FilterValueType::STRING: return "STRING";
        case FilterValueType::INT: return "INT";
        case FilterValueType::DOUBLE: return "DOUBLE";
        case FilterValueType::BOOL: return "BOOL";
        case FilterValueType::DATETIME: return "DATETIME";
        default: return "UNKNOWN_VALUE_TYPE";
    }
}

std::optional<FilterValueType> fromStringToFilterValueType(const std::string& typeStr) {
    std::string upperTypeStr = toUpper(typeStr);
    if (upperTypeStr == "STRING") return FilterValueType::STRING;
    if (upperTypeStr == "INT") return FilterValueType::INT;
    if (upperTypeStr == "DOUBLE") return FilterValueType::DOUBLE;
    if (upperTypeStr == "BOOL") return FilterValueType::BOOL;
    if (upperTypeStr == "DATETIME") return FilterValueType::DATETIME;
    return std::nullopt;
}
