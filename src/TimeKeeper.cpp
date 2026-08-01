#include "TimeKeeper.h"

static bool _synced = false;
static time_t _fallbackEpoch = 0;
static unsigned long _bootMs = 0;

void timeKeeperInit() {
    _bootMs = millis();
    _synced = false;

    // Epoch do relogio padrao (fallback sem internet).
    // mktime e chamado antes do configTime, entao trata o horario como UTC;
    // ajusta para o fuso configurado em TZ_OFFSET_SEC.
    struct tm t = {};
    t.tm_year = DEFAULT_TIME_YEAR - 1900;
    t.tm_mon = DEFAULT_TIME_MONTH - 1;
    t.tm_mday = DEFAULT_TIME_DAY;
    t.tm_hour = DEFAULT_TIME_HOUR;
    t.tm_min = DEFAULT_TIME_MINUTE;
    t.tm_sec = DEFAULT_TIME_SECOND;
    _fallbackEpoch = mktime(&t) - TZ_OFFSET_SEC;
}

bool timeKeeperTrySync(unsigned long timeoutMs) {
    if (_synced) return true;

    configTime(TZ_OFFSET_SEC, 0, NTP_SERVER);
    unsigned long deadline = millis() + timeoutMs;
    while (time(nullptr) < 1600000000 && millis() < deadline) {
        delay(200);
    }
    _synced = (time(nullptr) >= 1600000000);
    return _synced;
}

bool timeIsSynced() {
    return _synced;
}

// Hora atual: real (NTP) se sincronizado, senao fallback + tempo ligado
time_t epochNow() {
    if (_synced) {
        return time(nullptr);
    }
    return _fallbackEpoch + (time_t)((millis() - _bootMs) / 1000);
}
