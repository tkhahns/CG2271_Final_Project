#pragma once
#include <Arduino.h>

void   timeInit();                  // call once after WiFi connects
bool   timeIsSynced();
String currentIsoString();          // "2026-04-13 14:30:45 SGT" or "uptime 12.3s"
uint32_t uptimeSeconds();
