#include "ktane_console.h"

#if defined(KTANE_TEST_DUAL_CONSOLE)

size_t KtaneDualConsole::write(uint8_t c) {
  Serial.write(c);
  Serial1.write(c);
  return 1;
}

void KtaneDualConsole::flush() {
  Serial.flush();
  Serial1.flush();
}

KtaneDualConsole ktaneDualConsole;

#endif

void ktaneConsoleInit(unsigned long baud) {
#if defined(KTANE_PICOPROBE_UART)
  Serial1.setTX(0);
  Serial1.setRX(1);
  Serial1.begin(baud);
#elif defined(KTANE_TEST_DUAL_CONSOLE)
  Serial.begin(baud);
  Serial1.setTX(0);
  Serial1.setRX(1);
  Serial1.begin(baud);
  for (unsigned i = 0; i < 30 && !Serial; i++) {
    delay(100);
  }
  Serial.println(F("--- KTANE console (USB+UART GP0) ready ---"));
  Serial1.println(F("--- KTANE console (USB+UART GP0) ready ---"));
  Serial.flush();
#else
  Serial.begin(baud);
  for (unsigned i = 0; i < 30 && !Serial; i++) {
    delay(100);
  }
  Serial.println(F("--- KTANE console (USB) ready ---"));
  Serial.flush();
#endif
}
