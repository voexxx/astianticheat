#pragma once
#include <string>
#include <vector>

// ==============================
// НАСТРОЙКИ АНТИЧИТА
// ==============================

namespace Config {

    // Адрес твоего сервера (потом поменяем на реальный)
    const std::string SERVER_URL = "http://localhost:5000";

    // Как часто сканировать (в миллисекундах)
    const int SCAN_INTERVAL_MS = 30000;      // каждые 30 секунд
    const int REPORT_INTERVAL_MS = 60000;    // отправка на сервер каждую минуту

    // Версия античита
    const std::string AC_VERSION = "1.0.0";

    // Список известных чит-процессов
    const std::vector<std::string> KNOWN_CHEAT_PROCESSES = {
        // === Cheat Engine ===
        "cheatengine",
        "cheatengine-x86_64",
        "cheatengine-i386",

        // === Отладчики ===
        "x64dbg",
        "x32dbg",
        "ollydbg",
        "windbg",
        "ida",
        "ida64",
        "idaq",
        "idaq64",
        "idaw",
        "idaw64",

        // === Реверс инжиниринг ===
        "ghidra",
        "radare2",
        "dnspy",
        "de4dot",
        "dotpeek",
        "ilspy",

        // === Инжекторы ===
        "injector",
        "extreme injector",
        "xenos",
        "xenos64",
        "manual_map",
        "gdinjector",
        "lilith",

        // === Читы CS2 / CS:GO ===
        "skeet",
        "fatality",
        "neverlose",
        "onetap",
        "gamesense",
        "aimware",
        "interwebz",
        "primordial",
        "hvh.cat",

        // === Общие читы ===
        "aimbot",
        "wallhack",
        "triggerbot",
        "bhop",
        "spinbot",
        "legitbot",
        "ragebot",

        // === Мониторинг процессов ===
        "processhacker",
        "processhacker2",
        "processhacker3",
        "procexp",
        "procexp64",
        "procmon",
        "procmon64",

        // === Перехват трафика ===
        "wireshark",
        "fiddler",
        "charles",
        "mitmproxy",
        "burpsuite",

        // === Автокликеры / Макросы ===
        "autoclicker",
        "autohotkey",
        "autohotkey64",
        "razer synapse",
        "logitech ghub",
        "jitbit",
        "murgaa",

        // === Виртуальные машины (подозрительно) ===
        "vmware",
        "vboxservice",
        "vboxtray",
        "sandboxie",
        "sandman"
    };

    // Список известных чит-модулей (DLL)
    const std::vector<std::string> KNOWN_CHEAT_MODULES = {
        // === Общие ===
        "cheat.dll",
        "hack.dll",
        "aimbot.dll",
        "wallhack.dll",
        "triggerbot.dll",
        "inject.dll",
        "loader.dll",

        // === Известные читы ===
        "skeet.dll",
        "fatality.dll",
        "neverlose.dll",
        "onetap.dll",
        "gamesense.dll",
        "aimware.dll",

        // === Инструменты ===
        "minhook.dll",
        "detours.dll",
        "d3d9hook.dll",
        "d3d11hook.dll",
        "opengl32hook.dll"
    };
}
