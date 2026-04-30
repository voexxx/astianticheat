#include "reporter.h"
#include "config.h"
#include <windows.h>
#include <winhttp.h>
#include <sstream>
#include <string>

#pragma comment(lib, "winhttp.lib")

namespace Reporter {

    const wchar_t* API_KEY = L"change_me_in_production";
    const wchar_t* HOST = L"astianticheat.onrender.com";

    bool HttpPost(const std::string& path, const std::string& body) {
        HINTERNET hSession = WinHttpOpen(L"AstiAnticheat/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return false;

        HINTERNET hConnect = WinHttpConnect(hSession, HOST,
            INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

        std::wstring wpath(path.begin(), path.end());
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
            wpath.c_str(), NULL, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        std::wstring headers =
            L"Content-Type: application/json\r\nX-API-Key: ";
        headers += API_KEY;

        bool result = WinHttpSendRequest(hRequest,
            headers.c_str(), (DWORD)-1,
            (LPVOID)body.c_str(), (DWORD)body.size(),
            (DWORD)body.size(), 0);

        if (result) WinHttpReceiveResponse(hRequest, NULL);

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    bool SendLaunchReport(const Fingerprint::PlayerInfo& player, const std::string& room) {
        std::ostringstream j;
        j << "{"
            << "\"steamId\":\"" << player.steamId << "\","
            << "\"nick\":\"" << player.username << "\","
            << "\"mac\":\"" << player.macAddress << "\","
            << "\"hwid\":\"" << player.hwid << "\","
            << "\"room\":\"" << room << "\","
            << "\"acVersion\":\"" << player.acVersion << "\","
            << "\"launchTime\":" << player.launchTime
            << "}";
        return HttpPost("/api/launch", j.str());
    }

    bool SendScanReport(const Fingerprint::PlayerInfo& player,
        const Scanner::ScanResult& scan,
        const std::string& room) {
        std::ostringstream j;
        j << "{"
            << "\"hwid\":\"" << player.hwid << "\","
            << "\"room\":\"" << room << "\","
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
        return HttpPost("/api/scan", j.str());
    }

    bool SendHeartbeat(const std::string& hwid, const std::string& room) {
        std::string body = "{\"hwid\":\"" + hwid + "\",\"room\":\"" + room + "\"}";
        return HttpPost("/api/heartbeat", body);
    }
}