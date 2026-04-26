#include "fingerprint.h"
#include "scanner.h"
#include "reporter.h"
#include "config.h"
#include <windows.h>
#include <thread>
#include <chrono>
#include <string>
#include <vector>

#define IDI_TRAY     1001
#define IDM_SHOW     1002
#define IDM_EXIT     1003
#define WM_TRAY      (WM_USER + 1)
#define AUTORUN_KEY  L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"
#define APP_NAME     L"AstiAnticheat"

// ── Комнаты (должны совпадать с сайтом) ──
struct Room {
    std::wstring name;
    std::wstring desc;
    std::wstring password;
};

const std::vector<Room> ROOMS = {
    { L"ALPHA",   L"Основная комната",  L"alpha123"   },
    { L"BRAVO",   L"Турнирная комната", L"bravo456"   },
    { L"CHARLIE", L"Тренировочная",     L"charlie789" },
    { L"DELTA",   L"VIP доступ",        L"delta000"   },
    { L"ECHO",    L"Региональная лига", L"echo111"    },
    { L"FOXTROT", L"Закрытая лига",     L"foxtrot222" },
};

// ── Хэндлы ──
HWND hWnd, hStatus, hBtn, hLog;
HWND hRoomList, hRoomDesc, hPassInput, hJoinBtn, hRoomStatus;
NOTIFYICONDATAW nid = {};
HICON  hIcon;
HMENU  hTrayMenu;

bool isRunning = false;
bool isVisible = true;
bool inRoom = false;
int  selectedRoom = -1;
std::thread bgThread;

HBRUSH hBrBg = CreateSolidBrush(RGB(255, 255, 255));
HBRUSH hBrCard = CreateSolidBrush(RGB(245, 246, 248));
HBRUSH hBrInput = CreateSolidBrush(RGB(250, 250, 252));

std::wstring gNick = L"—";
std::wstring gId = L"—";
std::wstring gRoom = L"—";

// ══════════════════════════════
// АВТОЗАПУСК
// ══════════════════════════════
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

// ══════════════════════════════
// ТРЕЙ
// ══════════════════════════════
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

// ══════════════════════════════
// ЛОГ
// ══════════════════════════════
void AddLog(const std::wstring& msg) {
    SendMessageW(hLog, LB_ADDSTRING, 0, (LPARAM)msg.c_str());
    LRESULT n = SendMessage(hLog, LB_GETCOUNT, 0, 0);
    SendMessage(hLog, LB_SETTOPINDEX, n - 1, 0);
}

// ══════════════════════════════
// ФОНОВАЯ РАБОТА
// ══════════════════════════════
void BackgroundWork() {
    Fingerprint::PlayerInfo player = Fingerprint::Collect();
    Reporter::SendLaunchReport(player);

    gNick = std::wstring(player.username.begin(), player.username.end());
    gId = std::wstring(player.steamId.begin(), player.steamId.end());

    SetWindowTextW(hStatus,
        (L"Steam: " + gNick + L"   Комната: " + gRoom).c_str());

    UpdateTrayTip((L"Asti AC · " + gNick + L" · " + gRoom).c_str());

    while (isRunning) {
        Scanner::ScanResult scan = Scanner::FullScan();
        if (scan.cheatsFound) {
            AddLog(L"  ⚠ ЧИТ ОБНАРУЖЕН!");
            for (auto& p : scan.foundProcesses)
                AddLog(L"    → " + std::wstring(p.begin(), p.end()));
            Reporter::SendScanReport(player, scan);
            UpdateTrayTip(L"Asti AC · ⚠ ЧИТ ОБНАРУЖЕН");
        }
        else {
            AddLog(L"  ✓ Сканирование чисто");
        }
        Reporter::SendHeartbeat(player.hwid);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(Config::SCAN_INTERVAL_MS));
    }
}

void StartAC() {
    isRunning = true;
    SetWindowTextW(hBtn, L"Выключить");
    AddLog(L"  Античит запущен · Комната: " + gRoom);
    bgThread = std::thread(BackgroundWork);
    bgThread.detach();
    InvalidateRect(hWnd, NULL, TRUE);
}

void StopAC() {
    isRunning = false;
    SetWindowTextW(hBtn, L"Включить");
    AddLog(L"  Остановлен");
    UpdateTrayTip(L"Asti Anti-Cheat · Выключен");
    InvalidateRect(hWnd, NULL, TRUE);
}

// ══════════════════════════════
// ПРОВЕРКА ПАРОЛЯ КОМНАТЫ
// ══════════════════════════════
void TryJoinRoom() {
    if (selectedRoom < 0) {
        SetWindowTextW(hRoomStatus, L"Выбери комнату из списка");
        return;
    }

    wchar_t buf[128] = {};
    GetWindowTextW(hPassInput, buf, 128);
    std::wstring entered(buf);

    if (entered == ROOMS[selectedRoom].password) {
        inRoom = true;
        gRoom = ROOMS[selectedRoom].name;

        SetWindowTextW(hRoomStatus,
            (L"✓ Подключён: " + gRoom).c_str());
        SetWindowTextW(hJoinBtn, L"Выйти из комнаты");
        SetWindowTextW(hPassInput, L"");

        // Блокируем список и поле после входа
        EnableWindow(hRoomList, FALSE);
        EnableWindow(hPassInput, FALSE);

        AddLog(L"  Комната: " + gRoom + L" · Подключён");
        InvalidateRect(hWnd, NULL, TRUE);
    }
    else {
        SetWindowTextW(hRoomStatus, L"✗ Неверный пароль");
        // Мигание поля ввода
        SetWindowTextW(hPassInput, L"");
    }
}

