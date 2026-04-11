#include "ktane_console.h"

void ktaneConsoleInit(unsigned long baud) {
#if defined(KTANE_PICOPROBE_UART)
  Serial1.setTX(0);
  Serial1.setRX(1);
  Serial1.begin(baud);
#else
  Serial.begin(baud);
#endif
}
