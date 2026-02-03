#include "LogAnalyzer.h"
#include "Utils.h"
#include "LogAnalyzerConfig.h"
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
    auto expectedConfig = LogAnalyzerConfig::parseCLI(argc, argv);
    if (!expectedConfig) {
        // Print the error message (which could be help text) and exit.
        std::cerr << expectedConfig.error();
        return 1;
    }
    const auto& config = expectedConfig.value();

    // The rest of the logic uses the config object
    LogAnalyzer analyzer;
    for (const auto &mapping : config.customLogLevelMappings) {
        analyzer.setCustomLogLevelMapping(mapping.first, mapping.second);
    }

    std::ofstream outFile;
    std::ostream *outputStream = &std::cout;
    if (!config.outputPath.empty()) {
        outFile.open(config.outputPath);
        if (!outFile.is_open()) {
            std::cerr << "Error: Could not open output file: " << config.outputPath << std::endl;
            return 1;
        }
        outputStream = &outFile;
    }

    bool useColors = (config.colorOption == LogAnalyzerConfig::ColorOption::ALWAYS) ||
                     (config.colorOption == LogAnalyzerConfig::ColorOption::AUTO && isatty(fileno(stdout)) && config.outputPath.empty());

    // NOTE: The new --expression filter is not yet implemented. This will be part of the next stage.
    auto rootFilter = std::make_shared<CompositeFilter>(CompositeFilter::Logic::AND);

    // Inclusion filters
    auto inclusionFilters = std::make_shared<CompositeFilter>(config.filterLogic);
    if (config.minLogLevel.has_value()) inclusionFilters->add(std::make_shared<MinLevelFilter>(*config.minLogLevel));
    if (!config.filterLevels.empty()) {
        auto levelSet = std::make_shared<CompositeFilter>(CompositeFilter::Logic::OR);
        for (auto l : config.filterLevels) levelSet->add(std::make_shared<LevelFilter>(l));
        inclusionFilters->add(levelSet);
    }
    if (!config.filterKeywords.empty()) {
        auto keywordSet = std::make_shared<CompositeFilter>(config.filterLogic);
        for (const auto& keyword : config.filterKeywords) keywordSet->add(std::make_shared<KeywordFilter>(keyword, config.keywordCaseSensitive));
        inclusionFilters->add(keywordSet);
    }
    if (!config.regexPatterns.empty()) {
        auto regexSet = std::make_shared<CompositeFilter>(config.filterLogic);
        for (const auto& regex : config.regexPatterns) regexSet->add(std::make_shared<RegexFilter>(regex));
        inclusionFilters->add(regexSet);
    }
    rootFilter->add(inclusionFilters);

    // Exclusion filters (always ANDed)
    if (!config.excludeKeywords.empty()) {
        auto exclusionSet = std::make_shared<CompositeFilter>(CompositeFilter::Logic::OR);
        for (const auto& keyword : config.excludeKeywords) exclusionSet->add(std::make_shared<KeywordFilter>(keyword, config.keywordCaseSensitive));
        rootFilter->add(std::make_shared<ExclusionFilter>(exclusionSet));
    }
    if (!config.excludeRegexPatterns.empty()) {
        auto exclusionSet = std::make_shared<CompositeFilter>(CompositeFilter::Logic::OR);
        for (const auto& regex : config.excludeRegexPatterns) exclusionSet->add(std::make_shared<RegexFilter>(regex));
        rootFilter->add(std::make_shared<ExclusionFilter>(exclusionSet));
    }

    if (config.startTime || config.endTime) {
        rootFilter->add(std::make_shared<TimeRangeFilter>(
            config.startTime.value_or(std::chrono::system_clock::time_point::min()),
            config.endTime.value_or(std::chrono::system_clock::time_point::max())
        ));
    }

    // NOTE: The new --tail mode is not yet implemented. This will be part of the next stage.
    if (config.streamMode) {
        if (config.outputFormat != "text" && config.outputFormat != "csv") {
            std::cerr << "Error: Streaming mode only supports 'text' or 'csv' output format." << std::endl;
            return 1;
        }
        auto streamEntryCallback = [&](const LogEntry &entry) {
            if (rootFilter->matches(entry)) {
                 if (config.outputFormat == "text") {
                      *outputStream << analyzer.formatEntry(entry, config.textOutputFormat, useColors) << std::endl;
                 } else { // CSV
                    // TODO: Implement configurable CSV fields from config.csvFields
                     *outputStream << "\"" << analyzer.formatTimestamp(entry.timestamp) << "\"" << config.csvSeparator
                                   << "\"" << analyzer.logLevelToString(entry.level) << "\"" << config.csvSeparator
                                   << "\"" << entry.message << "\""
                                   << config.csvSeparator << "\"" << entry.sourceFile << "\"" << std::endl;
                 }
            }
            return true;
        };
        // NOTE: The new config.parserErrorAction is not yet plumbed into analyzeStream.
        analyzer.analyzeStream(config.filePaths, streamEntryCallback, config.customParserPattern);
    } else {
        for (const auto& path : config.filePaths) {
            // NOTE: The new config.parserErrorAction is not yet plumbed into append.
            if(auto res = analyzer.append(path, config.customParserPattern); !res) {
                 std::cerr << "Error analyzing file " << path << ": " << res.error().message << std::endl;
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
        if (config.sortBy != SortBy::TIMESTAMP || config.sortOrder != SortOrder::ASCENDING) {
            std::sort(filteredEntries.begin(), filteredEntries.end(), [&](const LogEntry& a, const LogEntry& b) {
                if (config.sortBy == SortBy::TIMESTAMP) {
                    return config.sortOrder == SortOrder::ASCENDING ? a.timestamp < b.timestamp : a.timestamp > b.timestamp;
                } else if (config.sortBy == SortBy::LEVEL) {
                    return config.sortOrder == SortOrder::ASCENDING ? a.level < b.level : a.level > b.level;
                } else { // MESSAGE
                    return config.sortOrder == SortOrder::ASCENDING ? a.message < b.message : a.message > b.message;
                }
            });
        }

        if (config.outputFormat == "text") {
            for(const auto& entry : filteredEntries) {
                *outputStream << analyzer.formatEntry(entry, config.textOutputFormat, useColors) << std::endl;
            }
        } else if (config.outputFormat == "csv") {
            // TODO: Implement configurable CSV fields from config.csvFields
            *outputStream << "Timestamp" << config.csvSeparator << "Level" << config.csvSeparator << "Message" << config.csvSeparator << "File\n";
            for (const auto& entry : filteredEntries) {
                *outputStream << analyzer.formatTimestamp(entry.timestamp) << config.csvSeparator
                              << analyzer.logLevelToString(entry.level) << config.csvSeparator;
                std::string msg = entry.message;
                bool needsQuotes = msg.find(config.csvSeparator) != std::string::npos || msg.find('"') != std::string::npos;
                if (needsQuotes) {
                    Utils::replaceAll(msg, "\"", "\"\"");
                    *outputStream << "\"" << msg << "\"";
                } else {
                    *outputStream << msg;
                }
                *outputStream << config.csvSeparator << entry.sourceFile << "\n";
            }
        } else if (config.outputFormat == "json") {
             json j;
             if (config.includeSummary) {
                 j["totalEntries"] = filteredEntries.size();
             }
             j["entries"] = json::array();
             for (const auto& entry : filteredEntries) {
                 // TODO: Implement configurable JSON fields (less critical, but good for consistency)
                 j["entries"].push_back(json{
                     {"timestamp", analyzer.formatTimestamp(entry.timestamp)},
                     {"level", analyzer.logLevelToString(entry.level)},
                     {"message", entry.message},
                     {"file", entry.sourceFile}
                 });
             }
             if (config.prettyPrint) {
                 *outputStream << std::setw(4) << j << std::endl;
             } else {
                 *outputStream << j << std::endl;
             }
        }

        // NOTE: The statistics part will be refactored into IStatisticCollector system next.
        if (config.showUniqueMessages) {
            std::map<std::string, int> counts;
            for (const auto& entry : filteredEntries) counts[entry.message]++;
            *outputStream << "\nUnique Messages: " << counts.size() << "\n";
        }

        if (config.showTopMessages) {
            std::map<std::string, int> counts;
            for (const auto& entry : filteredEntries) counts[entry.message]++;
            std::vector<std::pair<std::string, int>> sorted(counts.begin(), counts.end());
            std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b){ return a.second > b.second; });
            *outputStream << "\nTop " << config.topMessagesCount << " Messages:\n";
            for (int i=0; i < std::min((int)sorted.size(), config.topMessagesCount); ++i) {
                *outputStream << sorted[i].second << ": " << sorted[i].first << "\n";
            }
        }

        if (config.showEntryRate) {
             if (filteredEntries.size() > 1) {
                 auto dur = filteredEntries.back().timestamp - filteredEntries.front().timestamp;
                 auto secs = std::chrono::duration_cast<std::chrono::seconds>(dur).count();
                 double rate = secs > 0 ? (double)filteredEntries.size() / secs : filteredEntries.size();
                 *outputStream << "\nAverage Entry Rate: " << rate << " entries/sec\n";
             }
        }
    }

    return 0;
}
