// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#ifndef LOGANALYZER_CI_LESS_H
#define LOGANALYZER_CI_LESS_H

#include <string>
#include <algorithm>
#include <cctype>
#include <locale>
#include <map> // Although not directly used in ci_less, it's frequently used with it.

namespace LogAnalyzerInternal { // Using a distinct namespace to avoid conflict with class LogAnalyzer

// Case-insensitive comparator for strings
struct ci_less {
  struct nocase_compare {
    // Using static const std::locale classic_locale for efficiency and locale-independence
    char toLowerChar(char c) const {
      static const std::locale classic_locale;
      return std::use_facet<std::ctype<char>>(classic_locale).tolower(c);
    }

    bool operator()(char c1, char c2) const {
      return toLowerChar(c1) < toLowerChar(c2);
    }
  };
  bool operator()(const std::string &s1, const std::string &s2) const {
    return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(),
                                        s2.end(), nocase_compare());
  }
};

} // namespace LogAnalyzer

#endif // LOGANALYZER_CI_LESS_H
