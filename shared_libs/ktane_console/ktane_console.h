#pragma once

#include <Arduino.h>

// Default Stream for firmware log / debug output. Most modules use USB CDC Serial.
// Audio console test build sets -DEDGE_AUDIO_CONSOLE and routes logs to Serial1 (e.g. Picoprobe UART).

#if defined(EDGE_AUDIO_CONSOLE)
#define KTANE_CONSOLE_OUT Serial1
#else
#define KTANE_CONSOLE_OUT Serial
#endif
