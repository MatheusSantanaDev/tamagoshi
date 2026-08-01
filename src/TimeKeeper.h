#ifndef TIMEKEEPER_H
#define TIMEKEEPER_H

#include <Arduino.h>
#include <time.h>
#include "config.h"

// Relogio com fallback: sem internet usa a data padrao (DEFAULT_TIME_*)
// e conta o tempo por uptime; com internet sincroniza via NTP.

void timeKeeperInit();
bool timeKeeperTrySync(unsigned long timeoutMs);
bool timeIsSynced();
time_t epochNow();

#endif
