// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include "core/LogTypes.h"

namespace filter {

class IFilter {
public:
    virtual ~IFilter() = default;
    virtual bool matches(const LogEntry &entry) const = 0;
};

} // namespace filter
