// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include <string>
#include <optional>
#include "filter/Types.h" // Include the enums

namespace filter {

// to_string functions
std::string toString(SortBy sortBy);
std::string toString(SortOrder sortOrder);
std::string toString(FilterOperator op);
std::string toString(FilterLogicalOperator op);
std::string toString(FilterValueType type);

// from_string functions
std::optional<SortBy> fromStringToSortBy(const std::string& sortByStr);
std::optional<SortOrder> fromStringToSortOrder(const std::string& sortOrderStr);
std::optional<FilterOperator> fromStringToFilterOperator(const std::string& opStr);
std::optional<FilterLogicalOperator> fromStringToFilterLogicalOperator(const std::string& opStr);
std::optional<FilterValueType> fromStringToFilterValueType(const std::string& typeStr);

} // namespace filter
