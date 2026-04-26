#include "scanner.h"
#include "config.h"
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <cctype>

namespace Scanner {

    // Перевести строку в нижний регистр
    std::string ToLower(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
    }

    // Получить список всех запущенных процессов
    std::vector<std::string> GetRunningProcesses() {
        std::vector<std::string> processes;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return processes;

        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);

        if (Process32FirstW(snap, &pe)) {
            do {
                // Конвертируем имя процесса из широких символов
                char name[256];
                WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1,
                    name, sizeof(name), NULL, NULL);
                processes.push_back(ToLower(std::string(name)));
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
        return processes;
    }

    // Проверить процессы на известные читы
    std::vector<std::string> ScanProcesses() {
        std::vector<std::string> found;
        auto running = GetRunningProcesses();

        for (const auto& process : running) {
            for (const auto& cheat : Config::KNOWN_CHEAT_PROCESSES) {
                if (process.find(ToLower(cheat)) != std::string::npos) {
                    found.push_back(process);
                }
            }
        }
        return found;
    }

    // Проверить загруженные модули (DLL)
    std::vector<std::string> ScanModules() {
        std::vector<std::string> found;
        DWORD pid = GetCurrentProcessId();
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
        if (snap == INVALID_HANDLE_VALUE) return found;

        MODULEENTRY32W me;
        me.dwSize = sizeof(me);

        if (Module32FirstW(snap, &me)) {
            do {
                char name[256];
                WideCharToMultiByte(CP_UTF8, 0, me.szModule, -1,
                    name, sizeof(name), NULL, NULL);
                std::string modName = ToLower(std::string(name));

                for (const auto& cheat : Config::KNOWN_CHEAT_MODULES) {
                    if (modName.find(ToLower(cheat)) != std::string::npos) {
                        found.push_back(modName);
                    }
                }
            } while (Module32NextW(snap, &me));
        }
        CloseHandle(snap);
        return found;
    }

    // Полное сканирование
    ScanResult FullScan() {
        ScanResult result;
        result.foundProcesses = ScanProcesses();
        result.foundModules = ScanModules();
        result.cheatsFound = !result.foundProcesses.empty() ||
            !result.foundModules.empty();
        return result;
    }
}