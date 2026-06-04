// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "helper.h"
#include "export/core.h"
#include <sstream>

using json = nlohmann::json;

TEST(ExporterNdjsonTest, EachEntryOnOwnLine) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Alpha"));
    entries.push_back(createLogEntry(2, LogLevel::WARNING, "Beta"));
    entries.push_back(createLogEntry(3, LogLevel::ERROR, "Gamma"));

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::NDJSON;

    exporter.exportLogEntries(ss, entries, settings);

    std::string output = ss.str();
    std::vector<std::string> lines;
    std::istringstream lineStream(output);
    std::string line;
    while (std::getline(lineStream, line)) {
        if (!line.empty()) lines.push_back(line);
    }

    ASSERT_EQ(lines.size(), 3u);
    for (const auto& l : lines) {
        json parsed = json::parse(l);
        ASSERT_TRUE(parsed.is_object());
    }
    ASSERT_EQ(json::parse(lines[0])["MESSAGE"], "Alpha");
    ASSERT_EQ(json::parse(lines[1])["MESSAGE"], "Beta");
    ASSERT_EQ(json::parse(lines[2])["MESSAGE"], "Gamma");
}

TEST(ExporterNdjsonTest, NoArrayWrapper) {
    Exporter exporter;
    std::vector<LogEntry> entries;
    entries.push_back(createLogEntry(1, LogLevel::INFO, "Only"));

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::NDJSON;

    exporter.exportLogEntries(ss, entries, settings);

    std::string output = ss.str();
    // Must not start with '[' (no array wrapper)
    ASSERT_FALSE(output.empty());
    ASSERT_NE(output[0], '[');
    // Single line (plus trailing newline)
    long newlineCount = std::count(output.begin(), output.end(), '\n');
    ASSERT_EQ(newlineCount, 1);
}

TEST(ExporterNdjsonTest, EmptyEntriesProducesNoLines) {
    Exporter exporter;
    std::vector<LogEntry> entries;

    std::stringstream ss;
    ExportSettings settings;
    settings.format = ExportFormat::NDJSON;

    exporter.exportLogEntries(ss, entries, settings);

    ASSERT_TRUE(ss.str().empty());
}
