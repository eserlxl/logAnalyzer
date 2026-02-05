#ifndef FILTER_TYPES_H
#define FILTER_TYPES_H

#include <cstdint> // For uint8_t

// Enum for criteria used to sort log entries.
enum class SortBy : uint8_t {
    TIMESTAMP, // Sort by the timestamp of the log entry.
    LEVEL,     // Sort by the log level (e.g., INFO, WARN, ERROR).
    MESSAGE    // Sort by the content of the log message.
};

// Enum for the order in which sorted results should be presented.
enum class SortOrder : uint8_t {
    ASCENDING,  // Sort in ascending order (e.g., A-Z, 0-9, oldest to newest).
    DESCENDING  // Sort in descending order (e.g., Z-A, 9-0, newest to oldest).
};

// Enum for operators used in filter conditions.
enum class FilterOperator : uint8_t {
    EQUALS,             // Checks if a field's value is exactly equal to the filter value. (e.g., ==)
    NOT_EQUALS,         // Checks if a field's value is not equal to the filter value. (e.g., !=)
    CONTAINS,           // Checks if a field's string value contains the filter value as a substring.
    NOT_CONTAINS,       // Checks if a field's string value does not contain the filter value as a substring.
    STARTS_WITH,        // Checks if a field's string value starts with the filter value (prefix search).
    ENDS_WITH,          // Checks if a field's string value ends with the filter value (suffix search).
    REGEX_MATCH,        // Checks if a field's string value matches a given regular expression.
    LESS_THAN,          // Checks if a field's numeric or datetime value is less than the filter value. (e.g., <)
    GREATER_THAN,       // Checks if a field's numeric or datetime value is greater than the filter value. (e.g., >)
    LESS_THAN_OR_EQUAL, // Checks if a field's numeric or datetime value is less than or equal to the filter value. (e.g., <=)
    GREATER_THAN_OR_EQUAL // Checks if a field's numeric or datetime value is greater than or equal to the filter value. (e.g., >=)
    // TODO: Removed UNKNOWN. Implement explicit error handling or std::optional for parsing invalid operator strings.
};

// Enum for logical operators used to combine multiple filter expressions.
enum class FilterLogicalOperator : uint8_t {
    AND, // Combines two expressions; both must be true for the combined expression to be true.
    OR,  // Combines two expressions; at least one must be true for the combined expression to be true.
    // TODO: FilterLogicalOperator::NOT was removed as it represents a unary operator and should be handled differently (e.g., as a property of FilterCondition/FilterExpression).
    // TODO: Removed UNKNOWN. Implement explicit error handling or std::optional for parsing invalid logical operator strings.
};

// Enum to indicate how a filter value should be interpreted (e.g., for type conversion and comparison).
enum class FilterValueType : uint8_t {
    STRING,   // The filter value should be treated as a string.
    NUMERIC,  // The filter value should be treated as a numeric type (integer or float).
    DATETIME  // The filter value should be treated as a date and/or time.
    // TODO: Removed UNKNOWN. Implement explicit error handling or std::optional for parsing invalid value type strings.
};

#endif // FILTER_TYPES_H
