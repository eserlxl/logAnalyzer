// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include <cstdint> // For uint8_t

namespace filter {

// Enum for criteria used to sort log entries.
enum class SortBy : uint8_t {
    TIMESTAMP, // Sort by the timestamp of the log entry.
    LEVEL,     // Sort by the log level (e.g., INFO, WARN, ERROR).
    MESSAGE,   // Sort by the content of the log message.
    SOURCE,    // Sort by the source of the log (e.g., file name, component name).
    THREAD_ID, // Sort by the thread identifier, if available.
    MODULE,    // Sort by the module/component name, if available.
    HOST,      // Sort by the originating host, if available.
    ID,        // Sort by the numeric entry id, if available.
    LINE_NUMBER // Sort by the source line number, if available.
};

// Enum for the order in which sorted results should be presented.
enum class SortOrder : uint8_t {
    ASCENDING,  // Sort in ascending order (e.g., A-Z, 0-9, oldest to newest).
    DESCENDING  // Sort in descending order (e.g., Z-A, 9-0, newest to oldest).
};

// Enum for operators used in filter conditions.
enum class FilterOperator : uint8_t {
    // Relational
    EQUALS,
    NOT_EQUALS,
    LESS_THAN,
    GREATER_THAN,
    LESS_THAN_OR_EQUAL,
    GREATER_THAN_OR_EQUAL,

    // String
    CONTAINS,
    NOT_CONTAINS,
    STARTS_WITH,
    ENDS_WITH,
    REGEX,

    // Case-Insensitive String (New)
    EQUALS_I,
    NOT_EQUALS_I,
    CONTAINS_I,
    NOT_CONTAINS_I,
    STARTS_WITH_I,
    ENDS_WITH_I,

    // Set-based (New)
    IN,
    NOT_IN,

    // Presence
    IS_PRESENT,
    IS_ABSENT,
    IS_NULL,
    IS_NOT_NULL
};

// Enum for logical operators used to combine multiple filter expressions.
enum class FilterLogicalOperator : uint8_t {
    AND, // Combines two expressions; both must be true for the combined expression to be true.
    OR,  // Combines two expressions; at least one must be true for the combined expression to be true.
};

// Enum to indicate how a filter value should be interpreted.
enum class FilterValueType : uint8_t {
    UNKNOWN = 0,  // Default or unhandled type, implies string comparison
    STRING = 1,
    INT = 2,
    DOUBLE = 3,
    FLOAT = 3,    // Alias for DOUBLE
    BOOL = 4,
    DATETIME = 5,
    AUTO = 6,     // Infer the type from the value's syntax (New).
    VERSION = 7,  // Treat value as a semantic version string (New).
    IP_ADDRESS = 8, // Treat value as an IPv4/IPv6 address (New).
    REGEX = 9,     // Treat value as a regular expression pattern.
    LOG_LEVEL = 11  // Treat value as a log level (e.g., INFO, WARNING).
};

} // namespace filter
