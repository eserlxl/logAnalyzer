// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer/Core.h"
#include "utils/UtilsCore.h"
#include "utils/String.h"
#include "config/Settings.h"
#include "config/CLIConfig.h" // Added for CLIConfig
#include "core/Error.h" // New: For Error struct and Result alias
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <unistd.h> // For isatty

using json = nlohmann::json;

int main(int argc, char *argv[]) {
    auto expectedConfig = CLIConfig::parseCLI(argc, argv);
    if (!expectedConfig) {
        // Print the error message and exit.
        std::cerr << expectedConfig.error().message << std::endl;
        return 1;
    }
    const auto& [analyzerSettings, cliOptions] = expectedConfig.value();

    LogAnalyzer analyzer(analyzerSettings); // Construct with settings



    std::ofstream outFile;
    std::ostream *outputStream = &std::cout;
    if (!cliOptions.outputPath.empty()) {
        outFile.open(cliOptions.outputPath);
        if (!outFile.is_open()) {
            std::cerr << "Error: Could not open output file: " << cliOptions.outputPath << std::endl;
            return 1;
        }
        outputStream = &outFile;
    }

    bool useColors = (cliOptions.colorOption == CLIConfig::ColorOption::ALWAYS) ||
                     (cliOptions.colorOption == CLIConfig::ColorOption::AUTO && isatty(fileno(stdout)) && cliOptions.outputPath.empty());

    // NOTE: The new --expression filter is not yet implemented. This will be part of the next stage.
    auto rootFilter = std::make_shared<CompositeFilter>(CompositeFilter::Logic::AND);

    // Inclusion filters
    auto inclusionFilters = std::make_shared<CompositeFilter>(cliOptions.filterLogic.value_or(CompositeFilter::Logic::AND));
    if (cliOptions.minLogLevel.has_value()) inclusionFilters->add(std::make_shared<MinLevelFilter>(*cliOptions.minLogLevel));
    if (!cliOptions.filterLevels.empty()) {
        auto levelSet = std::make_shared<CompositeFilter>(CompositeFilter::Logic::OR);
        for (auto l : cliOptions.filterLevels) levelSet->add(std::make_shared<LevelFilter>(l));
        inclusionFilters->add(levelSet);
    }
    if (!cliOptions.filterKeywords.empty()) {
        auto keywordSet = std::make_shared<CompositeFilter>(cliOptions.filterLogic.value_or(CompositeFilter::Logic::AND));
        for (const auto& keyword : cliOptions.filterKeywords) keywordSet->add(std::make_shared<KeywordFilter>(keyword, cliOptions.keywordCaseSensitive));
        inclusionFilters->add(keywordSet);
    }
    if (!cliOptions.regexPatterns.empty()) {
        auto regexSet = std::make_shared<CompositeFilter>(cliOptions.filterLogic.value_or(CompositeFilter::Logic::AND));
        for (const auto& regex : cliOptions.regexPatterns) {
            auto regexFilterResult = RegexFilter::create(regex);
            if (!regexFilterResult.has_value()) {
                std::cerr << "Error: Invalid regex pattern for inclusion filter: " << regexFilterResult.error().toString() << std::endl;
                return 1;
            }
            regexSet->add(regexFilterResult.value());
        }
        inclusionFilters->add(regexSet);
    }
    rootFilter->add(inclusionFilters);

    // Exclusion filters (always ANDed)
    if (!cliOptions.excludeKeywords.empty()) {
        auto exclusionSet = std::make_shared<CompositeFilter>(CompositeFilter::Logic::OR);
        for (const auto& keyword : cliOptions.excludeKeywords) exclusionSet->add(std::make_shared<KeywordFilter>(keyword, cliOptions.keywordCaseSensitive));
        rootFilter->add(std::make_shared<ExclusionFilter>(exclusionSet));
    }
    if (!cliOptions.excludeRegexPatterns.empty()) {
        auto exclusionSet = std::make_shared<CompositeFilter>(CompositeFilter::Logic::OR);
        for (const auto& regex : cliOptions.excludeRegexPatterns) {
            auto regexFilterResult = RegexFilter::create(regex);
            if (!regexFilterResult.has_value()) {
                std::cerr << "Error: Invalid regex pattern for exclusion filter: " << regexFilterResult.error().toString() << std::endl;
                return 1;
            }
            exclusionSet->add(regexFilterResult.value());
        }
        rootFilter->add(std::make_shared<ExclusionFilter>(exclusionSet));
    }

    if (cliOptions.startTime || cliOptions.endTime) {
        rootFilter->add(std::make_shared<TimeRangeFilter>(
            cliOptions.startTime.value_or(std::chrono::system_clock::time_point::min()),
            cliOptions.endTime.value_or(std::chrono::system_clock::time_point::max())
        ));
    }

        if (cliOptions.streamMode) {
        if (cliOptions.outputFormat != "text" && cliOptions.outputFormat != "csv") {
            std::cerr << "Error: Streaming mode only supports 'text' or 'csv' output format." << std::endl;
            return 1;
        }
        auto streamEntryCallback = [&](const LogEntry &entry) {
            if (rootFilter->matches(entry)) {
                 if (cliOptions.outputFormat == "text") {
                    FormattingOptions fmtOptions;
                    fmtOptions.useColor = useColors;
                    fmtOptions.dateTimeFormat = "%Y-%m-%d %H:%M:%S";
                    *outputStream << analyzer.formatEntry(entry, cliOptions.textOutputFormat, fmtOptions) << std::endl;
                 } else { // CSV
                    // TODO: Implement configurable CSV fields from cliOptions.csvFields
                    *outputStream << (entry.timestamp.has_value() ? Utils::formatTimestamp(*entry.timestamp) : "") << cliOptions.csvSeparator
                                  << Utils::logLevelToString(entry.level) << cliOptions.csvSeparator;

                    std::string msg = entry.message;
                    bool needsQuotes = msg.find(cliOptions.csvSeparator) != std::string::npos || msg.find('"') != std::string::npos;
                    if (needsQuotes) {
                        Utils::replaceAll(msg, "\"", "\"\"");
                        *outputStream << "\"" << msg << "\"";
                    } else {
                        *outputStream << msg;
                    }
                    *outputStream << cliOptions.csvSeparator;

                    std::string sourceFile = entry.sourceFile;
                    needsQuotes = sourceFile.find(cliOptions.csvSeparator) != std::string::npos || sourceFile.find('"') != std::string::npos;
                    if (needsQuotes) {
                        Utils::replaceAll(sourceFile, "\"", "\"\"");
                        *outputStream << "\"" << sourceFile << "\"";
                    } else {
                        *outputStream << sourceFile;
                    }
                    *outputStream << std::endl;
                 }
            }
            return true;
        };

        if(auto res = analyzer.analyzeStream(cliOptions.filePaths, streamEntryCallback, cliOptions.parserErrorAction); !res) {
            std::cerr << "Error during stream analysis: " << res.error().toString() << std::endl;
            return 1;
        }

    } else {
        for (const auto& path : cliOptions.filePaths) {
            if(auto res = analyzer.append(path, cliOptions.parserErrorAction); !res) {
                 std::cerr << "Error analyzing file " << path << ": " << res.error().toString() << std::endl;
                 return 1;
            }
        }
        std::vector<LogEntry> filteredEntries;
        for (const auto& entry : analyzer.getEntries()) {
            if (rootFilter->matches(entry)) {
                filteredEntries.push_back(entry);
            }
        }

        // Sorting
        if (cliOptions.sortBy != SortBy::TIMESTAMP || cliOptions.sortOrder != SortOrder::ASCENDING) {
            std::sort(filteredEntries.begin(), filteredEntries.end(), [&](const LogEntry& a, const LogEntry& b) {
                if (cliOptions.sortBy == SortBy::TIMESTAMP) {
                    return cliOptions.sortOrder == SortOrder::ASCENDING ? a.timestamp < b.timestamp : a.timestamp > b.timestamp;
                } else if (cliOptions.sortBy == SortBy::LEVEL) {
                    return cliOptions.sortOrder == SortOrder::ASCENDING ? a.level < b.level : a.level > b.level;
                } else { // MESSAGE
                    return cliOptions.sortOrder == SortOrder::ASCENDING ? a.message < b.message : a.message > b.message;
                }
            });
        }

        if (cliOptions.outputFormat == "text") {
            for(const auto& entry : filteredEntries) {
                FormattingOptions fmtOptions;
                fmtOptions.useColor = useColors;
                fmtOptions.dateTimeFormat = "%Y-%m-%d %H:%M:%S";
                *outputStream << analyzer.formatEntry(entry, cliOptions.textOutputFormat, fmtOptions) << std::endl;
            }
        } else if (cliOptions.outputFormat == "csv") {
            // TODO: Implement configurable CSV fields from cliOptions.csvFields
            *outputStream << "Timestamp" << cliOptions.csvSeparator << "Level" << cliOptions.csvSeparator << "Message" << cliOptions.csvSeparator << "File\n";
            for (const auto& entry : filteredEntries) {
                *outputStream << (entry.timestamp.has_value() ? Utils::formatTimestamp(*entry.timestamp) : "") << cliOptions.csvSeparator
                              << Utils::logLevelToString(entry.level) << cliOptions.csvSeparator;
                std::string msg = entry.message;
                bool needsQuotes = msg.find(cliOptions.csvSeparator) != std::string::npos || msg.find('"') != std::string::npos;
                if (needsQuotes) {
                    Utils::replaceAll(msg, "\"", "\"\"");
                    *outputStream << "\"" << msg << "\"";
                } else {
                    *outputStream << msg;
                }
                *outputStream << cliOptions.csvSeparator;
                std::string sourceFile = entry.sourceFile;
                needsQuotes = sourceFile.find(cliOptions.csvSeparator) != std::string::npos || sourceFile.find('"') != std::string::npos;
                if (needsQuotes) {
                    Utils::replaceAll(sourceFile, "\"", "\"\"");
                    *outputStream << "\"" << sourceFile << "\"";
                } else {
                    *outputStream << sourceFile;
                }
                *outputStream << "\n";
            }
        } else if (cliOptions.outputFormat == "json") {
             json j;
             if (cliOptions.includeSummary) {
                 j["totalEntries"] = filteredEntries.size();
             }
             j["entries"] = json::array();
             for (const auto& entry : filteredEntries) {
                 // TODO: Implement configurable JSON fields (less critical, but good for consistency)
                 j["entries"].push_back(json{
                     {"timestamp", entry.timestamp.has_value() ? json(Utils::formatTimestamp(*entry.timestamp)) : json(json::value_t::null)},
                     {"level", Utils::logLevelToString(entry.level)},
                     {"message", entry.message},
                     {"file", entry.sourceFile}
                 });
             }
             if (cliOptions.prettyPrint) {
                 *outputStream << std::setw(4) << j << std::endl;
             } else {
                 *outputStream << j << std::endl;
             }
        }

        // IStatisticCollector logic
    for (const auto& statName : cliOptions.enabledStatistics) {
        if (statName == "unique_messages") {
            analyzer.addStatisticCollector(std::make_shared<UniqueMessagesCollector>());
        } else if (statName.rfind("top_messages", 0) == 0) {
            int n = cliOptions.topMessagesCount;
            // Allow override from option like top_messages:5
            auto colonPos = statName.find(':');
            if (colonPos != std::string::npos) {
                try {
                    n = std::stoi(statName.substr(colonPos + 1));
                } catch (const std::exception& e) {
                    std::cerr << "Warning: Invalid number for top_messages. Using default " << n << std::endl;
                }
            }
            analyzer.addStatisticCollector(std::make_shared<TopMessagesCollector>(n));
        } else if (statName == "entry_rate") {
            analyzer.addStatisticCollector(std::make_shared<EntryRateCollector>());
        } else {
            std::cerr << "Warning: Unknown statistic '" << statName << "' requested." << std::endl;
        }
    }
    
    // ... processing logs ...

    // After processing all entries, run stats over the *filtered* entries
    for(const auto& entry : filteredEntries) {
        analyzer.processEntryForStatistics(entry);
    }

    if (!cliOptions.enabledStatistics.empty()) {
        *outputStream << "\n--- Statistics ---\n";
        auto reports = analyzer.getAllStatisticReports();
        for (const auto& reportPair : reports) {
            *outputStream << reportPair.second.dump(cliOptions.prettyPrint ? 4 : -1) << std::endl;
        }
    }
}

    return 0;
}
