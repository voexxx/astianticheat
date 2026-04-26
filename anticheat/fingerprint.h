#pragma once
#include <string>

namespace Fingerprint {

    // Структура с данными об игроке
    struct PlayerInfo {
        std::string hwid;        // Уникальный ID железа
        std::string macAddress;  // MAC адрес
        std::string pcName;      // Имя компьютера
        std::string username;    // Имя пользователя Windows
        std::string steamId;     // Steam ID (потом добавим)
        std::string acVersion;   // Версия античита
        long long   launchTime;  // Время запуска
    };

    // Функции сбора данных
    std::string GetHWID();
    std::string GetMacAddress();
    std::string GetPCName();
    std::string GetUsername();
    long long   GetCurrentTime();

    // Собрать всё вместе
    PlayerInfo Collect();
}
