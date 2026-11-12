#pragma once

#include <Arduino.h>

// Default Stream for firmware log / debug output.
// -DKTANE_PICOPROBE_UART → Serial1 on GP0 (TX) / GP1 (RX) (Picoprobe bridge).
// -DKTANE_TEST_DUAL_CONSOLE → USB Serial + UART mirror (harnesses without a probe).
// Otherwise → USB CDC Serial.

#if defined(KTANE_TEST_DUAL_CONSOLE)

class KtaneDualConsole : public Print {
public:
  size_t write(uint8_t c) override;
  void flush() override;
};

extern KtaneDualConsole ktaneDualConsole;
#define KTANE_CONSOLE_OUT ktaneDualConsole

#elif defined(KTANE_PICOPROBE_UART)

#define KTANE_CONSOLE_OUT Serial1

#else

#define KTANE_CONSOLE_OUT Serial

#endif

void ktaneConsoleInit(unsigned long baud = 115200);
