// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#pragma once

#include "gtest/gtest.h"
#include "config/CLIConfig.h"
#include "core/Log/Types.h"
#include "core/Error.h"
#include "utils/Time.h"
#include "export/Core.h"
#include "config/Settings.h"
#include <fstream>
#include <filesystem>
#include <chrono>

// Test fixture for CLIConfig tests
class CLIConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a dummy file for tests that need a file path
        std::ofstream dummy_file("dummy_log_file.log");
        if (dummy_file) {
            dummy_file << "dummy content\n";
            dummy_file.close();
        }
    }

    void TearDown() override {
        std::filesystem::remove("dummy_log_file.log");
        std::filesystem::remove("dummy_log_file2.log");
        std::filesystem::remove("output.txt");
    }

    // Helper function to call parseCLI with a vector of C-style strings
    ErrorCode::Result<std::pair<LogAnalyzerSettings, CLIConfig::CLIOptions>> parse(std::vector<const char*> args) {
        return CLIConfig::parseCLI(args.size(), args.data());
    }
};
