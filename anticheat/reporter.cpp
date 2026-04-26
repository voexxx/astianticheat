#include "reporter.h"
#include "config.h"
#include <windows.h>
#include <winhttp.h>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

namespace Reporter {

    // Отправить HTTP POST запрос
    bool HttpPost(const std::string& path, const std::string& body) {
        HINTERNET hSession = WinHttpOpen(L"Anticheat/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return false;

        HINTERNET hConnect = WinHttpConnect(hSession,
            L"localhost",
            5000, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return false;
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect,
            L"POST",
            std::wstring(path.begin(), path.end()).c_str(),
            NULL, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        std::wstring headers = L"Content-Type: application/json";
        bool result = WinHttpSendRequest(hRequest,
            headers.c_str(), -1,
            (LPVOID)body.c_str(), body.size(),
            body.size(), 0);

        WinHttpReceiveResponse(hRequest, NULL);

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Конвертировать данные игрока в JSON
    std::string PlayerToJson(const Fingerprint::PlayerInfo& p) {
        std::ostringstream json;
        json << "{"
            << "\"hwid\":\"" << p.hwid << "\","
            << "\"mac\":\"" << p.macAddress << "\","
            << "\"pcName\":\"" << p.pcName << "\","
            << "\"username\":\"" << p.username << "\","
            << "\"steamId\":\"" << p.steamId << "\","
            << "\"acVersion\":\"" << p.acVersion << "\","
            << "\"launchTime\":" << p.launchTime
            << "}";
        return json.str();
    }

    // Конвертировать результат сканирования в JSON
    std::string ScanToJson(const Fingerprint::PlayerInfo& p,
        const Scanner::ScanResult& scan) {
        std::ostringstream json;
        json << "{"
            << "\"hwid\":\"" << p.hwid << "\","
            << "\"cheatsFound\":" << (scan.cheatsFound ? "true" : "false") << ","
            << "\"processes\":[";

        for (size_t i = 0; i < scan.foundProcesses.size(); i++) {
            if (i > 0) json << ",";
            json << "\"" << scan.foundProcesses[i] << "\"";
        }
        json << "],"
            << "\"modules\":[";

        for (size_t i = 0; i < scan.foundModules.size(); i++) {
            if (i > 0) json << ",";
            json << "\"" << scan.foundModules[i] << "\"";
        }
        json << "]}";
        return json.str();
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
        return HttpPost("/api/heartbeat", "{\"hwid\":\"" + hwid + "\"}");
    }
}