// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2024 Eser KUBALI

#pragma once

#include <gtest/gtest.h>
#include <fstream>
#include <vector>
#include <string>
#include <optional>
#include "config/cli.h"
#include "config/core.h"
#include "config/settings.h"

class CLIConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a dummy file for tests that need a file to exist.
        std::ofstream dummy_file("dummy_log_file.log");
        dummy_file << "dummy content\n";
        dummy_file.close();
    }

    void TearDown() override {
        // Clean up dummy files
        remove("dummy_log_file.log");
        remove("dummy_log_file2.log");
    }

    auto parse(std::vector<std::string> args) {
        std::vector<const char*> argv;
        for (const auto& arg : args) {
            argv.push_back(arg.c_str());
        }
        return CLIConfig::parseCLI(static_cast<int>(argv.size()), argv.data());
    }
};
