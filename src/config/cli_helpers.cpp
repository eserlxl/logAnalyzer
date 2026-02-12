// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "config/cli.h"
#include "config/utils.h"
#include <regex>
#include <sstream>
#include <algorithm>

namespace CLIConfigHelpers {

void trimInPlace(std::string& s) {
    const auto first = s.find_first_not_of(" \t");
    if (first == std::string::npos) {
        s.clear();
        return;
    }
    const auto last = s.find_last_not_of(" \t");
    s = s.substr(first, last - first + 1);
}

std::optional<StatisticConfig> parseStatisticConfig(const std::string& statStr) {
    StatisticConfig config;
    std::string normalized = statStr;
    trimInPlace(normalized);
    const std::string normalizedLower = Utils::toLower(normalized);
    
    if (normalizedLower.rfind("top_messages:", 0) == 0) {
        std::string topN = normalized.substr(13);
        trimInPlace(topN);
        if (topN.empty() || !std::all_of(topN.begin(), topN.end(), [](unsigned char c) {
                return std::isdigit(c) != 0;
            })) {
            return std::nullopt;
        }
        config.type = StatisticType::TOP_MESSAGES;
        config.params["top_n"] = topN;
        return config;
    }

    auto legacyType = Utils::stringToStatisticType(normalized);
    if (legacyType) {
        config.type = *legacyType;
        return config;
    }

    bool typeFound = false;
    std::string token;
    std::istringstream tokenStream(normalized);
    
    while (std::getline(tokenStream, token, ',')) {
        trimInPlace(token);
        if (token.empty()) continue;
        auto pos = token.find('=');
        if (pos != std::string::npos) {
            std::string key = token.substr(0, pos);
            std::string value = token.substr(pos + 1);
            trimInPlace(key);
            trimInPlace(value);
            if (key.empty()) return std::nullopt;
            const std::string normalizedKey = Utils::toLower(key);
            
            if (normalizedKey == "type") {
                if (typeFound) return std::nullopt;
                auto type = Utils::stringToStatisticType(value);
                if (type) {
                    config.type = *type;
                    typeFound = true;
                } else return std::nullopt; 
            } else {
                config.params[normalizedKey] = value;
            }
        } else {
            trimInPlace(token);
            auto type = Utils::stringToStatisticType(token);
            if (type) {
                if (typeFound) return std::nullopt;
                config.type = *type;
                typeFound = true;
            } else return std::nullopt;
        }
    }
    return typeFound ? std::optional<StatisticConfig>(config) : std::nullopt;
}

std::pair<std::string, std::string> parseFieldAlias(const std::string& fieldStr) {
    static const std::regex aliasPattern(R"(^\s*(.*?)\s+[aA][sS]\s+(.*?)\s*$)");
    std::smatch match;
    if (std::regex_match(fieldStr, match, aliasPattern)) {
        std::string field = match[1].str();
        std::string alias = match[2].str();
        trimInPlace(field);
        trimInPlace(alias);
        if (field.empty() || alias.empty()) return {"", ""};
        return {std::move(field), std::move(alias)};
    }
    std::string field = fieldStr;
    trimInPlace(field);
    return {field, field};
}

} // namespace CLIConfigHelpers
