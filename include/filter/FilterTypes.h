#ifndef FILTER_TYPES_H
#define FILTER_TYPES_H

// Enum for sorting criteria.
enum class SortBy {
    TIMESTAMP,
    LEVEL,
    MESSAGE
};

// Enum for sorting order.
enum class SortOrder {
    ASCENDING,
    DESCENDING
};

enum class FilterOperator {
    EQUALS,             // ==
    NOT_EQUALS,         // !=
    CONTAINS,           // substring search
    NOT_CONTAINS,       // not substring search
    STARTS_WITH,        // prefix search
    ENDS_WITH,          // suffix search
    REGEX_MATCH,        // regex match
    LESS_THAN,          // < (numeric/datetime)
    GREATER_THAN,       // > (numeric/datetime)
    LESS_THAN_OR_EQUAL, // <= (numeric/datetime)
    GREATER_THAN_OR_EQUAL, // >= (numeric/datetime)
    UNKNOWN
};

// New: Enum for logical operators to combine filter expressions
enum class FilterLogicalOperator {
    AND,
    OR,
    NOT, // Unary operator, applied to the next expression
    UNKNOWN
};

// New: Enum to indicate how a filter value should be interpreted
enum class FilterValueType {
    STRING,
    NUMERIC,
    DATETIME,
    UNKNOWN // Added UNKNOWN enumerator
};

#endif // FILTER_TYPES_H
