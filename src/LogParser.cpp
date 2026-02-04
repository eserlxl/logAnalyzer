#include "LogParser.h"
#include "LogTypes.h"
#include "Error.h"
#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace ErrorCode;

std::vector<FieldMapping> inferFieldMappingsFromPattern([[maybe_unused]] const std::string& pattern) {
    std::vector<FieldMapping> inferredMappings;
    inferredMappings.push_back(FieldMapping(LogEntryField::TIMESTAMP, 1, std::string("%Y-%m-%d %H:%M:%S")));
    inferredMappings.push_back(FieldMapping(LogEntryField::LEVEL, 2));
    inferredMappings.push_back(FieldMapping(LogEntryField::MESSAGE, 3));
    return inferredMappings;
}

Result<std::unique_ptr<DefaultLogParser>> DefaultLogParser::create(
    std::string pattern,
    std::vector<FieldMapping> fieldMappings,
    const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less>& levelMappings,
    std::optional<std::string> logEntryStartPattern,
    CLIConfig::ParserErrorAction errorAction) {
    try {
        return std::make_unique<DefaultLogParser>(
            std::move(pattern),
            std::move(fieldMappings),
            levelMappings,
            std::move(logEntryStartPattern),
            errorAction
        );
    } catch (const std::regex_error& e) {
        return std::unexpected(Error(Error::Code::InvalidRegex, e.what()));
    }
}

const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> DefaultLogParser::DEFAULT_LEVEL_MAPPINGS = {
    {"TRACE", LogLevel::TRACE},
    {"DEBUG", LogLevel::DEBUG},
    {"INFO", LogLevel::INFO},
    {"WARNING", LogLevel::WARNING},
    {"ERROR", LogLevel::ERROR},
    {"FATAL", LogLevel::FATAL}
};

DefaultLogParser::DefaultLogParser(
    std::string pattern,
    std::vector<FieldMapping> fieldMappings,
    const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less> &levelMappings,
    std::optional<std::string> logEntryStartPattern,
    CLIConfig::ParserErrorAction errorAction)
    : logPattern(pattern, std::regex::optimize),
      patternString(std::move(pattern)),
      fieldMappings(std::move(fieldMappings)),
      customLevelMappings(levelMappings),
      logEntryStartPatternString(std::move(logEntryStartPattern)),
      _parserErrorAction(errorAction)
{
    if (logEntryStartPatternString.has_value()) {
        logEntryStartRegex = std::regex(logEntryStartPatternString.value(), std::regex::optimize);
    }
}

DefaultLogParser::DefaultLogParser(
    std::string pattern)
    : DefaultLogParser(
        pattern,
        inferFieldMappingsFromPattern(pattern),
        {},
        std::nullopt,
        CLIConfig::ParserErrorAction::Warn) {}

