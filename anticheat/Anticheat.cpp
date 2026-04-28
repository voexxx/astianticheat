#include "fingerprint.h"
#include "scanner.h"
#include "reporter.h"
#include "config.h"
#include <windows.h>
#include <winhttp.h>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

#define IDI_TRAY    1001
#define IDM_SHOW    1002
#define IDM_EXIT    1003
#define WM_TRAY     (WM_USER + 1)
#define WM_ROOMS_LOADED (WM_USER + 2)
#define AUTORUN_KEY L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"
#define APP_NAME    L"AstiAnticheat"

// White theme
#define CLR_BG     RGB(255,255,255)
#define CLR_CARD   RGB(245,246,248)
#define CLR_BORDER RGB(220,221,226)
#define CLR_TEXT   RGB(20, 20, 20 )
#define CLR_MUTED  RGB(100,100,115)
#define CLR_GREEN  RGB(22, 163,74 )
#define CLR_RED    RGB(220,38, 38 )

HBRUSH hBrBg = CreateSolidBrush(CLR_BG);
HBRUSH hBrCard = CreateSolidBrush(CLR_CARD);
HBRUSH hBrInput = CreateSolidBrush(RGB(250, 250, 252));

struct Room {
    std::wstring name;
    std::wstring desc;
    bool isOpen;
    int  online;
    int  maxSlots;
};

std::vector<Room> ROOMS; // загружается с сервера

HWND hWnd, hStatus, hBtn, hLog;
HWND hRoomList, hRoomDesc, hPassLabel, hPassInput, hJoinBtn, hRoomStatus;
NOTIFYICONDATAW nid = {};
HICON  hIcon;
HMENU  hTrayMenu;

bool isRunning = false;
bool isVisible = true;
bool inRoom = false;
int  selectedRoom = -1;
std::thread bgThread;
std::wstring gCurrentRoom = L"OPEN";

std::wstring gNick = L"--";
std::wstring gId = L"--";

// ── AUTORUN ──
void SetAutorun(bool enable) {
    HKEY hKey;
    RegOpenKeyExW(HKEY_CURRENT_USER, AUTORUN_KEY, 0, KEY_SET_VALUE, &hKey);
    if (enable) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        RegSetValueExW(hKey, APP_NAME, 0, REG_SZ,
            (BYTE*)path, (wcslen(path) + 1) * sizeof(wchar_t));
    }
    else {
        RegDeleteValueW(hKey, APP_NAME);
    }
    RegCloseKey(hKey);
}

bool IsAutorunEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, AUTORUN_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;
    bool exists = RegQueryValueExW(hKey, APP_NAME, NULL, NULL, NULL, NULL) == ERROR_SUCCESS;
    RegCloseKey(hKey);
    return exists;
}

// ── TRAY ──
void AddTrayIcon() {
    nid.cbSize = sizeof(nid);
    nid.hWnd = hWnd;
    nid.uID = IDI_TRAY;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = hIcon;
    wcscpy_s(nid.szTip, L"Asti Anti-Cheat");
    Shell_NotifyIconW(NIM_ADD, &nid);
}
void RemoveTrayIcon() { Shell_NotifyIconW(NIM_DELETE, &nid); }
void UpdateTrayTip(const wchar_t* tip) {
    wcscpy_s(nid.szTip, tip);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}
void ShowApp() { ShowWindow(hWnd, SW_SHOW); SetForegroundWindow(hWnd); isVisible = true; }
void HideApp() { ShowWindow(hWnd, SW_HIDE); isVisible = false; }

// ── LOG ──
void AddLog(const std::wstring& msg) {
    SendMessageW(hLog, LB_ADDSTRING, 0, (LPARAM)msg.c_str());
    LRESULT n = SendMessage(hLog, LB_GETCOUNT, 0, 0);
    SendMessage(hLog, LB_SETTOPINDEX, n - 1, 0);
}

