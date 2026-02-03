#include "LogParser.h"
#include "Utils.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>

// Provides a default implementation that compiles the regex string on-the-fly.
std::regex ILogParser::getLineFilterRegexCompiled() const {
  return std::regex(getLineFilterRegex());
}

DefaultLogParser::DefaultLogParser(
    std::string pattern,
    const std::map<std::string, LogLevel, std::less<>> &mappings)
    : patternString(std::move(pattern)) {
  if (!patternString.empty()) {
    logPattern = std::regex(patternString, std::regex::ECMAScript);
  }

  if (mappings.empty()) {
    customLevelMappings["INFO"] = LogLevel::INFO;
    customLevelMappings["WARNING"] = LogLevel::WARNING;
    customLevelMappings["ERROR"] = LogLevel::ERROR;
    customLevelMappings["DEBUG"] = LogLevel::DEBUG;
  } else {
    for (const auto &mapping : mappings) {
      customLevelMappings[mapping.first] = mapping.second;
    }
  }
}

ParseResult DefaultLogParser::parseLine(std::string_view line,
                                        size_t lineNumber) const {
  ParseResult result;
  result.success = false;

  if (patternString.empty()) {
    result.errorMessage = "No regex pattern provided to parser.";
    result.failingPart = std::string(line);
    return result;
  }

  std::string lineStr(line);
  std::smatch match;

  if (!std::regex_match(lineStr, match, logPattern)) {
    result.errorMessage = "Line does not match log pattern.";
    result.failingPart = lineStr;
    return result;
  }

  if (match.size() < 4) {
    result.errorMessage = "Regex pattern did not capture all required fields "
                          "(timestamp, level, message).";
    result.failingPart = patternString;
    return result;
  }

  LogEntry entry;
  entry.id = lineNumber;

  // 1. Parse Timestamp
  std::string timestampStr = match[1].str();
  std::tm tm = {};
  std::stringstream ss(timestampStr);
  ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
  if (ss.fail()) {
    ss.clear();
    ss.str(timestampStr);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S"); // Try ISO 8601 format
    if (ss.fail()) {
      result.errorMessage = "Failed to parse timestamp.";
      result.failingPart = timestampStr;
      return result;
    }
  }
  entry.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

  // 2. Parse Log Level
  std::string levelStr = match[2].str();
  auto it = customLevelMappings.find(levelStr);
  if (it == customLevelMappings.end()) {
    entry.level = LogLevel::UNKNOWN;
  } else {
    entry.level = it->second;
  }

  // 3. Extract Message and Structured Fields
  entry.message = match[3].str();
  std::regex kvPattern("(\\w+)=(\"([^\"]*)\"|(\\S+))");
  auto words_begin =
      std::sregex_iterator(entry.message.begin(), entry.message.end(), kvPattern);
  auto words_end = std::sregex_iterator();

  for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
    std::smatch kvMatch = *i;
    std::string key = kvMatch[1].str();
    std::string value = kvMatch[3].matched ? kvMatch[3].str() : kvMatch[4].str();
    entry.structuredFields[key] = value;
  }

  result.success = true;
  result.entry = std::move(entry);
  return result;
}

std::unique_ptr<ILogParser> DefaultLogParser::clone() const {
  std::map<std::string, LogLevel, std::less<>> mappings;
  for (const auto &pair : customLevelMappings) {
    mappings[pair.first] = pair.second;
  }
  return std::make_unique<DefaultLogParser>(patternString, mappings);
}

std::regex DefaultLogParser::getLineFilterRegexCompiled() const {
  if (patternString.empty()) {
    return std::regex(".*");
  }
  return logPattern;
}