Result<LogEntry> DefaultLogParser::parseLineInternal(std::string_view line, size_t lineNumber, const std::string& sourceFile) const {
  LogEntry entry;
  entry.id = lineNumber;
  entry.sourceLineNumber = lineNumber;
  entry.sourceFile = sourceFile;
  entry.level = LogLevel::UNKNOWN;

  if (patternString.empty()) {
    return std::unexpected(Error(Error::Code::MalformedLogEntry, "No regex pattern provided to parser."));
  }

  std::string lineStr(line);
  if (!lineStr.empty() && lineStr.back() == '\r') {
      lineStr.pop_back();
  }
  std::smatch match;

  if (!std::regex_match(lineStr, match, logPattern)) {
    return std::unexpected(Error(Error::Code::MalformedLogEntry, "Line does not match log pattern."));
  }

  bool customFieldsExplicitlyMapped = false;
  for (const auto& mapping : fieldMappings) {
      if (std::holds_alternative<LogEntryField>(mapping.field) &&
          std::get<LogEntryField>(mapping.field) == LogEntryField::STRUCTURED_FIELD) {
          customFieldsExplicitlyMapped = true;
          break;
      }
  }

  for (const auto& mapping : fieldMappings) {
    if (!mapping.groupIndex.has_value() || mapping.groupIndex.value() >= match.size() || !match[mapping.groupIndex.value()].matched) {
        continue;
    }
    std::string capturedValue = match[mapping.groupIndex.value()].str();

    std::visit(
        [&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, LogEntryField>) {
                switch (arg) {
                    case LogEntryField::TIMESTAMP: {
                        auto parsedTime = Utils::parseTimeWithFormats(capturedValue, mapping.formats);
                        if(parsedTime) {
                            entry.timestamp = *parsedTime;
                        } else {
                            // This part of code inside lambda can't return from parent function
                        }
                        break;
                    }
                    case LogEntryField::LEVEL: {
                        entry.level = Utils::stringToLogLevel(capturedValue, customLevelMappings);
                        break;
                    }
                    case LogEntryField::MESSAGE: {
                        entry.message = capturedValue;
                        if (!customFieldsExplicitlyMapped) {
                            const std::regex kvPattern_legacy("([\\w.-]+)\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s,.]+))");
                            auto words_begin = std::sregex_iterator(entry.message.begin(), entry.message.end(), kvPattern_legacy);
                            auto words_end = std::sregex_iterator();
                            for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
                                std::smatch kvMatch = *i;
                                std::string k = kvMatch[1].str();
                                std::string v = kvMatch[2].matched ? kvMatch[2].str() : (kvMatch[3].matched ? kvMatch[3].str() : kvMatch[4].str());
                                entry.customFields[k] = v;
                            }
                        }
                        break;
                    }
                    case LogEntryField::SOURCE_FILE: {
                        entry.sourceFile = capturedValue;
                        break;
                    }
                    case LogEntryField::STRUCTURED_FIELD: {
                        std::string delimiter = (!mapping.formats.empty() && !mapping.formats[0].empty()) ? mapping.formats[0] : "=";
                        std::string pattern_str = "([\\w.-]+)\\s*" + delimiter + "\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s,]+))";
                        const std::regex kvPattern(pattern_str);
                        auto kv_begin = std::sregex_iterator(capturedValue.begin(), capturedValue.end(), kvPattern);
                        auto kv_end = std::sregex_iterator();
                        for (std::sregex_iterator i = kv_begin; i != kv_end; ++i) {
                            std::smatch kvMatch = *i;
                            std::string k = kvMatch[1].str();
                            std::string v = kvMatch[2].matched ? kvMatch[2].str() : (kvMatch[3].matched ? kvMatch[3].str() : kvMatch[4].str());
                            entry.customFields[k] = v;
                        }
                        break;
                    }
                    default:
                        break;
                }
            } else if constexpr (std::is_same_v<T, std::string>) {
                entry.customFields[arg] = capturedValue;
            }
        },
        mapping.field);
  }

  return entry;
}

Result<LogEntry> DefaultLogParser::parseLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) const {
    return parseLineInternal(line, lineNumber, sourceFile);
}

std::optional<Result<LogEntry>> DefaultLogParser::processLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) {
    if (!logEntryStartRegex.has_value()) {
        currentLogEntryBuffer.clear();
        bufferedLineNumbers.clear();
        lastProcessedLineNumber = lineNumber;
        return parseLineInternal(line, lineNumber, sourceFile);
    }

    std::string lineStr(line);
    bool startsNewEntry = std::regex_search(lineStr, *logEntryStartRegex);

    if (startsNewEntry) {
        if (!currentLogEntryBuffer.empty()) {
            Result<LogEntry> prevResult = parseLineInternal(currentLogEntryBuffer, currentLogEntryStartLineNumber, sourceFile);
            currentLogEntryBuffer = lineStr;
            currentLogEntryStartLineNumber = lineNumber;
            bufferedLineNumbers = {lineNumber};
            lastProcessedLineNumber = lineNumber;
            return prevResult;
        } else {
            currentLogEntryBuffer = lineStr;
            currentLogEntryStartLineNumber = lineNumber;
            bufferedLineNumbers = {lineNumber};
            lastProcessedLineNumber = lineNumber;
            return std::nullopt;
        }
    } else {
        if (currentLogEntryBuffer.empty()) {
             currentLogEntryBuffer = lineStr;
             currentLogEntryStartLineNumber = lineNumber;
             bufferedLineNumbers.push_back(lineNumber);
        } else {
            currentLogEntryBuffer += "\n";
            currentLogEntryBuffer += line;
            bufferedLineNumbers.push_back(lineNumber);
        }
        lastProcessedLineNumber = lineNumber;
        return std::nullopt;
    }
}

std::vector<Result<LogEntry>> DefaultLogParser::flushRemaining() {
    std::vector<Result<LogEntry>> results;
    if (!currentLogEntryBuffer.empty()) {
        // Source file is not known here, so we pass an empty string
        results.push_back(parseLineInternal(currentLogEntryBuffer, currentLogEntryStartLineNumber, ""));
        currentLogEntryBuffer.clear();
        currentLogEntryStartLineNumber = 0;
        bufferedLineNumbers.clear();
    }
    return results;
}

std::unique_ptr<ILogParser> DefaultLogParser::clone() const {
  return std::make_unique<DefaultLogParser>(patternString, fieldMappings, customLevelMappings, logEntryStartPatternString, _parserErrorAction);
}
