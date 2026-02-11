// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef FILTER_JSON_FIXTURE_H
#define FILTER_JSON_FIXTURE_H

#include <gtest/gtest.h>
#include <memory> // For std::unique_ptr
#include "filter/condition.h" // For FilterCondition, etc.
#include "core/log/types.h" // For LogEntryField

class FilterJsonTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for tests
    }

    void TearDown() override {
        // Common teardown for tests
    }
};

#endif // FILTER_JSON_FIXTURE_H
