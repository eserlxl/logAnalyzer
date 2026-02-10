// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include <gtest/gtest.h>
#include "config/settings.h"
#include "export/core.h"

TEST(MergeTest, PartialMergeDoesNotOverwriteDefaults) {
    LogAnalyzerSettings base;
    base.setCaseSensitiveParsing(true); // Non-default
    ExportSettings es;
    es.format = ExportFormat::JSON;
    base.setExportSettings(es);

    LogAnalyzerSettings overlay; // Default constructed (partials)
    // overlay.caseSensitiveParsing is nullopt
    // overlay.exportSettings.format is nullopt

    base.merge(overlay);

    // This assertion SHOULD PASS with the fix
    ASSERT_TRUE(base.caseSensitiveParsing.has_value());
    EXPECT_TRUE(base.caseSensitiveParsing.value()); 
    
    ASSERT_TRUE(base.exportSettings.format.has_value());
    EXPECT_EQ(base.exportSettings.format.value(), ExportFormat::JSON);
}

TEST(MergeTest, CollectionMergeIsAdditive) {
    LogAnalyzerSettings base;
    base.clearFieldMappings();
    base.addFieldMapping(LogEntryField::TIMESTAMP, 1);

    LogAnalyzerSettings overlay;
    overlay.clearFieldMappings();
    overlay.addFieldMapping(LogEntryField::MESSAGE, 2);

    base.merge(overlay);

    // Should be 2, because we append now
    EXPECT_EQ(base.fieldMappings.size(), 2);
}
