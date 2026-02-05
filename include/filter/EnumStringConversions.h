#ifndef FILTER_ENUM_STRING_CONVERSIONS_H
#define FILTER_ENUM_STRING_CONVERSIONS_H

#include <string>
#include <optional>
#include "filter/Types.h" // Include the enums

// to_string functions
std::string toString(FilterOperator op);
std::string toString(FilterLogicalOperator op);
std::string toString(FilterValueType type);

// from_string functions
std::optional<FilterOperator> fromStringToFilterOperator(const std::string& opStr);
std::optional<FilterLogicalOperator> fromStringToFilterLogicalOperator(const std::string& opStr);
std::optional<FilterValueType> fromStringToFilterValueType(const std::string& typeStr);

#endif // FILTER_ENUM_STRING_CONVERSIONS_H
