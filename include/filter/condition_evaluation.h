// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef FILTER_CONDITION_EVALUATION_H
#define FILTER_CONDITION_EVALUATION_H

#include "filter/condition.h"
#include "core/log/types.h"
#include "core/error.h"
#include <optional>
#include <string>

namespace filter {

// Internal helper to evaluate a single condition
// Exposed for splitting implementation from Expression.cpp
ErrorCode::Result<bool> evaluateCondition(const FilterCondition& cond, const LogEntry& entry);

// Helper to get value from a log entry based on condition field
std::optional<std::string> getFieldValue(const LogEntry& entry, const FilterCondition& cond);

} // namespace filter

#endif // FILTER_CONDITION_EVALUATION_H
