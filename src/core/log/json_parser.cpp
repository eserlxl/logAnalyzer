// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "core/log/json_parser.h"
#include "utils/time.h" // For Utils::parseTimestamp
#include "utils/string.h" // For Utils::contains
#include "utils/core.h" // For Utils::generateLogEntryId
#include <iostream> // For std::cerr
#include <sstream> // For std::stringstream

// For nlohmann::json_object_t
#include <nlohmann/json.hpp>

JsonLogParser::JsonLogParser(const std::map<std::string, LogLevel, LogAnalyzerInternal::ci_less>& levelMappings,
                             ParserErrorAction errorAction,
                             std::optional<std::function<void(const std::string&)>> warningLogger)
    : customLevelMappings_(levelMappings),
      parserErrorAction_(errorAction),
      warningLogger_(std::move(warningLogger)) {
}

ErrorCode::Result<LogEntry> JsonLogParser::parseLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) const {
    LogEntry entry;
    entry.sourceFile = sourceFile;
    entry.sourceLineNumber = lineNumber;
    entry.id = Utils::generateLogEntryId(sourceFile, lineNumber, line);

    try {
        nlohmann::json j = nlohmann::json::parse(line);

        // Timestamp
        if (j.contains("timestamp") && j["timestamp"].is_string()) {
            auto tsRes = Utils::parseTime(j["timestamp"].get<std::string>());
            if (tsRes) {
                entry.timestamp = *tsRes;
            } else {
                entry.parsingErrors.push_back(tsRes.error());
            }
        } else {
            entry.parsingErrors.push_back({Code::MissingField, "Missing or invalid 'timestamp' field"});
        }

        // Level
        if (j.contains("level") && j["level"].is_string()) {
            entry.level = Utils::stringToLogLevel(j["level"].get<std::string>(), customLevelMappings_);
        } else {
            entry.level = LogLevel::UNKNOWN;
            entry.parsingErrors.push_back({Code::MissingField, "Missing or invalid 'level' field"});
        }

        // Message
        if (j.contains("message") && j["message"].is_string()) {
            entry.message = j["message"].get<std::string>();
        } else {
            entry.message = "";
            entry.parsingErrors.push_back({Code::MissingField, "Missing or invalid 'message' field"});
        }

        // Optional fields
        if (j.contains("threadId") && j["threadId"].is_string()) {
            entry.threadId = j["threadId"].get<std::string>();
        }
        if (j.contains("module") && j["module"].is_string()) {
            entry.module = j["module"].get<std::string>();
        }
        if (j.contains("host") && j["host"].is_string()) {
            entry.host = j["host"].get<std::string>();
        }
        if (j.contains("structuredData") && j["structuredData"].is_string()) {
            entry.structuredData = j["structuredData"].get<std::string>();
        } else if (j.is_object()) {
            // If structuredData field is not present, but the log line itself is a JSON object,
            // we can treat the entire JSON as structured data if it's not purely mapping to LogEntry fields.
            // For now, we'll store the raw JSON string if it contains extra fields.
            // This is a heuristic; a more robust solution might require explicit configuration.
            nlohmann::json::object_t otherFields;
            for (auto it = j.begin(); it != j.end(); ++it) {
                // Exclude standard fields already processed
                if (it.key() != "timestamp" && it.key() != "level" && it.key() != "message" &&
                    it.key() != "threadId" && it.key() != "module" && it.key() != "host" &&
                    it.key() != "structuredData") {
                    otherFields[it.key()] = it.value();
                }
            }
            if (!otherFields.empty()) {
                entry.structuredData = nlohmann::json(otherFields).dump();
            }
        }

        // Custom fields - iterate over remaining fields in JSON object
        if (j.is_object()) {
            for (auto it = j.begin(); it != j.end(); ++it) {
                // If it's not a standard field, add it to customFields
                if (it.key() != "timestamp" && it.key() != "level" && it.key() != "message" &&
                    it.key() != "threadId" && it.key() != "module" && it.key() != "host" &&
                    it.key() != "structuredData") {
                    entry.customFields[it.key()] = it.value().dump(); // Store as string representation
                }
            }
        }


    } catch (const nlohmann::json::parse_error& e) {
        entry.parsingErrors.push_back({Code::JsonParseError, std::string("JSON parse error: ") + e.what()});
    } catch (const nlohmann::json::type_error& e) {
        entry.parsingErrors.push_back({Code::JsonTypeError, std::string("JSON type error: ") + e.what()});
    } catch (const nlohmann::json::exception& e) {
        entry.parsingErrors.push_back({Code::UnknownJsonError, std::string("JSON error: ") + e.what()});
    } catch (const std::exception& e) {
        entry.parsingErrors.push_back({Code::Unknown, std::string("Unknown error during JSON parsing: ") + e.what()});
    }

    if (entry.hasParsingErrors()) {
        if (parserErrorAction_ == ParserErrorAction::Throw) {
            return std::unexpected(entry.parsingErrors.front());
        }
        return applyParserErrorAction(std::unexpected(entry.parsingErrors.front()), line, lineNumber, sourceFile);
    }

    return entry;
}

