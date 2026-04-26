#include "fingerprint.h"
#include "config.h"
#include <windows.h>
#include <iphlpapi.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <fstream>

#pragma comment(lib, "iphlpapi.lib")

namespace Fingerprint {

    // Получить MAC адрес
    std::string GetMacAddress() {
        IP_ADAPTER_INFO adapterInfo[16];
        DWORD bufLen = sizeof(adapterInfo);
        if (GetAdaptersInfo(adapterInfo, &bufLen) == ERROR_SUCCESS) {
            std::ostringstream mac;
            mac << std::hex << std::setfill('0');
            for (int i = 0; i < 6; i++) {
                if (i > 0) mac << ":";
                mac << std::setw(2) << (int)adapterInfo[0].Address[i];
            }
            return mac.str();
        }
        return "UNKNOWN";
    }

    // Получить Steam ID из файлов Steam
    std::string GetSteamID() {
        // Пути где Steam хранит данные
        std::vector<std::string> paths = {
            "C:\\Program Files (x86)\\Steam\\config\\loginusers.vdf",
            "C:\\Program Files\\Steam\\config\\loginusers.vdf"
        };

        for (const auto& path : paths) {
            std::ifstream file(path);
            if (!file.is_open()) continue;

            std::string line;
            std::string lastId;
            while (std::getline(file, line)) {
                // Ищем строку со Steam ID (17 цифр)
                if (line.find("\"7656") != std::string::npos) {
                    size_t start = line.find("\"7656");
                    size_t end = line.find("\"", start + 1);
                    if (end != std::string::npos) {
                        lastId = line.substr(start + 1, end - start - 1);
                    }
                }
                // Ищем Steam никнейм
            }
            if (!lastId.empty()) return lastId;
        }
        return "UNKNOWN";
    }

    // Получить Steam никнейм
    std::string GetSteamUsername() {
        std::vector<std::string> paths = {
            "C:\\Program Files (x86)\\Steam\\config\\loginusers.vdf",
            "C:\\Program Files\\Steam\\config\\loginusers.vdf"
        };

        for (const auto& path : paths) {
            std::ifstream file(path);
            if (!file.is_open()) continue;

            std::string line;
            while (std::getline(file, line)) {
                if (line.find("\"PersonaName\"") != std::string::npos) {
                    size_t start = line.find_last_of("\"");
                    size_t end = line.rfind("\"", start - 1);
                    if (start != std::string::npos && end != std::string::npos) {
                        return line.substr(end + 1, start - end - 1);
                    }
                }
            }
        }
        return "UNKNOWN";
    }

    // Получить текущее время
    long long GetCurrentTime() {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()
        ).count();
    }

    // Собрать все данные вместе
    PlayerInfo Collect() {
        PlayerInfo info;
        info.macAddress = GetMacAddress();
        info.steamId = GetSteamID();
        info.username = GetSteamUsername(); // Steam ник вместо Windows юзера
        info.hwid = "";  // убрали
        info.pcName = "";  // убрали
        info.acVersion = Config::AC_VERSION;
        info.launchTime = GetCurrentTime();
        return info;
    }
}