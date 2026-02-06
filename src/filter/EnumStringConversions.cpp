// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

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

namespace filter {

// FilterOperator conversions
std::string toString(FilterOperator op) {
    switch (op) {
        // Relational
        case FilterOperator::EQUALS: return "EQUALS";
        case FilterOperator::NOT_EQUALS: return "NOT_EQUALS";
        case FilterOperator::LESS_THAN: return "LESS_THAN";
        case FilterOperator::GREATER_THAN: return "GREATER_THAN";
        case FilterOperator::LESS_THAN_OR_EQUAL: return "LESS_THAN_OR_EQUAL";
        case FilterOperator::GREATER_THAN_OR_EQUAL: return "GREATER_THAN_OR_EQUAL";
        // String
        case FilterOperator::CONTAINS: return "CONTAINS";
        case FilterOperator::NOT_CONTAINS: return "NOT_CONTAINS";
        case FilterOperator::STARTS_WITH: return "STARTS_WITH";
        case FilterOperator::ENDS_WITH: return "ENDS_WITH";
        case FilterOperator::REGEX: return "REGEX";
        // Presence
        case FilterOperator::IS_PRESENT: return "IS_PRESENT";
        case FilterOperator::IS_ABSENT: return "IS_ABSENT";
        case FilterOperator::IS_NULL: return "IS_NULL";
        case FilterOperator::IS_NOT_NULL: return "IS_NOT_NULL";
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
    if (upperOpStr == "REGEX_MATCH" || upperOpStr == "REGEX") return FilterOperator::REGEX;
    if (upperOpStr == "LESS_THAN") return FilterOperator::LESS_THAN;
    if (upperOpStr == "GREATER_THAN") return FilterOperator::GREATER_THAN;
    if (upperOpStr == "LESS_THAN_OR_EQUAL" || upperOpStr == "LTE" || upperOpStr == "LESS_THAN_OR_EQUALS") return FilterOperator::LESS_THAN_OR_EQUAL;
    if (upperOpStr == "GREATER_THAN_OR_EQUAL" || upperOpStr == "GTE" || upperOpStr == "GREATER_THAN_OR_EQUALS") return FilterOperator::GREATER_THAN_OR_EQUAL;
    if (upperOpStr == "IS_PRESENT") return FilterOperator::IS_PRESENT;
    if (upperOpStr == "IS_ABSENT") return FilterOperator::IS_ABSENT;
    if (upperOpStr == "IS_NULL") return FilterOperator::IS_NULL;
    if (upperOpStr == "IS_NOT_NULL") return FilterOperator::IS_NOT_NULL;
    // New case-insensitive operators
    if (upperOpStr == "EQUALS_I") return FilterOperator::EQUALS_I;
    if (upperOpStr == "NOT_EQUALS_I") return FilterOperator::NOT_EQUALS_I;
    if (upperOpStr == "CONTAINS_I") return FilterOperator::CONTAINS_I;
    if (upperOpStr == "NOT_CONTAINS_I") return FilterOperator::NOT_CONTAINS_I;
    if (upperOpStr == "STARTS_WITH_I") return FilterOperator::STARTS_WITH_I;
    if (upperOpStr == "ENDS_WITH_I") return FilterOperator::ENDS_WITH_I;
    // New set-based operators
    if (upperOpStr == "IN") return FilterOperator::IN;
    if (upperOpStr == "NOT_IN") return FilterOperator::NOT_IN;
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
        case FilterValueType::AUTO: return "AUTO";
        case FilterValueType::STRING: return "STRING";
        case FilterValueType::INT: return "INT";
        case FilterValueType::DOUBLE: return "DOUBLE";
        case FilterValueType::BOOL: return "BOOL";
        case FilterValueType::DATETIME: return "DATETIME";
        case FilterValueType::VERSION: return "VERSION";
        case FilterValueType::IP_ADDRESS: return "IP_ADDRESS";
        case FilterValueType::REGEX: return "REGEX";
        case FilterValueType::FLOAT: return "FLOAT";
        case FilterValueType::LOG_LEVEL: return "LOG_LEVEL";
        default: return "UNKNOWN_VALUE_TYPE";
    }
}


std::optional<FilterValueType> fromStringToFilterValueType(const std::string& typeStr) {
    std::string upperTypeStr = toUpper(typeStr);
    if (upperTypeStr == "AUTO") return FilterValueType::AUTO;
    if (upperTypeStr == "STRING") return FilterValueType::STRING;
    if (upperTypeStr == "INT") return FilterValueType::INT;
    if (upperTypeStr == "DOUBLE") return FilterValueType::DOUBLE;
    if (upperTypeStr == "BOOL") return FilterValueType::BOOL;
    if (upperTypeStr == "DATETIME") return FilterValueType::DATETIME;
    if (upperTypeStr == "VERSION") return FilterValueType::VERSION;
    if (upperTypeStr == "IP_ADDRESS") return FilterValueType::IP_ADDRESS;
    if (upperTypeStr == "REGEX") return FilterValueType::REGEX;
    if (upperTypeStr == "FLOAT") return FilterValueType::FLOAT;
    if (upperTypeStr == "LOG_LEVEL") return FilterValueType::LOG_LEVEL;
    return std::nullopt;
}

// SortBy conversions
std::string toString(SortBy sortBy) {
    switch (sortBy) {
        case SortBy::TIMESTAMP: return "TIMESTAMP";
        case SortBy::LEVEL: return "LEVEL";
        case SortBy::MESSAGE: return "MESSAGE";
        case SortBy::SOURCE: return "SOURCE";
        case SortBy::THREAD_ID: return "THREAD_ID";
        default: return "UNKNOWN_SORT_BY";
    }
}

std::optional<SortBy> fromStringToSortBy(const std::string& sortByStr) {
    std::string upperSortByStr = toUpper(sortByStr);
    if (upperSortByStr == "TIMESTAMP") return SortBy::TIMESTAMP;
    if (upperSortByStr == "LEVEL") return SortBy::LEVEL;
    if (upperSortByStr == "MESSAGE") return SortBy::MESSAGE;
    if (upperSortByStr == "SOURCE") return SortBy::SOURCE;
    if (upperSortByStr == "THREAD_ID") return SortBy::THREAD_ID;
    return std::nullopt;
}

// SortOrder conversions
std::string toString(SortOrder sortOrder) {
    switch (sortOrder) {
        case SortOrder::ASCENDING: return "ASCENDING";
        case SortOrder::DESCENDING: return "DESCENDING";
        default: return "UNKNOWN_SORT_ORDER";
    }
}

std::optional<SortOrder> fromStringToSortOrder(const std::string& sortOrderStr) {
    std::string upperSortOrderStr = toUpper(sortOrderStr);
    if (upperSortOrderStr == "ASCENDING" || upperSortOrderStr == "ASC") return SortOrder::ASCENDING;
    if (upperSortOrderStr == "DESCENDING" || upperSortOrderStr == "DESC") return SortOrder::DESCENDING;
    return std::nullopt;
}

} // namespace filter

