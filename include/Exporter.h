#ifndef EXPORTER_H
#define EXPORTER_H

#include "LogTypes.h"
#include "Filter.h"
#include <iostream>
#include <vector>
#include <string>

// Forward declaration of LogAnalyzer to avoid circular dependency if needed for utility methods
class LogAnalyzer; 

class Exporter {
public:
    // Exports filtered log entries as JSON
    void exportAsJson(
        std::ostream& os, 
        const std::vector<LogEntry>& entries, 
        bool includeSummary, 
        bool prettyPrint);

    // Exports filtered log entries as CSV
    void exportAsCsv(
        std::ostream& os, 
        const std::vector<LogEntry>& entries, 
        char separator);

    // Placeholder for text export (can be enhanced later)
    void exportAsText(
        std::ostream& os, 
        const std::vector<LogEntry>& entries, 
        const std::string& formatString,
        bool useColors);

private:
    // Helper to format a single log entry for text output
    std::string formatEntryForText(
        const LogEntry& entry, 
        const std::string& formatString, 
        bool useColors);
};

#endif // EXPORTER_H