void LeaveRoom() {
    inRoom = false;
    gRoom = L"—";
    selectedRoom = -1;

    SendMessage(hRoomList, LB_SETCURSEL, -1, 0);
    SetWindowTextW(hRoomStatus, L"");
    SetWindowTextW(hJoinBtn, L"Войти");
    SetWindowTextW(hRoomDesc, L"");
    SetWindowTextW(hPassInput, L"");

    EnableWindow(hRoomList, TRUE);
    EnableWindow(hPassInput, TRUE);

    AddLog(L"  Вышел из комнаты");
    InvalidateRect(hWnd, NULL, TRUE);
}

// ══════════════════════════════
// WINDOW PROC
// ══════════════════════════════
LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_CREATE: {
        HFONT fTitle = CreateFontW(22, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, DEFAULT_QUALITY, 0, L"Segoe UI");
        HFONT fMed = CreateFontW(13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, DEFAULT_QUALITY, 0, L"Segoe UI");
        HFONT fSmall = CreateFontW(12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, DEFAULT_QUALITY, 0, L"Segoe UI");
        HFONT fBtn = CreateFontW(12, 0, 0, 0, FW_MEDIUM, 0, 0, 0, DEFAULT_CHARSET, 0, 0, DEFAULT_QUALITY, 0, L"Segoe UI");

        // ── Заголовок ──
        HWND hT = CreateWindowW(L"STATIC", L"Asti Anti-Cheat",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 16, 260, 28, h, NULL, NULL, NULL);
        SendMessage(hT, WM_SETFONT, (WPARAM)fTitle, TRUE);

        // ── Автозапуск ──
        HWND hAuto = CreateWindowW(L"BUTTON", L"Автозапуск",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            290, 22, 120, 18, h, (HMENU)2, NULL, NULL);
        SendMessage(hAuto, WM_SETFONT, (WPARAM)fSmall, TRUE);
        if (IsAutorunEnabled())
            SendMessage(hAuto, BM_SETCHECK, BST_CHECKED, 0);

        // ── Статус ──
        hStatus = CreateWindowW(L"STATIC", L"Steam: —   Комната: —",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 50, 380, 18, h, NULL, NULL, NULL);
        SendMessage(hStatus, WM_SETFONT, (WPARAM)fSmall, TRUE);

        // ─────────────────────────────
        // СЕКЦИЯ: ВЫБОР КОМНАТЫ
        // ─────────────────────────────
        HWND hSecLabel = CreateWindowW(L"STATIC", L"КОМНАТА",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 76, 100, 14, h, NULL, NULL, NULL);
        SendMessage(hSecLabel, WM_SETFONT, (WPARAM)fSmall, TRUE);

        // Список комнат
        hRoomList = CreateWindowW(L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            20, 94, 180, 130, h, (HMENU)10, NULL, NULL);
        SendMessage(hRoomList, WM_SETFONT, (WPARAM)fSmall, TRUE);

        // Заполняем список
        for (const auto& r : ROOMS) {
            std::wstring item = r.name + L"  ·  " + r.desc;
            SendMessageW(hRoomList, LB_ADDSTRING, 0, (LPARAM)item.c_str());
        }

        // Описание выбранной комнаты
        hRoomDesc = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            210, 94, 190, 40, h, NULL, NULL, NULL);
        SendMessage(hRoomDesc, WM_SETFONT, (WPARAM)fSmall, TRUE);

        // Поле пароля
        HWND hPassLabel = CreateWindowW(L"STATIC", L"Пароль:",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            210, 140, 60, 16, h, NULL, NULL, NULL);
        SendMessage(hPassLabel, WM_SETFONT, (WPARAM)fSmall, TRUE);

        hPassInput = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD,
            275, 137, 120, 20, h, (HMENU)11, NULL, NULL);
        SendMessage(hPassInput, WM_SETFONT, (WPARAM)fSmall, TRUE);

        // Кнопка войти
        hJoinBtn = CreateWindowW(L"BUTTON", L"Войти",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            210, 168, 185, 24, h, (HMENU)12, NULL, NULL);
        SendMessage(hJoinBtn, WM_SETFONT, (WPARAM)fBtn, TRUE);

        // Статус комнаты
        hRoomStatus = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 230, 380, 18, h, NULL, NULL, NULL);
        SendMessage(hRoomStatus, WM_SETFONT, (WPARAM)fSmall, TRUE);

        // ─────────────────────────────
        // СЕКЦИЯ: АНТИЧИТ
        // ─────────────────────────────
        HWND hSecAC = CreateWindowW(L"STATIC", L"ЗАЩИТА",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 254, 100, 14, h, NULL, NULL, NULL);
        SendMessage(hSecAC, WM_SETFONT, (WPARAM)fSmall, TRUE);

        hBtn = CreateWindowW(L"BUTTON", L"Включить",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, 272, 120, 30, h, (HMENU)1, NULL, NULL);
        SendMessage(hBtn, WM_SETFONT, (WPARAM)fBtn, TRUE);

        // Лог
        hLog = CreateWindowW(L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOSEL | WS_BORDER,
            20, 312, 380, 160, h, NULL, NULL, NULL);
        SendMessage(hLog, WM_SETFONT, (WPARAM)fSmall, TRUE);

        // Версия
        HWND hV = CreateWindowW(L"STATIC", L"v1.0 · Закрытие → трей",
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            0, 480, 388, 16, h, NULL, NULL, NULL);
        SendMessage(hV, WM_SETFONT, (WPARAM)fSmall, TRUE);

        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);

        // Вкл/выкл античит
        if (id == 1) {
            if (!isRunning) StartAC(); else StopAC();
        }

        // Автозапуск
        if (id == 2) {
            HWND hAuto = GetDlgItem(h, 2);
            bool checked = SendMessage(hAuto, BM_GETCHECK, 0, 0) == BST_CHECKED;
            SetAutorun(checked);
            AddLog(checked ? L"  Автозапуск включён" : L"  Автозапуск выключен");
        }

        // Выбор комнаты в списке
        if (id == 10 && HIWORD(wp) == LBN_SELCHANGE) {
            selectedRoom = (int)SendMessage(hRoomList, LB_GETCURSEL, 0, 0);
            if (selectedRoom >= 0 && selectedRoom < (int)ROOMS.size()) {
                SetWindowTextW(hRoomDesc,
                    (ROOMS[selectedRoom].name + L"\n" + ROOMS[selectedRoom].desc).c_str());
            }
        }

        // Войти / выйти из комнаты
        if (id == 12) {
            if (!inRoom) TryJoinRoom();
            else LeaveRoom();
        }

        // Enter в поле пароля
        if (id == 11 && HIWORD(wp) == EN_CHANGE) {
            // сбрасываем ошибку при вводе
            SetWindowTextW(hRoomStatus, L"");
        }

        break;
    }

                   // Enter в поле пароля
    case WM_KEYDOWN:
        if (wp == VK_RETURN) TryJoinRoom();
        break;

        // ── ТРЕЙ ──
    case WM_TRAY: {
        if (lp == WM_LBUTTONDBLCLK) ShowApp();
        if (lp == WM_RBUTTONUP) {
            POINT pt; GetCursorPos(&pt);
            hTrayMenu = CreatePopupMenu();
            AppendMenuW(hTrayMenu, MF_STRING, IDM_SHOW, L"Открыть");
            AppendMenuW(hTrayMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hTrayMenu, MF_STRING | MF_GRAYED, 0,
                inRoom ? (L"Комната: " + gRoom).c_str() : L"Комната: не выбрана");
            AppendMenuW(hTrayMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hTrayMenu, MF_STRING, IDM_EXIT, L"Выйти");
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
        AddLog(L"  Свёрнут в трей · защита работает");
        return 0;

    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp; HWND hw = (HWND)lp;
        SetBkMode(dc, TRANSPARENT);
        if (hw == hRoomStatus)
            SetTextColor(dc, inRoom ? RGB(20, 160, 80) : RGB(180, 40, 40));
        else if (hw == hStatus)
            SetTextColor(dc, RGB(90, 90, 100));
        else
            SetTextColor(dc, RGB(20, 20, 20));
        return (LRESULT)hBrBg;
    }
    case WM_CTLCOLORLISTBOX: {
        HDC dc = (HDC)wp;
        SetBkColor(dc, RGB(245, 246, 248));
        SetTextColor(dc, RGB(20, 20, 20));
        return (LRESULT)hBrCard;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetBkColor(dc, RGB(250, 250, 252));
        return (LRESULT)hBrInput;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        FillRect(dc, &rc, hBrBg);

        // Линия под заголовком
        HPEN p1 = CreatePen(PS_SOLID, 1, RGB(220, 221, 226));
        SelectObject(dc, p1);
        MoveToEx(dc, 20, 68, NULL); LineTo(dc, 380, 68);
        // Линия над логом
        MoveToEx(dc, 20, 302, NULL); LineTo(dc, 380, 302);
        // Линия под комнатами
        MoveToEx(dc, 20, 248, NULL); LineTo(dc, 380, 248);
        DeleteObject(p1);

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

// ══════════════════════════════
// MAIN
// ══════════════════════════════
int WINAPI WinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"AstiAnticheatMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"Asti Anti-Cheat уже запущен.", L"Asti AC", MB_OK | MB_ICONINFORMATION);
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
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 530,
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