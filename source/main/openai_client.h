#pragma once
#include "config.h"

void initOpenAI();

// Returns an AI-generated advice string (empty on failure).
// Rate-limited internally to OPENAI_MIN_INTERVAL_MS.
String askOpenAI(const DeskState &state, const String &question);
String askOpenAIForAdvice(const DeskState &state);