// ── HTTP GET (для загрузки комнат) ──
std::string HttpGet(const std::wstring& path) {
    std::string result;

    HINTERNET hSession = WinHttpOpen(L"AstiAnticheat/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return result;

    HINTERNET hConnect = WinHttpConnect(hSession,
        L"astianticheat.onrender.com",
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return result; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect,
        L"GET", path.c_str(), NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpReceiveResponse(hRequest, NULL);
        DWORD size = 0;
        do {
            WinHttpQueryDataAvailable(hRequest, &size);
            if (size == 0) break;
            std::vector<char> buf(size + 1, 0);
            DWORD read = 0;
            WinHttpReadData(hRequest, buf.data(), size, &read);
            result.append(buf.data(), read);
        } while (size > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

// ── ПРОСТОЙ JSON ПАРСЕР ──
// Ищет значение поля в JSON строке
std::string JsonGetString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    size_t end = json.find("\"", pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

bool JsonGetBool(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return false;
    pos += search.size();
    return json.substr(pos, 4) == "true";
}

int JsonGetInt(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return 0;
    pos += search.size();
    return std::stoi(json.substr(pos, 10));
}

// ── ОБНОВИТЬ СПИСОК КОМНАТ В UI ──
void RefreshRoomList() {
    SendMessage(hRoomList, LB_RESETCONTENT, 0, 0);
    for (const auto& r : ROOMS) {
        std::wstring tag = r.isOpen ? L"[open] " : L"[lock] ";
        std::wstring slots = std::to_wstring(r.online) + L"/" + std::to_wstring(r.maxSlots);
        std::wstring item = tag + r.name + L"  " + slots;
        SendMessageW(hRoomList, LB_ADDSTRING, 0, (LPARAM)item.c_str());
    }
}

// ── ЗАГРУЗИТЬ КОМНАТЫ С СЕРВЕРА ──
void LoadRoomsFromServer() {
    AddLog(L"  Loading rooms...");
    std::string json = HttpGet(L"/api/rooms");

    if (json.empty()) {
        AddLog(L"  Server unavailable - using defaults");
        // Дефолтные комнаты если сервер недоступен
        ROOMS = {
            { L"OPEN",    L"Open room - No password", true,  0, 10 },
            { L"ALPHA",   L"Main room",               false, 0, 20 },
            { L"BRAVO",   L"Tournament room",         false, 0, 20 },
            { L"CHARLIE", L"Training room",           false, 0, 20 },
            { L"DELTA",   L"VIP access",              false, 0, 10 },
            { L"ECHO",    L"Closed league",           false, 0, 10 },
        };
        PostMessage(hWnd, WM_ROOMS_LOADED, 0, 0);
        return;
    }

    // Парсим массив JSON объектов
    ROOMS.clear();
    size_t pos = 0;
    while ((pos = json.find("{", pos)) != std::string::npos) {
        size_t end = json.find("}", pos);
        if (end == std::string::npos) break;
        std::string obj = json.substr(pos, end - pos + 1);

        Room r;
        std::string name = JsonGetString(obj, "name");
        std::string desc = JsonGetString(obj, "desc");
        r.name = std::wstring(name.begin(), name.end());
        r.desc = std::wstring(desc.begin(), desc.end());
        r.isOpen = JsonGetBool(obj, "open");
        r.online = JsonGetInt(obj, "online");
        r.maxSlots = JsonGetInt(obj, "maxSlots");

        if (!r.name.empty()) ROOMS.push_back(r);
        pos = end + 1;
    }

    AddLog(L"  Rooms loaded: " + std::to_wstring(ROOMS.size()));
    PostMessage(hWnd, WM_ROOMS_LOADED, 0, 0);
}

// ── BACKGROUND SCAN ──
void BackgroundWork() {
    Fingerprint::PlayerInfo player = Fingerprint::Collect();
    Reporter::SendLaunchReport(player);

    gNick = std::wstring(player.username.begin(), player.username.end());
    gId = std::wstring(player.steamId.begin(), player.steamId.end());

    SetWindowTextW(hStatus,
        (L"Steam: " + gNick + L"     Room: " + gCurrentRoom).c_str());
    UpdateTrayTip((L"Asti AC - " + gNick).c_str());

    while (isRunning) {
        Scanner::ScanResult scan = Scanner::FullScan();
        if (scan.cheatsFound) {
            AddLog(L"  [!] CHEAT DETECTED");
            for (auto& p : scan.foundProcesses)
                AddLog(L"      -> " + std::wstring(p.begin(), p.end()));
            Reporter::SendScanReport(player, scan);
            UpdateTrayTip(L"Asti AC - [!] CHEAT DETECTED");
        }
        else {
            AddLog(L"  [OK] Clean");
        }
        Reporter::SendHeartbeat(player.hwid);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(Config::SCAN_INTERVAL_MS));
    }
}

void StartAC() {
    isRunning = true;
    SetWindowTextW(hBtn, L"Stop");
    AddLog(L"  Started - Room: " + gCurrentRoom);
    InvalidateRect(hWnd, NULL, TRUE);
    bgThread = std::thread(BackgroundWork);
    bgThread.detach();
}

void StopAC() {
    isRunning = false;
    SetWindowTextW(hBtn, L"Start");
    AddLog(L"  Stopped");
    UpdateTrayTip(L"Asti Anti-Cheat");
    InvalidateRect(hWnd, NULL, TRUE);
}

// ── ROOMS ──
void TryJoinRoom() {
    if (selectedRoom < 0 || selectedRoom >= (int)ROOMS.size()) {
        SetWindowTextW(hRoomStatus, L"Select a room from the list");
        return;
    }
    const Room& room = ROOMS[selectedRoom];

    if (room.isOpen) {
        inRoom = true;
        gCurrentRoom = room.name;
        SetWindowTextW(hRoomStatus, (L"Connected: " + room.name).c_str());
        SetWindowTextW(hJoinBtn, L"Leave room");
        EnableWindow(hRoomList, FALSE);
        EnableWindow(hPassInput, FALSE);
        AddLog(L"  Joined: " + room.name);
        InvalidateRect(hWnd, NULL, TRUE);
        return;
    }

    wchar_t buf[128] = {};
    GetWindowTextW(hPassInput, buf, 128);
    if (wcslen(buf) == 0) {
        SetWindowTextW(hRoomStatus, L"Enter password");
        return;
    }

    inRoom = true;
    gCurrentRoom = room.name;
    SetWindowTextW(hRoomStatus, (L"Connected: " + room.name).c_str());
    SetWindowTextW(hJoinBtn, L"Leave room");
    SetWindowTextW(hPassInput, L"");
    EnableWindow(hRoomList, FALSE);
    EnableWindow(hPassInput, FALSE);
    AddLog(L"  Joined: " + room.name);
    InvalidateRect(hWnd, NULL, TRUE);
}

void LeaveRoom() {
    inRoom = false;
    gCurrentRoom = L"OPEN";
    selectedRoom = -1;
    SendMessage(hRoomList, LB_SETCURSEL, -1, 0);
    SetWindowTextW(hRoomStatus, L"");
    SetWindowTextW(hJoinBtn, L"Join");
    SetWindowTextW(hRoomDesc, L"");
    SetWindowTextW(hPassInput, L"");
    ShowWindow(hPassLabel, SW_SHOW);
    ShowWindow(hPassInput, SW_SHOW);
    EnableWindow(hRoomList, TRUE);
    EnableWindow(hPassInput, TRUE);
    AddLog(L"  Left room");
    InvalidateRect(hWnd, NULL, TRUE);
}

// ── DRAW LINE ──
void Line(HDC dc, int x1, int y1, int x2, int y2) {
    HPEN p = CreatePen(PS_SOLID, 1, CLR_BORDER);
    SelectObject(dc, p);
    MoveToEx(dc, x1, y1, NULL); LineTo(dc, x2, y2);
    DeleteObject(p);
}

// ── WINDOW PROC ──
LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_CREATE: {
        HFONT fBig = CreateFontW(22, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, DEFAULT_QUALITY, 0, L"Segoe UI");
        HFONT fMed = CreateFontW(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, DEFAULT_QUALITY, 0, L"Segoe UI");
        HFONT fSmall = CreateFontW(12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, DEFAULT_QUALITY, 0, L"Segoe UI");
        HFONT fBtn = CreateFontW(13, 0, 0, 0, FW_MEDIUM, 0, 0, 0, DEFAULT_CHARSET, 0, 0, DEFAULT_QUALITY, 0, L"Segoe UI");

        HWND hT = CreateWindowW(L"STATIC", L"Asti Anti-Cheat",
            WS_CHILD | WS_VISIBLE, 20, 16, 260, 28, h, NULL, NULL, NULL);
        SendMessage(hT, WM_SETFONT, (WPARAM)fBig, TRUE);

        HWND hAuto = CreateWindowW(L"BUTTON", L"Start with Windows",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            280, 22, 170, 18, h, (HMENU)2, NULL, NULL);
        SendMessage(hAuto, WM_SETFONT, (WPARAM)fSmall, TRUE);
        if (IsAutorunEnabled()) SendMessage(hAuto, BM_SETCHECK, BST_CHECKED, 0);

        HWND hSt = CreateWindowW(L"STATIC", L"Steam: --     Room: --",
            WS_CHILD | WS_VISIBLE, 20, 50, 430, 18, h, NULL, NULL, NULL);
        SendMessage(hSt, WM_SETFONT, (WPARAM)fSmall, TRUE);
        hStatus = hSt;

        HWND hL1 = CreateWindowW(L"STATIC", L"ROOM",
            WS_CHILD | WS_VISIBLE, 20, 78, 60, 14, h, NULL, NULL, NULL);
        SendMessage(hL1, WM_SETFONT, (WPARAM)fSmall, TRUE);

        hRoomList = CreateWindowW(L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            20, 96, 220, 150, h, (HMENU)10, NULL, NULL);
        SendMessage(hRoomList, WM_SETFONT, (WPARAM)fMed, TRUE);
        SendMessageW(hRoomList, LB_ADDSTRING, 0, (LPARAM)L"  Loading...");

        hRoomDesc = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE, 250, 96, 200, 40, h, NULL, NULL, NULL);
        SendMessage(hRoomDesc, WM_SETFONT, (WPARAM)fSmall, TRUE);

        hPassLabel = CreateWindowW(L"STATIC", L"Password:",
            WS_CHILD | WS_VISIBLE, 250, 142, 70, 18, h, NULL, NULL, NULL);
        SendMessage(hPassLabel, WM_SETFONT, (WPARAM)fSmall, TRUE);

        hPassInput = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD,
            326, 139, 124, 22, h, (HMENU)11, NULL, NULL);
        SendMessage(hPassInput, WM_SETFONT, (WPARAM)fSmall, TRUE);

        hJoinBtn = CreateWindowW(L"BUTTON", L"Join",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            250, 170, 200, 30, h, (HMENU)12, NULL, NULL);
        SendMessage(hJoinBtn, WM_SETFONT, (WPARAM)fBtn, TRUE);

        hRoomStatus = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE, 20, 254, 430, 18, h, NULL, NULL, NULL);
        SendMessage(hRoomStatus, WM_SETFONT, (WPARAM)fSmall, TRUE);

        HWND hL2 = CreateWindowW(L"STATIC", L"PROTECTION",
            WS_CHILD | WS_VISIBLE, 20, 282, 100, 14, h, NULL, NULL, NULL);
        SendMessage(hL2, WM_SETFONT, (WPARAM)fSmall, TRUE);

        hBtn = CreateWindowW(L"BUTTON", L"Start",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, 300, 160, 36, h, (HMENU)1, NULL, NULL);
        SendMessage(hBtn, WM_SETFONT, (WPARAM)fBtn, TRUE);

        hLog = CreateWindowW(L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOSEL | WS_BORDER,
            20, 350, 430, 185, h, NULL, NULL, NULL);
        SendMessage(hLog, WM_SETFONT, (WPARAM)fSmall, TRUE);

        HWND hV = CreateWindowW(L"STATIC", L"v1.0  Close = minimize to tray",
            WS_CHILD | WS_VISIBLE | SS_RIGHT, 0, 545, 458, 16, h, NULL, NULL, NULL);
        SendMessage(hV, WM_SETFONT, (WPARAM)fSmall, TRUE);

        // Загружаем комнаты в фоне
        std::thread(LoadRoomsFromServer).detach();
        break;
    }

    case WM_ROOMS_LOADED:
        RefreshRoomList();
        break;

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == 1) { if (!isRunning) StartAC(); else StopAC(); }
        if (id == 2) {
            HWND hA = GetDlgItem(h, 2);
            bool c = SendMessage(hA, BM_GETCHECK, 0, 0) == BST_CHECKED;
            SetAutorun(c);
            AddLog(c ? L"  Autostart enabled" : L"  Autostart disabled");
        }
        if (id == 10 && HIWORD(wp) == LBN_SELCHANGE) {
            selectedRoom = (int)SendMessage(hRoomList, LB_GETCURSEL, 0, 0);
            if (selectedRoom >= 0 && selectedRoom < (int)ROOMS.size()) {
                const Room& r = ROOMS[selectedRoom];
                std::wstring slots = std::to_wstring(r.online) + L"/" + std::to_wstring(r.maxSlots);
                SetWindowTextW(hRoomDesc, (r.name + L"\n" + r.desc + L"\n" + slots + L" online").c_str());
                ShowWindow(hPassLabel, r.isOpen ? SW_HIDE : SW_SHOW);
                ShowWindow(hPassInput, r.isOpen ? SW_HIDE : SW_SHOW);
            }
        }
        if (id == 12) { if (!inRoom) TryJoinRoom(); else LeaveRoom(); }
        break;
    }

    case WM_TRAY: {
        if (lp == WM_LBUTTONDBLCLK) ShowApp();
        if (lp == WM_RBUTTONUP) {
            POINT pt; GetCursorPos(&pt);
            hTrayMenu = CreatePopupMenu();
            AppendMenuW(hTrayMenu, MF_STRING, IDM_SHOW, L"Open");
            AppendMenuW(hTrayMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hTrayMenu, MF_STRING | MF_GRAYED, 0,
                inRoom ? (L"Room: " + gCurrentRoom).c_str() : L"No room selected");
            AppendMenuW(hTrayMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hTrayMenu, MF_STRING, IDM_EXIT, L"Exit");
            SetForegroundWindow(h);
            int cmd = TrackPopupMenu(hTrayMenu,
                TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_RETURNCMD,
                pt.x, pt.y, 0, h, NULL);
            DestroyMenu(hTrayMenu);
            if (cmd == IDM_SHOW) ShowApp();
            if (cmd == IDM_EXIT) {
                isRunning = false;
                RemoveTrayIcon();
                PostQuitMessage(0);
            }
        }
        break;
    }

    case WM_CLOSE:
        HideApp();
        AddLog(L"  Minimized to tray");
        return 0;

    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp; HWND hw = (HWND)lp;
        SetBkMode(dc, TRANSPARENT);
        if (hw == hRoomStatus)
            SetTextColor(dc, inRoom ? CLR_GREEN : CLR_RED);
        else if (hw == hStatus)
            SetTextColor(dc, CLR_MUTED);
        else
            SetTextColor(dc, CLR_TEXT);
        return (LRESULT)hBrBg;
    }
    case WM_CTLCOLORLISTBOX: {
        HDC dc = (HDC)wp;
        SetBkColor(dc, RGB(245, 246, 248));
        SetTextColor(dc, CLR_TEXT);
        return (LRESULT)hBrCard;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetBkColor(dc, RGB(250, 250, 252));
        SetTextColor(dc, CLR_TEXT);
        return (LRESULT)hBrInput;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        FillRect(dc, &rc, hBrBg);
        Line(dc, 20, 70, 450, 70);
        Line(dc, 20, 248, 450, 248);
        Line(dc, 20, 274, 450, 274);
        Line(dc, 20, 342, 450, 342);
        EndPaint(h, &ps);
        break;
    }
    case WM_ERASEBKGND: {
        HDC dc = (HDC)wp; RECT rc;
        GetClientRect(h, &rc);
        FillRect(dc, &rc, hBrBg);
        return 1;
    }
    case WM_DESTROY:
        isRunning = false;
        RemoveTrayIcon();
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

int WINAPI WinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"AstiAnticheatMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"Asti Anti-Cheat is already running.", L"Asti AC",
            MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    hIcon = LoadIcon(hInst, IDI_APPLICATION);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"AstiACClass";
    wc.hbrBackground = hBrBg;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = hIcon;
    RegisterClassW(&wc);

    hWnd = CreateWindowW(L"AstiACClass", L"Asti Anti-Cheat",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 470, 575,
        NULL, NULL, hInst, NULL);

    AddTrayIcon();
    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CloseHandle(hMutex);
    return 0;
}