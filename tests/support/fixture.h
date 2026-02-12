// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include "gtest/gtest.h"
#include <gmock/gmock.h>
#include "config/core.h"
#include "core/log/types.h"
#include "filter/core.h"
#include "export/core.h"
#include "stats/core.h"
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

using namespace filter;

struct LogAnalyzerConfigTest : public ::testing::Test {};
