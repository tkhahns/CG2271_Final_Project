#pragma once
#include "config.h"

enum TgCmd : uint8_t {
  CMD_NONE = 0,
  CMD_START,
  CMD_STATUS,
  CMD_SETTEMP,
  CMD_SETDIST,
  CMD_HELP,
  CMD_ASK,
  CMD_CLEAR
};

struct TgResult {
  bool   received;
  TgCmd  command;
  float  value;       // for SETTEMP / SETDIST
  String text;        // for ASK (free-form question)
};

void initTelegram();
TgResult pollTelegram();
bool sendTelegramMessage(const String &msg);
bool sendTelegramAlert(const DeskState &state);
String formatStatus(const DeskState &state);
