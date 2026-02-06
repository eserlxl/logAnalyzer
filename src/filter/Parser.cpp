// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "filter/Parser.h"

namespace filter {

ErrorCode::Result<FilterExpression> parseQuery(const std::string& query) {
    // TODO: Implement the query parser.
    // This is a complex task that will require a proper parsing strategy
    // (e.g., recursive descent). For now, we return a "Not Implemented" error.
    return std::unexpected(ErrorCode::Error(Code::NotImplemented, "Query parser is not yet implemented."));
}

} // namespace filter
