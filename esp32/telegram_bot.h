#ifndef TELEGRAM_BOT_H
#define TELEGRAM_BOT_H

#include "uart_handler.h"

// Enum for the type of command received from Telegram
typedef enum {
    CMD_NONE,
    CMD_START,
    CMD_STATUS,
    CMD_SETTEMP,
    CMD_SETDIST,
    CMD_HELP,
    CMD_UNKNOWN
} TelegramCmd_t;

// Result of polling Telegram
typedef struct {
    TelegramCmd_t command;
    float         value;       // Parsed value for /settemp, /setdist
    bool          received;    // True if any command was received this poll
} TelegramResult_t;

void             initTelegram();
TelegramResult_t pollTelegram(const SensorData_t& data, float& tempThreshold, float& distThreshold);
bool             sendTelegramMessage(const String& text);
bool             sendTelegramAlert(const SensorData_t& data, const String& advisory);

#endif
