// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer/Log/Writer.h" // New
#include "analyzer/Core.h"
#include "export/Core.h"
#include "utils/Core.h"
#include "utils/String.h"
#include "config/Settings.h"
#include "config/CLI.h" // Added for CLIConfig
#include "core/Error.h" // New: For Error struct and Result alias
#include "filter/ConcreteFilters.h"
#include "filter/Parser.h"
#include "filter/Types.h"
#include "stats/Core.h" // For statistic collectors
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
using namespace filter;

namespace {
CompositeFilter::Logic toCompositeLogic(filter::FilterLogicalOperator op) {
    return op == filter::FilterLogicalOperator::OR
        ? CompositeFilter::Logic::OR
        : CompositeFilter::Logic::AND;
}
} // namespace

int main(int argc, char *argv[]) {
    auto expectedConfig = CLIConfig::parseCLI(argc, argv);
    if (!expectedConfig) {
        // Print the error message and exit.
        std::cerr << expectedConfig.error().message << std::endl;
        return 1;
    }
    const auto& [analyzerSettings, cliOptions] = expectedConfig.value();

    LogAnalyzer analyzer(analyzerSettings); // Construct with settings
    LogWriter logWriter(analyzer); // New: Create LogWriter instance

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

    auto rootFilter = std::make_shared<CompositeFilter>(CompositeFilter::Logic::AND);
    std::optional<filter::FilterExpression> parsedExpression;

    // Inclusion filters
    const CompositeFilter::Logic inclusionLogic =
        toCompositeLogic(cliOptions.filterLogic.value_or(filter::FilterLogicalOperator::AND));
    auto inclusionFilters = std::make_shared<CompositeFilter>(inclusionLogic);
    if (cliOptions.minLogLevel.has_value()) inclusionFilters->add(std::make_shared<MinLevelFilter>(*cliOptions.minLogLevel));
    if (!cliOptions.filterLevels.empty()) {
        auto levelSet = std::make_shared<CompositeFilter>(CompositeFilter::Logic::OR);
        for (auto l : cliOptions.filterLevels) levelSet->add(std::make_shared<LevelFilter>(l));
        inclusionFilters->add(levelSet);
    }
    if (!cliOptions.filterKeywords.empty()) {
        auto keywordSet = std::make_shared<CompositeFilter>(inclusionLogic);
        for (const auto& keyword : cliOptions.filterKeywords) keywordSet->add(std::make_shared<KeywordFilter>(keyword, cliOptions.keywordCaseSensitive));
        inclusionFilters->add(keywordSet);
    }
    if (!cliOptions.regexPatterns.empty()) {
        auto regexSet = std::make_shared<CompositeFilter>(inclusionLogic);
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

    if (!cliOptions.complexFilterExpression.empty()) {
        auto expressionResult = filter::parseQuery(cliOptions.complexFilterExpression);
        if (!expressionResult) {
            std::cerr << "Error: Invalid --expression filter: " << expressionResult.error().toString() << std::endl;
            return 1;
        }
        parsedExpression = std::move(*expressionResult);
    }

    auto expressionMatches = [&](const LogEntry& entry) {
        if (!parsedExpression.has_value()) {
            return true;
        }
        auto evalResult = parsedExpression->evaluate(entry);
        if (!evalResult.has_value()) {
            std::cerr << "Warning: Failed to evaluate --expression for entry: " << evalResult.error().toString() << std::endl;
            return false;
        }
        return *evalResult;
    };

    if (cliOptions.streamMode) {
        if (cliOptions.outputFormat != "text" && cliOptions.outputFormat != "csv") {
            std::cerr << "Error: Streaming mode only supports 'text' or 'csv' output format." << std::endl;
            return 1;
        }

        // Prepare CSV fields if needed
        std::vector<ExportFieldMapping> csvFieldsToExport;
        if (cliOptions.outputFormat == "csv") {
             if (!analyzerSettings.exportSettings.csvFields.empty()) {
                for (const auto& fieldPair : analyzerSettings.exportSettings.csvFields) {
                    ExportFieldMapping mapping;
                    LogEntryField fieldEnum = Utils::stringToLogEntryField(fieldPair.first);
                    if (fieldEnum != LogEntryField::UNKNOWN) {
                        mapping.field = fieldEnum;
                    } else {
                        mapping.field = fieldPair.first; // It's a custom field
                    }
                    
                    if (fieldPair.first != fieldPair.second) {
                        mapping.customHeader = fieldPair.second;
                    }
                    csvFieldsToExport.push_back(mapping);
                }
            } else {
                // Default CSV fields
                csvFieldsToExport.emplace_back(LogEntryField::TIMESTAMP, "Timestamp");
                csvFieldsToExport.emplace_back(LogEntryField::LEVEL, "Level");
                csvFieldsToExport.emplace_back(LogEntryField::MESSAGE, "Message");
                csvFieldsToExport.emplace_back(LogEntryField::SOURCE_FILE, "File");
            }
            
            // Print Header
            for (size_t i = 0; i < csvFieldsToExport.size(); ++i) {
                std::string header = csvFieldsToExport[i].customHeader;
                if (header.empty()) {
                    std::visit([&header](auto&& arg) {
                        using T = std::decay_t<decltype(arg)>;
                        if constexpr (std::is_same_v<T, LogEntryField>) {
                            header = Utils::logEntryFieldToString(arg);
                        } else if constexpr (std::is_same_v<T, std::string>) {
                            header = arg;
                        }
                    }, csvFieldsToExport[i].field);
                }
                
                // Escape header
                 bool needsQuotes = header.find(cliOptions.csvSeparator) != std::string::npos || header.find('"') != std::string::npos;
                 if (needsQuotes) {
                     std::string escaped = header;
                     Utils::replaceAll(escaped, "\"", "\"\"");
                     *outputStream << "\"" << escaped << "\"";
                 } else {
                     *outputStream << header;
                 }
                 
                 if (i < csvFieldsToExport.size() - 1) *outputStream << cliOptions.csvSeparator;
            }
            *outputStream << std::endl;
        }

        auto streamEntryCallback = [&](const LogEntry &entry) {
            if (rootFilter->matches(entry) && expressionMatches(entry)) {
                 if (cliOptions.outputFormat == "text") {
                    FormattingOptions fmtOptions;
                    fmtOptions.useColor = useColors;
                    fmtOptions.dateTimeFormat = "%Y-%m-%d %H:%M:%S";
                    *outputStream << logWriter.formatEntry(entry, fmtOptions) << std::endl;
                 } else { // CSV
                    for (size_t i = 0; i < csvFieldsToExport.size(); ++i) {
                        const auto& fieldMapping = csvFieldsToExport[i];
                        std::string value;
                        
                        std::visit([&](auto&& arg) {
                            using T = std::decay_t<decltype(arg)>;
                            if constexpr (std::is_same_v<T, LogEntryField>) {
                                switch (arg) {
                                    case LogEntryField::ID: if (entry.id.has_value()) value = std::to_string(entry.id.value()); break;
                                    case LogEntryField::TIMESTAMP: if (entry.timestamp.has_value()) value = Utils::formatTimestamp(entry.timestamp.value()); break;
                                    case LogEntryField::LEVEL: value = Utils::logLevelToString(entry.level); break;
                                    case LogEntryField::MESSAGE: value = entry.message; break;
                                    case LogEntryField::SOURCE_FILE: value = entry.sourceFile; break;
                                    case LogEntryField::LINE_NUMBER: if (entry.sourceLineNumber.has_value()) value = std::to_string(entry.sourceLineNumber.value()); break;
                                    case LogEntryField::THREAD_ID: if (entry.threadId.has_value()) value = entry.threadId.value(); break;
                                    case LogEntryField::MODULE: if (entry.module.has_value()) value = entry.module.value(); break;
                                    case LogEntryField::HOST: if (entry.host.has_value()) value = entry.host.value(); break;
                                    default: break;
                                }
                            } else if constexpr (std::is_same_v<T, std::string>) {
                                if (entry.customFields.count(arg)) {
                                    value = entry.customFields.at(arg);
                                }
                            }
                        }, fieldMapping.field);

                        bool needsQuotes = value.find(cliOptions.csvSeparator) != std::string::npos || value.find('"') != std::string::npos || value.find('\n') != std::string::npos;
                        if (needsQuotes) {
                            Utils::replaceAll(value, "\"", "\"\"");
                            *outputStream << "\"" << value << "\"";
                        } else {
                            *outputStream << value;
                        }
                        if (i < csvFieldsToExport.size() - 1) *outputStream << cliOptions.csvSeparator;
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
        const auto entriesSnapshot = analyzer.getEntriesSnapshot();
        for (const auto& entry : entriesSnapshot) {
            if (rootFilter->matches(entry) && expressionMatches(entry)) {
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

        ExportSettings exportSettings;
        exportSettings.useAnsiColors = useColors;
        exportSettings.separator = cliOptions.csvSeparator;
        if (cliOptions.prettyPrint) {
            exportSettings.jsonIndent = 4;
        }

        auto convertCliFieldsToExportMappings = [&](const std::vector<std::pair<std::string, std::string>>& cliFields) {
            std::vector<ExportFieldMapping> mappings;
            for (const auto& fieldPair : cliFields) {
                ExportFieldMapping mapping;
                LogEntryField fieldEnum = Utils::stringToLogEntryField(fieldPair.first);
                if (fieldEnum != LogEntryField::UNKNOWN) {
                    mapping.field = fieldEnum;
                } else {
                    mapping.field = fieldPair.first;
                }

                if (fieldPair.first != fieldPair.second) {
                    mapping.customHeader = fieldPair.second;
                }
                mappings.push_back(mapping);
            }
            return mappings;
        };

        if (cliOptions.outputFormat == "text") {
            exportSettings.format = ExportFormat::PLAINTEXT;
            exportSettings.textFormatString = cliOptions.textOutputFormat;
            exportSettings.useAnsiColors = useColors;
        } else if (cliOptions.outputFormat == "csv") {
            exportSettings.format = ExportFormat::CSV;
            exportSettings.fieldsToExport = convertCliFieldsToExportMappings(analyzerSettings.exportSettings.csvFields);
            exportSettings.includeHeader = true; // Always include header for CSV
        } else if (cliOptions.outputFormat == "json") {
            exportSettings.format = ExportFormat::JSON;
            exportSettings.fieldsToExport = convertCliFieldsToExportMappings(analyzerSettings.exportSettings.jsonFields);
        } else if (cliOptions.outputFormat == "xml") {
            exportSettings.format = ExportFormat::XML;
        } else {
             std::cerr << "Error: Unknown output format: " << cliOptions.outputFormat << std::endl;
             return 1;
        }

        // Use the Exporter class for all non-text outputs
        Exporter exporter;
        try {
            exporter.exportLogEntries(*outputStream, filteredEntries, exportSettings);
        } catch (const ExportException& e) {
            std::cerr << "Error exporting log entries: " << e.what() << std::endl;
            return 1;
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
