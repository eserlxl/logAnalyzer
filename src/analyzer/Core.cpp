#include "analyzer/Core.h"
#include "core/LogParser.h"
#include "filter/Core.h"
#include "stats/Statistics.h"
#include "export/Exporter.h"
#include "utils/Core.h"
#include "core/Error.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <regex>
#include <iomanip>
#include <sstream>
#include <memory> 
#include <vector>
#include <utility>
#include <future>
#include <functional>
#include <iterator>
#include <map>
#include <mutex>
#include <expected>
#include <string_view>
#include <span>

#include "config/Core.h" // Renamed from LogAnalyzerConfig.h
#include "config/CLIConfig.h"


LogAnalyzer::LogAnalyzer()
    : currentSettings_(),
      customLogLevelMapping_(currentSettings_.customLogLevelMappings), // Initialize with settings' mappings
      currentParser_(nullptr) // Initialize to nullptr, then assign in body
{
    // Need to explicitly construct parser using the factory method
    auto parser_or_error = DefaultLogParser::create(
        currentSettings_.lineParsePattern,
        currentSettings_.fieldMappings,
        customLogLevelMapping_, // Use LogAnalyzer's own mapping
        currentSettings_.logEntryStartPattern,
        CLIConfig::ParserErrorAction::Warn // Default action for constructor
    );

    if (parser_or_error.has_value()) {
        currentParser_ = std::move(parser_or_error.value());
    } else {
        // Log an error and throw an exception, as a constructor cannot return std::unexpected
        std::cerr << "Fatal Error: Failed to initialize LogParser in LogAnalyzer default constructor: " << parser_or_error.error().message << std::endl;
        throw std::runtime_error("LogParser initialization failed: " + parser_or_error.error().message);
    }

    for (const auto& config : currentSettings_.statisticConfigs) {
        if (auto collector = createStatisticCollector(config)) {
            collectors_.push_back(collector);
        }
    }
}
LogAnalyzer::LogAnalyzer(const LogAnalyzerSettings& settings)
    : currentSettings_(settings), // Initialize currentSettings_ with provided settings
      customLogLevelMapping_(settings.customLogLevelMappings), // Initialize customLogLevelMapping_ from settings
      currentParser_(nullptr) // Initialize to nullptr, then assign in body
{
    // Need to explicitly construct parser using the factory method
    auto parser_or_error = DefaultLogParser::create(
        currentSettings_.lineParsePattern,
        currentSettings_.fieldMappings,
        customLogLevelMapping_, // Use LogAnalyzer's own mapping
        currentSettings_.logEntryStartPattern,
        CLIConfig::ParserErrorAction::Warn // Default action for constructor
    );

    if (parser_or_error.has_value()) {
        currentParser_ = std::move(parser_or_error.value());
    } else {
        // Log an error and throw an exception, as a constructor cannot return std::unexpected
        std::cerr << "Fatal Error: Failed to initialize LogParser in LogAnalyzer settings constructor: " << parser_or_error.error().message << std::endl;
        throw std::runtime_error("LogParser initialization failed: " + parser_or_error.error().message);
    }

    for (const auto& config : settings.statisticConfigs) {
        if (auto collector = createStatisticCollector(config)) {
            collectors_.push_back(collector);
        }
    }
}

LogAnalyzer::~LogAnalyzer() {
    // Wait for any pending async tasks to complete.
    std::unique_lock<std::mutex> lock(pendingAsyncTasksMutex_);
    for (auto& future : pendingAsyncTasks_) {
        if (future.valid()) {
            future.wait();
        }
    }
}

ErrorCode::Result<void> LogAnalyzer::setSettings(const LogAnalyzerSettings& settings) {
    std::unique_lock<std::shared_mutex> lock(stateMutex_); // Use unique_lock for modifying methods
    currentSettings_ = settings; // Assign directly, no move as settings is const&
    customLogLevelMapping_ = settings.customLogLevelMappings; // Update LogAnalyzer's own mapping
    
    // Clear existing collectors and create new ones based on the updated settings
    collectors_.clear();
    for (const auto& config : currentSettings_.statisticConfigs) {
        if (auto collector = createStatisticCollector(config)) {
            collectors_.push_back(collector);
        }
    }

    auto parser_or_error = DefaultLogParser::create(
        currentSettings_.lineParsePattern, 
        currentSettings_.fieldMappings, 
        customLogLevelMapping_ // Use LogAnalyzer's own mapping
    );
    if (parser_or_error.has_value()) {
        currentParser_ = std::move(parser_or_error.value());
        return {};
    } else {
        return std::unexpected(ErrorCode::Error(Code::InvalidRegex, "Failed to create parser with new settings."));
    }
}

const LogAnalyzerSettings& LogAnalyzer::getSettings() const {
    return currentSettings_;
}

void LogAnalyzer::clear() {
    std::unique_lock<std::shared_mutex> lock(stateMutex_);
    entries_.clear();
    levelCounts.clear();
    lastReport = {};
}

void LogAnalyzer::setCustomLogLevelMapping(std::string_view levelString, LogLevel mappedLevel) {
    std::unique_lock<std::shared_mutex> lock(customLogLevelMappingMutex_); // Use specific mutex for this map
    customLogLevelMapping_[std::string(levelString)] = mappedLevel;
    // Recreate the parser with the updated customLogLevelMapping_
    // This assumes that other settings (pattern, fieldMappings) are not changing,
    // and customLogLevelMapping_ is independent from currentSettings_.
    auto parser_or_error = DefaultLogParser::create(
        currentSettings_.lineParsePattern,
        currentSettings_.fieldMappings,
        customLogLevelMapping_,
        currentSettings_.logEntryStartPattern,
        CLIConfig::ParserErrorAction::Warn // Default action for constructor
    );

    if (parser_or_error.has_value()) {
        currentParser_ = std::move(parser_or_error.value());
    } else {
        // If updating custom log levels breaks the parser, we should report it.
        // For now, print error and keep old parser. Or throw. Throwing is safer.
        std::cerr << "Error: Failed to re-create parser after updating custom log levels: " << parser_or_error.error().message << std::endl;
        throw std::runtime_error("Failed to re-create parser after updating custom log levels: " + parser_or_error.error().message);
    }
}


