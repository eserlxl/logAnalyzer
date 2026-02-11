// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "analyzer/log/writer.h"
#include "analyzer/core.h"
#include "export/core.h"
#include "utils/core.h"
#include "utils/string.h"
#include "config/cli.h"
#include "filter/concrete_filters.h"
#include "filter/parser.h"
#include "filter/types.h"
#include "stats/core.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

using namespace filter;

namespace {
CompositeFilter::Logic toCompositeLogic(filter::FilterLogicalOperator op) {
    return op == filter::FilterLogicalOperator::OR
        ? CompositeFilter::Logic::OR
        : CompositeFilter::Logic::AND;
}

void writeCsvEscaped(std::ostream& os, std::string value, char separator) {
    const bool needsQuotes = value.find(separator) != std::string::npos
        || value.find('"') != std::string::npos
        || value.find('\n') != std::string::npos
        || value.find('\r') != std::string::npos;
    if (!needsQuotes) {
        os << value;
        return;
    }
    Utils::replaceAll(value, "\"", "\"\"");
    os << '"' << value << '"';
}
} // namespace

int main(int argc, char *argv[]) {
    LogAnalyzerSettings analyzerSettings;

    auto expectedConfig = CLIConfig::parseCLI(argc, argv);
    if (!expectedConfig) {
        // Print the error message and exit.
        std::cerr << expectedConfig.error().message << '\n';
        return 1;
    }

    analyzerSettings.merge(expectedConfig.value().first);
    const auto& cliOptions = expectedConfig.value().second;
    const bool statisticsEnabled = !analyzerSettings.statisticConfigs.empty();

    LogAnalyzer analyzer(analyzerSettings); // Construct with settings
    LogWriter logWriter(analyzer); // New: Create LogWriter instance

    std::ofstream outFile;
    std::ostream *outputStream = &std::cout;
    if (!cliOptions.outputPath.empty()) {
        outFile.open(cliOptions.outputPath);
        if (!outFile.is_open()) {
            std::cerr << "Error: Could not open output file: " << cliOptions.outputPath << '\n';
            return 1;
        }
        outputStream = &outFile;
    }

    const bool isTerminalOutput =
#if defined(_WIN32)
        _isatty(_fileno(stdout)) != 0;
#else
        isatty(fileno(stdout)) != 0;
#endif

    bool useColors = (cliOptions.colorOption == CLIConfig::ColorOption::ALWAYS) ||
                     (cliOptions.colorOption == CLIConfig::ColorOption::AUTO && isTerminalOutput && cliOptions.outputPath.empty());

    auto rootFilter = std::make_shared<CompositeFilter>(CompositeFilter::Logic::AND);
    std::optional<filter::FilterExpression> parsedExpression;

    // Inclusion filters
    const CompositeFilter::Logic inclusionLogic =
        toCompositeLogic(cliOptions.filterLogic.value_or(filter::FilterLogicalOperator::AND));
    auto inclusionFilters = std::make_shared<CompositeFilter>(inclusionLogic);
    if (cliOptions.minLogLevel) inclusionFilters->add(std::make_shared<MinLevelFilter>(*cliOptions.minLogLevel));
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
            if (!regexFilterResult) {
                std::cerr << "Error: Invalid regex pattern for inclusion filter: " << regexFilterResult.error().toString() << '\n';
                return 1;
            }
            regexSet->add(*regexFilterResult);
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
            if (!regexFilterResult) {
                std::cerr << "Error: Invalid regex pattern for exclusion filter: " << regexFilterResult.error().toString() << '\n';
                return 1;
            }
            exclusionSet->add(*regexFilterResult);
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
            std::cerr << "Error: Invalid --expression filter: " << expressionResult.error().toString() << '\n';
            return 1;
        }
        parsedExpression = std::move(*expressionResult);
    }

    auto expressionMatches = [&](const LogEntry& entry) {
        if (!parsedExpression) {
            return true;
        }
        auto evalResult = parsedExpression->evaluate(entry);
        if (!evalResult) {
            std::cerr << "Warning: Failed to evaluate --expression for entry: " << evalResult.error().toString() << '\n';
            return false;
        }
        return *evalResult;
    };

    if (cliOptions.streamMode) {
        if (cliOptions.outputFormat != "text" && cliOptions.outputFormat != "csv") {
            std::cerr << "Error: Streaming mode only supports 'text' or 'csv' output format." << '\n';
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
                 writeCsvEscaped(*outputStream, header, cliOptions.csvSeparator);
                 
                 if (i < csvFieldsToExport.size() - 1) *outputStream << cliOptions.csvSeparator;
            }
            *outputStream << '\n';
        }

        auto streamEntryCallback = [&](const LogEntry &entry) {
            if (rootFilter->matches(entry) && expressionMatches(entry)) {
                 if (cliOptions.outputFormat == "text") {
                    FormattingOptions fmtOptions;
                    fmtOptions.useColor = useColors;
                    fmtOptions.dateTimeFormat = "%Y-%m-%d %H:%M:%S";
                    *outputStream << logWriter.formatEntry(entry, fmtOptions) << '\n';
                 } else { // CSV
                    for (size_t i = 0; i < csvFieldsToExport.size(); ++i) {
                        const auto& fieldMapping = csvFieldsToExport[i];
                        std::string value;
                        
                        std::visit([&](auto&& arg) {
                            using T = std::decay_t<decltype(arg)>;
                            if constexpr (std::is_same_v<T, LogEntryField>) {
                                switch (arg) {
                                    case LogEntryField::ID: if (entry.id) value = std::to_string(*entry.id); break;
                                    case LogEntryField::TIMESTAMP: if (entry.timestamp) value = Utils::formatTimestamp(*entry.timestamp); break;
                                    case LogEntryField::LEVEL: value = Utils::logLevelToString(entry.level); break;
                                    case LogEntryField::MESSAGE: value = entry.message; break;
                                    case LogEntryField::SOURCE_FILE: value = entry.sourceFile; break;
                                    case LogEntryField::LINE_NUMBER: if (entry.sourceLineNumber) value = std::to_string(*entry.sourceLineNumber); break;
                                    case LogEntryField::THREAD_ID: if (entry.threadId) value = *entry.threadId; break;
                                    case LogEntryField::MODULE: if (entry.module) value = *entry.module; break;
                                    case LogEntryField::HOST: if (entry.host) value = *entry.host; break;
                                    default: break;
                                }
                            } else if constexpr (std::is_same_v<T, std::string>) {
                                if (entry.customFields.count(arg)) {
                                    value = entry.customFields.at(arg);
                                }
                            }
                        }, fieldMapping.field);

                        writeCsvEscaped(*outputStream, value, cliOptions.csvSeparator);
                        if (i < csvFieldsToExport.size() - 1) *outputStream << cliOptions.csvSeparator;
                    }
                    *outputStream << '\n';
                 }
            }
            return true;
        };

        if(auto res = analyzer.analyzeStream(cliOptions.filePaths, streamEntryCallback, cliOptions.parserErrorAction); !res) {
            std::cerr << "Error during stream analysis: " << res.error().toString() << '\n';
            return 1;
        }

    } else {
        for (const auto& path : cliOptions.filePaths) {
            if(auto res = analyzer.append(path, cliOptions.parserErrorAction); !res) {
                 std::cerr << "Error analyzing file " << path << ": " << res.error().toString() << '\n';
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

        // Sorting: treat unspecified options as defaults.
        const SortBy sortBy = cliOptions.sortBy.value_or(SortBy::TIMESTAMP);
        const SortOrder sortOrder = cliOptions.sortOrder.value_or(SortOrder::ASCENDING);
        if (sortBy != SortBy::TIMESTAMP || sortOrder != SortOrder::ASCENDING) {
            std::sort(filteredEntries.begin(), filteredEntries.end(), [&](const LogEntry& a, const LogEntry& b) {
                auto less = [&](const auto& lhs, const auto& rhs) {
                    return sortOrder == SortOrder::ASCENDING ? lhs < rhs : lhs > rhs;
                };

                switch (sortBy) {
                    case SortBy::TIMESTAMP:
                        return less(a.timestamp, b.timestamp);
                    case SortBy::LEVEL:
                        return less(a.level, b.level);
                    case SortBy::MESSAGE:
                        return less(a.message, b.message);
                    case SortBy::SOURCE:
                        return less(a.sourceFile, b.sourceFile);
                    case SortBy::THREAD_ID:
                        return less(a.threadId, b.threadId);
                    default:
                        return less(a.message, b.message);
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
             std::cerr << "Error: Unknown output format: " << cliOptions.outputFormat << '\n';
             return 1;
        }

        // Use the Exporter class for all non-text outputs
        Exporter exporter;
        try {
            exporter.exportLogEntries(*outputStream, filteredEntries, exportSettings);
        } catch (const ExportException& e) {
            std::cerr << "Error exporting log entries: " << e.what() << '\n';
            return 1;
        }

        if (statisticsEnabled) {
            // Collectors are configured via parsed settings; reset them and
            // run statistics only on the final filtered entry set.
            analyzer.resetStatisticCollectors();
            for (const auto& entry : filteredEntries) {
                analyzer.processEntryForStatistics(entry);
            }

            *outputStream << "\n--- Statistics ---\n";
            auto reports = analyzer.getAllStatisticReports();
            for (const auto& reportPair : reports) {
                *outputStream << reportPair.second.dump(cliOptions.prettyPrint ? 4 : -1) << '\n';
            }
        }
}

    return 0;
}
