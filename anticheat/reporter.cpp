#include "reporter.h"
#include "config.h"
#include <windows.h>
#include <winhttp.h>
#include <sstream>
#include <string>

#pragma comment(lib, "winhttp.lib")

namespace Reporter {

    // API ключ должен совпадать с сервером
    // В продакшне вынеси в config.h
    const wchar_t* API_KEY = L"change_me_in_production";
    const wchar_t* HOST = L"astianticheat.onrender.com";
    const wchar_t* ROOM_VAL = L"OPEN"; // дефолтная комната

    // Отправить HTTP POST запрос
    bool HttpPost(const std::string& path, const std::string& body) {
        // Открываем сессию
        HINTERNET hSession = WinHttpOpen(
            L"AstiAnticheat/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return false;

        // Подключаемся к серверу через HTTPS
        HINTERNET hConnect = WinHttpConnect(
            hSession,
            HOST,
            INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return false;
        }

        // Создаём HTTPS запрос
        std::wstring wpath(path.begin(), path.end());
        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect,
            L"POST",
            wpath.c_str(),
            NULL,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        // Заголовки с API ключом
        std::wstring headers =
            L"Content-Type: application/json\r\n"
            L"X-API-Key: ";
        headers += API_KEY;

        // Отправляем запрос
        bool result = WinHttpSendRequest(
            hRequest,
            headers.c_str(), (DWORD)-1,
            (LPVOID)body.c_str(), (DWORD)body.size(),
            (DWORD)body.size(), 0);

        if (result) {
            WinHttpReceiveResponse(hRequest, NULL);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Конвертация данных игрока в JSON
    std::string PlayerToJson(const Fingerprint::PlayerInfo& p) {
        std::ostringstream j;
        j << "{"
            << "\"steamId\":\"" << p.steamId << "\","
            << "\"nick\":\"" << p.username << "\","
            << "\"mac\":\"" << p.macAddress << "\","
            << "\"hwid\":\"" << p.hwid << "\","
            << "\"room\":\"" << "OPEN" << "\","
            << "\"acVersion\":\"" << p.acVersion << "\","
            << "\"launchTime\":" << p.launchTime
            << "}";
        return j.str();
    }

    // Конвертация результата сканирования в JSON
    std::string ScanToJson(const Fingerprint::PlayerInfo& p,
        const Scanner::ScanResult& scan) {
        std::ostringstream j;
        j << "{"
            << "\"hwid\":\"" << p.hwid << "\","
            << "\"room\":\"" << "OPEN" << "\","
            << "\"cheatsFound\":" << (scan.cheatsFound ? "true" : "false") << ","
            << "\"processes\":[";
        for (size_t i = 0; i < scan.foundProcesses.size(); i++) {
            if (i > 0) j << ",";
            j << "\"" << scan.foundProcesses[i] << "\"";
        }
        j << "],"
            << "\"modules\":[";
        for (size_t i = 0; i < scan.foundModules.size(); i++) {
            if (i > 0) j << ",";
            j << "\"" << scan.foundModules[i] << "\"";
        }
        j << "]}";
        return j.str();
    }

    // Отправить отчёт о запуске
    bool SendLaunchReport(const Fingerprint::PlayerInfo& player) {
        return HttpPost("/api/launch", PlayerToJson(player));
    }

    // Отправить результат сканирования
    bool SendScanReport(const Fingerprint::PlayerInfo& player,
        const Scanner::ScanResult& scan) {
        return HttpPost("/api/scan", ScanToJson(player, scan));
    }

    // Отправить пинг (игрок онлайн)
    bool SendHeartbeat(const std::string& hwid) {
        std::string body = "{\"hwid\":\"" + hwid + "\",\"room\":\"OPEN\"}";
        return HttpPost("/api/heartbeat", body);
    }
}