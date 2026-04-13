#include "time_util.h"
#include "wifi_manager.h"
#include <time.h>

// Singapore Standard Time — UTC+8, no DST
static const long  GMT_OFFSET_SEC      = 8 * 3600;
static const int   DAYLIGHT_OFFSET_SEC = 0;
static const char *NTP_PRIMARY         = "pool.ntp.org";
static const char *NTP_SECONDARY       = "time.google.com";

static bool ntpConfigured = false;

void timeInit() {
  if (!wifiIsConnected()) return;
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_PRIMARY, NTP_SECONDARY);
  ntpConfigured = true;

  // Try to block briefly for sync, but don't hang forever.
  Serial.print("[Time] Syncing NTP");
  struct tm t;
  for (int i = 0; i < 10; ++i) {
    if (getLocalTime(&t, 500)) {
      Serial.printf("\n[Time] Synced: %04d-%02d-%02d %02d:%02d:%02d SGT\n",
        t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
        t.tm_hour, t.tm_min, t.tm_sec);
      return;
    }
    Serial.print(".");
  }
  Serial.println("\n[Time] NTP sync pending — will fall back to uptime.");
}

bool timeIsSynced() {
  if (!ntpConfigured) return false;
  struct tm t;
  return getLocalTime(&t, 0);
}

String currentIsoString() {
  struct tm t;
  if (ntpConfigured && getLocalTime(&t, 0)) {
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S SGT", &t);
    return String(buf);
  }
  return "uptime " + String(millis() / 1000.0f, 1) + "s";
}

uint32_t uptimeSeconds() {
  return millis() / 1000UL;
}
