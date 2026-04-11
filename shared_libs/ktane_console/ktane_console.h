#pragma once

#include <Arduino.h>

// Default Stream for firmware log / debug output. Most modules use USB CDC Serial.
// With -DKTANE_PICOPROBE_UART, logs go to Serial1 on GP0 (TX) / GP1 (RX) — Pico Probe UART
// bridge — so USB CDC stays free for other use.

#if defined(KTANE_PICOPROBE_UART)
#define KTANE_CONSOLE_OUT Serial1
#else
#define KTANE_CONSOLE_OUT Serial
#endif

// Call once from setup() before logging on KTANE_CONSOLE_OUT.
void ktaneConsoleInit(unsigned long baud = 115200);
