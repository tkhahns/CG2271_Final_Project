#pragma once
#include "config.h"

void initGemini();

// Returns an AI-generated advice string (empty on failure).
// Rate-limited internally to GEMINI_MIN_INTERVAL_MS.
String askGemini(const DeskState &state, const String &question);
String askGeminiForAdvice(const DeskState &state);
String getLastGeminiError();
