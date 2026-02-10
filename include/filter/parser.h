// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include "filter/expression.h"
#include "core/error.h"
#include <string>

namespace filter {

/**
 * @brief Parses a human-readable query string into a FilterExpression.
 *
 * This function provides the primary entry point for converting user input
 * into a valid filter expression tree. The query language supports logical
 * operators (AND, OR, NOT), parentheses for grouping, and conditions.
 *
 * Language Syntax Example:
 * `level >= WARNING AND (message contains 'denied' OR source = "auth.cpp")`
 * `NOT custom.user_id is_null`
 *
 * @param query The filter query string.
 * @return A result containing the parsed FilterExpression or an error detailing
 *         the syntax issue.
 */
ErrorCode::Result<FilterExpression> parseQuery(const std::string& query);

} // namespace filter