std::unique_ptr<ILogParser> JsonLogParser::clone() const {
    return std::make_unique<JsonLogParser>(customLevelMappings_, parserErrorAction_, warningLogger_);
}

std::optional<ErrorCode::Result<LogEntry>> JsonLogParser::processLine(std::string_view line, size_t lineNumber, const std::string& sourceFile) {
    // JSON parser processes line by line, so no multi-line buffering.
    return parseLine(line, lineNumber, sourceFile);
}

std::vector<ErrorCode::Result<LogEntry>> JsonLogParser::flushRemaining() {
    // No buffered lines for JSON parser
    return {};
}

void JsonLogParser::processStream(std::istream& inputStream, 
                                  const std::function<void(ErrorCode::Result<LogEntry>)>& onEntry,
                                  const std::string& sourceFile) {
    std::string line;
    size_t lineNumber = 0;
    while (std::getline(inputStream, line)) {
        lineNumber++;
        if (line.empty()) {
            continue;
        }
        ErrorCode::Result<LogEntry> result = parseLine(line, lineNumber, sourceFile);
        onEntry(result);
    }
}

LogEntry JsonLogParser::applyParserErrorAction(const ErrorCode::Result<LogEntry>& parseResult,
                                                std::string_view originalLine,
                                                size_t lineNumber,
                                                const std::string& sourceFile) const {
    if (parseResult.has_value()) {
        return parseResult.value();
    }

    const ErrorCode::Error& error = parseResult.error();
    std::string errorMessage = "Error parsing line " + std::to_string(lineNumber) + " in " + sourceFile + ": " + error.message;

    switch (parserErrorAction_) {
        case ParserErrorAction::Ignore: {
            LogEntry errorEntry;
            errorEntry.sourceFile = sourceFile;
            errorEntry.sourceLineNumber = lineNumber;
            errorEntry.message = "IGNORED LINE (parse error): " + std::string(originalLine);
            errorEntry.level = LogLevel::UNKNOWN;
            errorEntry.parsingErrors.push_back(error);
            return errorEntry;
        }
        case ParserErrorAction::Warn: {
            if (warningLogger_) {
                (*warningLogger_)(errorMessage + " Line content: " + std::string(originalLine));
            } else {
                // Fallback warning to stderr if no logger provided
                std::cerr << "Warning: " << errorMessage << '\n';
            }
            LogEntry errorEntry; // Still return an entry, potentially with UNKNOWN level
            errorEntry.sourceFile = sourceFile;
            errorEntry.sourceLineNumber = lineNumber;
            errorEntry.message = "WARNING (parse error): " + std::string(originalLine);
            errorEntry.level = LogLevel::UNKNOWN;
            errorEntry.parsingErrors.push_back(error);
            return errorEntry;
        }
        case ParserErrorAction::Throw: {
            LogEntry errorEntry;
            errorEntry.sourceFile = sourceFile;
            errorEntry.sourceLineNumber = lineNumber;
            errorEntry.message = "THROW MODE parse error: " + std::string(originalLine);
            errorEntry.level = LogLevel::UNKNOWN;
            errorEntry.parsingErrors.push_back(error);
            return errorEntry;
        }
    }
    // Should not be reached
    LogEntry defaultErrorEntry;
    defaultErrorEntry.sourceFile = sourceFile;
    defaultErrorEntry.sourceLineNumber = lineNumber;
    defaultErrorEntry.message = "Unhandled parser error action for line: " + std::string(originalLine);
    defaultErrorEntry.level = LogLevel::UNKNOWN;
    defaultErrorEntry.parsingErrors.push_back(error);
    return defaultErrorEntry;
}
