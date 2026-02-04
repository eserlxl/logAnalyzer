#include "LogAnalyzer.h"
#include "Utils.h"
#include "LogAnalyzerSettings.h"
#include "CLIConfig.h" // Added for CLIConfig
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
        // Print the error message (which could be help text) and exit.
        std::cerr << expectedConfig.error();
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
                std::cerr << "Error: Invalid regex pattern for inclusion filter: " << regexFilterResult.error() << std::endl;
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
                std::cerr << "Error: Invalid regex pattern for exclusion filter: " << regexFilterResult.error() << std::endl;
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

    // NOTE: The new --tail mode is not yet implemented. This will be part of the next stage.
    if (cliOptions.streamMode) {
        if (cliOptions.outputFormat != "text" && cliOptions.outputFormat != "csv") {
            std::cerr << "Error: Streaming mode only supports 'text' or 'csv' output format." << std::endl;
            return 1;
        }
        auto streamEntryCallback = [&](const LogEntry &entry) {
            if (rootFilter->matches(entry)) {
                 if (cliOptions.outputFormat == "text") {
                    LogAnalyzer::FormattingOptions fmtOptions;
                    fmtOptions.useColor = useColors;
                    fmtOptions.dateTimeFormat = "%Y-%m-%d %H:%M:%S";
                    *outputStream << analyzer.formatEntry(entry, cliOptions.textOutputFormat, fmtOptions) << std::endl;
                 } else { // CSV
                    // TODO: Implement configurable CSV fields from cliOptions.csvFields
                     *outputStream << "\"" << analyzer.formatTimestamp(entry.timestamp) << "\"" << cliOptions.csvSeparator
                                   << "\"" << analyzer.logLevelToString(entry.level) << "\"" << cliOptions.csvSeparator
                                   << "\"" << entry.message << "\""
                                   << cliOptions.csvSeparator << "\"" << entry.sourceFile << "\"" << std::endl;
                 }
            }
            return true;
        };
        // NOTE: The new cliOptions.parserErrorAction is not yet plumbed into analyzeStream.
        analyzer.analyzeStream(cliOptions.filePaths, streamEntryCallback);
    } else {
        for (const auto& path : cliOptions.filePaths) {
            // NOTE: The new cliOptions.parserErrorAction is not yet plumbed into append.
            if(auto res = analyzer.append(path); !res) {
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
                LogAnalyzer::FormattingOptions fmtOptions;
                fmtOptions.useColor = useColors;
                fmtOptions.dateTimeFormat = "%Y-%m-%d %H:%M:%S";
                *outputStream << analyzer.formatEntry(entry, cliOptions.textOutputFormat, fmtOptions) << std::endl;
            }
        } else if (cliOptions.outputFormat == "csv") {
            // TODO: Implement configurable CSV fields from cliOptions.csvFields
            *outputStream << "Timestamp" << cliOptions.csvSeparator << "Level" << cliOptions.csvSeparator << "Message" << cliOptions.csvSeparator << "File\n";
            for (const auto& entry : filteredEntries) {
                *outputStream << analyzer.formatTimestamp(entry.timestamp) << cliOptions.csvSeparator
                              << analyzer.logLevelToString(entry.level) << cliOptions.csvSeparator;
                std::string msg = entry.message;
                bool needsQuotes = msg.find(cliOptions.csvSeparator) != std::string::npos || msg.find('"') != std::string::npos;
                if (needsQuotes) {
                    Utils::replaceAll(msg, "\"", "\"\"");
                    *outputStream << "\"" << msg << "\"";
                } else {
                    *outputStream << msg;
                }
                *outputStream << cliOptions.csvSeparator << entry.sourceFile << "\n";
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
                     {"timestamp", analyzer.formatTimestamp(entry.timestamp)},
                     {"level", analyzer.logLevelToString(entry.level)},
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

        // NOTE: The statistics part will be refactored into IStatisticCollector system next.
        if (cliOptions.showUniqueMessages) {
            std::map<std::string, int> counts;
            for (const auto& entry : filteredEntries) counts[entry.message]++;
            *outputStream << "\nUnique Messages: " << counts.size() << "\n";
        }

        if (cliOptions.showTopMessages) {
            std::map<std::string, int> counts;
            for (const auto& entry : filteredEntries) counts[entry.message]++;
            std::vector<std::pair<std::string, int>> sorted(counts.begin(), counts.end());
            std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b){ return a.second > b.second; });
            *outputStream << "\nTop " << cliOptions.topMessagesCount << " Messages:\n";
            for (int i=0; i < std::min((int)sorted.size(), cliOptions.topMessagesCount); ++i) {
                *outputStream << sorted[i].second << ": " << sorted[i].first << "\n";
            }
        }

        if (cliOptions.showEntryRate) {
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
