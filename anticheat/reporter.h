#pragma once
#include "fingerprint.h"
#include "scanner.h"
#include <string>

namespace Reporter {

    // Отправить данные о запуске античита
    bool SendLaunchReport(const Fingerprint::PlayerInfo& player);

    // Отправить результат сканирования
    bool SendScanReport(const Fingerprint::PlayerInfo& player,
        const Scanner::ScanResult& scan);

    // Отправить пинг (игрок онлайн)
    bool SendHeartbeat(const std::string& hwid);

    // Конвертировать данные в JSON
    std::string PlayerToJson(const Fingerprint::PlayerInfo& player);
    std::string ScanToJson(const Fingerprint::PlayerInfo& player,
        const Scanner::ScanResult& scan);
}
