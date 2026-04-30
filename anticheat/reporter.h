#pragma once
#include "fingerprint.h"
#include "scanner.h"
#include <string>

namespace Reporter {
    bool SendLaunchReport(const Fingerprint::PlayerInfo& player, const std::string& room);
    bool SendScanReport(const Fingerprint::PlayerInfo& player, const Scanner::ScanResult& scan, const std::string& room);
    bool SendHeartbeat(const std::string& hwid, const std::string& room);
}