#include "LogAnalyzer.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <log_file_path>" << std::endl;
        return 1;
    }

    std::string filePath = argv[1];
    LogAnalyzer analyzer;
    analyzer.analyze(filePath);
    analyzer.printSummary();

    // Example of filtering
    std::cout << std::endl;
    analyzer.filterByLevel(LogLevel::ERROR);

    return 0;
}
