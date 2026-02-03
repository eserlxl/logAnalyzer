#ifndef LOG_PARSER_H
#define LOG_PARSER_H

#include "LogTypes.h"
#include <cctype>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <string_view>

// Case-insensitive comparator for strings
struct ci_less {
  struct nocase_compare {
    bool operator()(const unsigned char &c1, const unsigned char &c2) const {
      return std::tolower(c1) < std::tolower(c2);
    }
  };
  bool operator()(const std::string &s1, const std::string &s2) const {
    return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(),
                                        s2.end(), nocase_compare());
  }
};

// Interface for pluggable log parsers
class ILogParser {
public:
  virtual ~ILogParser() = default;
  virtual ParseResult parseLine(std::string_view line,
                                size_t lineNumber) const = 0;
  virtual std::unique_ptr<ILogParser> clone() const = 0;
  virtual std::string getLineFilterRegex() const { return ".*"; }
  virtual std::regex getLineFilterRegexCompiled() const;
};

// Default implementation of ILogParser using regex
class DefaultLogParser : public ILogParser {
public:
  // Modified constructor to accept custom mappings
  DefaultLogParser(
      std::string pattern = "",
      const std::map<std::string, LogLevel, std::less<>> &mappings = {});
  ParseResult parseLine(std::string_view line,
                        size_t lineNumber) const override;
  std::unique_ptr<ILogParser> clone() const override;
  std::string getLineFilterRegex() const override { return patternString; }
  std::regex getLineFilterRegexCompiled() const override;

private:
  std::regex logPattern;
  std::string patternString; // Store pattern string to allow cloning
  std::map<std::string, LogLevel, ci_less>
      customLevelMappings; // Use this copy
};

#endif // LOG_PARSER_H
