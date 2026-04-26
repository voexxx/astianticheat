#pragma once
#include <string>
#include <vector>

namespace Scanner {

    // Результат сканирования
    struct ScanResult {
        bool        cheatsFound;          // Найдены ли читы
        std::vector<std::string> foundProcesses;  // Найденные чит-процессы
        std::vector<std::string> foundModules;    // Найденные чит-модули
    };

    // Функции сканирования
    std::vector<std::string> GetRunningProcesses();
    std::vector<std::string> ScanProcesses();
    std::vector<std::string> ScanModules();

    // Запустить полное сканирование
    ScanResult FullScan();
}
